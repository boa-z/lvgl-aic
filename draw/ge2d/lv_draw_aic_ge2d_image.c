/**
 * @file lv_draw_aic_ge2d_image.c
 * @brief RGB copy/scale through the ArtInChip GE2D engine.
 *
 * Supports unrotated copies, bounded RGB scaling with explicit inverse-map
 * phases, arbitrary-angle rotation, and bounded multi-pass scaled rotation.
 * Unsupported geometry falls back before submission; a hardware failure never
 * retries blending on a potentially partly written target.
 *
 * Source alpha is the one thing that is not "the simple case" and is handled
 * anyway: an ARGB8888 source is blended with its own per-pixel alpha, and a
 * source without an alpha channel is blended by global alpha when the
 * descriptor carries a partial opacity. Both go through the same Porter/Duff
 * rule; see the long note on the rule in lv_draw_aic_ge2d_blit().
 *
 * Both IMAGE and LAYER tasks arrive here. A LAYER task carries an lv_layer_t in
 * place of the source, so its draw buffer is wrapped in a temporary image
 * descriptor and the identical blit runs - there is no second code path to keep
 * in step, and no separate "blend a layer" implementation to drift.
 *
 * Normal images use LVGL's helper. Tiles hold one LVGL decoder
 * across validation and submission passes so unsupported later tiles cannot
 * cause software replay over pixels already blended by GE.
 *
 * Three things here are easy to get wrong and are deliberately explicit:
 *   1. Both strides come from the buffer headers. Neither side is necessarily
 *      width * bpp.
 *   2. If the decoded source turns out to be unusable - unsupported format, or
 *      an address outside the GE window - the task is handed to the software
 *      renderer. It is never reported as FINISHED-with-nothing-drawn, and the
 *      image is never dropped.
 *   3. A layer with no buffer is not an error. LVGL never allocates the buffer
 *      of a layer nothing was drawn on, so the task completes having drawn
 *      nothing, which is exactly what lv_draw_sw_layer() does.
 *
 * The executor also reports WHICH of those happened, through the outcome
 * out-parameter, because "the unit claimed it" and "the engine drew it" are not
 * the same statement and only the executor can tell them apart. See the note on
 * the sw_fallback counters in lv_draw_aic_ge2d.h.
 *
 * Default draw buffers can use the application's bounded CMA allocation
 * wrapper. Ordinary fallback heap buffers outside the GE address window still
 * composite in software; the address gate remains authoritative.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#define AIC_LVGL_USE_PRIVATE_API 1
#include "lv_draw_aic_ge2d.h"
#include "lv_aic_rgb_image_private.h"
#include "lv_draw_aic_ge2d_utils.h"
#include "lv_draw_aic_ge2d_yuv.h"
#include "lv_aic_fake_image.h"
#include "lv_aic_pixel_format.h"
#include "lv_draw_aic_ge2d_scale.h"
#include "lv_draw_aic_ge2d_rotate.h"
#include "lv_draw_aic_ge2d_transform.h"
#include "lv_draw_aic_ge2d_stripes.h"

#if AIC_LVGL_USE_GE2D && AIC_LVGL_BSP_MPP

#include "lvgl_aic_private.h"

#include <aic_core.h>
#include <aic_osal.h>
#include <mpp_ge.h>
#include <limits.h>

/* Synchronous draw unit: one full-image decoder and at most one task in
 * flight. A failed DMA keeps this descriptor and its owned resources alive;
 * subsequent public calls refuse work until reboot. */
static bool s_blit_called;
static bool s_blit_ok;
static bool s_blit_failed;
static lv_aic_rgb_image_t *rgb_quarantined;
/* Stable address: decoders may retain pointers into their descriptor. */
static lv_image_decoder_dsc_t image_decoder;
static bool decoder_quarantined;
bool lv_draw_aic_ge2d_image_faulted(void)
{
    return rgb_quarantined != NULL || decoder_quarantined;
}
static bool s_blit_validate_only;

/* One synchronous operation owns both scratch buffers. On uncertain DMA the
 * decoder and destination are already quarantined; retain these buffers too. */
static lv_aic_ge2d_transform_plan_t transform_plan;
static lv_draw_buf_t transform_padded, transform_scaled;
static bool transform_active;
#ifndef AIC_LVGL_GE2D_TRANSFORM_BYTES
#define AIC_LVGL_GE2D_TRANSFORM_BYTES (2U * 1024U * 1024U)
#endif

static void transform_release(void)
{
    if(transform_padded.data) aicos_free_align(MEM_CMA,transform_padded.data);
    if(transform_scaled.data) aicos_free_align(MEM_CMA,transform_scaled.data);
    lv_memzero(&transform_padded,sizeof transform_padded);
    lv_memzero(&transform_scaled,sizeof transform_scaled);
}

static bool transform_buffer(lv_draw_buf_t *buf, uint32_t w, uint32_t h,
                             uint32_t stride, uint32_t bytes)
{
    void *data = aicos_malloc_align(MEM_CMA,bytes,64);
    if(!data) return false;
    if(((uintptr_t)data & 63U) ||
       lv_draw_buf_init(buf,w,h,LV_COLOR_FORMAT_ARGB8888,stride,data,bytes) != LV_RESULT_OK ||
       !lv_draw_aic_ge2d_buf_address_valid(buf)) {
        aicos_free_align(MEM_CMA,data); lv_memzero(buf,sizeof *buf); return false;
    }
    buf->header.flags |= LV_IMAGE_FLAGS_PREMULTIPLIED;
    return true;
}

static struct mpp_buf transform_mpp(const lv_draw_buf_t *buf, enum mpp_pixel_format cf)
{
    struct mpp_buf m = {0};
    m.buf_type = MPP_PHY_ADDR; m.phy_addr[0] = (uint32_t)(uintptr_t)buf->data;
    m.stride[0] = buf->header.stride; m.size.width = buf->header.w; m.size.height = buf->header.h;
    m.format = cf;
    return m;
}

static bool transform_submit(struct mpp_ge *ge, struct ge_bitblt *blt)
{
    int result=lv_aic_ge2d_stripe_run(ge,blt);
    if(result==0) return false;
    if(result<0) {
        s_blit_failed = true;
        LV_LOG_ERROR("GE2D transform preparation failed; retaining scratch DMA buffers");
        return false;
    }
    return true;
}


static int lv_draw_aic_ge2d_angle_2_12(int32_t angle, bool cosine)
{
    /* LVGL angles have tenths-of-degree precision. Interpolate the native
     * degree table before converting Q15 to GE Q12; do not round the angle. */
    int32_t degrees = angle / 10 + (cosine ? 90 : 0);
    int32_t remainder = angle % 10;
    int32_t value = lv_trigo_sin((int16_t)degrees) * (10 - remainder) +
                    lv_trigo_sin((int16_t)(degrees + 1)) * remainder;

    return value / 80;
}

static bool image_is_premultiplied(const lv_draw_buf_t *src)
{
    return src->header.cf == LV_COLOR_FORMAT_ARGB8888_PREMULTIPLIED ||
           (src->header.cf == LV_COLOR_FORMAT_ARGB8888 &&
            (src->header.flags & LV_IMAGE_FLAGS_PREMULTIPLIED));
}

static void image_source_alpha(struct ge_ctrl *ctrl, lv_color_format_t cf,
                               lv_opa_t opa, bool premultiplied)
{
    if (opa >= LV_OPA_COVER && cf != LV_COLOR_FORMAT_ARGB8888) {
        ctrl->alpha_en = 0U;
        ctrl->src_alpha_mode = 0U;
    }
    else {
        ctrl->alpha_en = 1U;
        ctrl->alpha_rules = GE_PD_NONE;
        /* SDK rewrites the full-opacity premultiplied path to (1, 1-As).
         * Mixed opacity uses its depremultiply stage plus (As, 1-As). */
        ctrl->src_alpha_mode = premultiplied && opa >= LV_OPA_COVER ? 0U : 2U;
        ctrl->src_global_alpha = opa;
    }
}

/* Copy/convert to padded premultiplied pixels, then scale their stored
 * channels without another alpha operation. Apply global opacity only at the
 * final rotation. No CPU access to either buffer follows a DMA write. */
static bool transform_prepare(const lv_draw_buf_t *src)
{
    const lv_aic_ge2d_transform_plan_t *p = &transform_plan;
    enum mpp_pixel_format fmt;
    lv_color_format_t cf = src->header.cf == LV_COLOR_FORMAT_ARGB8888_PREMULTIPLIED ?
                          LV_COLOR_FORMAT_ARGB8888 : src->header.cf;
    struct mpp_ge *ge = lv_draw_aic_ge2d_device();
    if(!ge || !lv_aic_pixel_format_to_mpp(cf,&fmt)) return false;
    if(!transform_buffer(&transform_padded,p->padded_w,p->padded_h,p->padded_stride,p->padded_bytes) ||
       !transform_buffer(&transform_scaled,p->scaled_w,p->scaled_h,p->scaled_stride,p->scaled_bytes)) return false;
    lv_memzero(transform_padded.data,p->padded_bytes);
    /* Explicit output padding remains transparent even for strong downscale,
     * where two input border pixels occupy less than one scaled pixel. */
    lv_memzero(transform_scaled.data,p->scaled_bytes);
    lv_area_t source = {0,0,(int32_t)src->header.w-1,(int32_t)src->header.h-1};
    lv_area_t padded = {0,0,p->padded_w-1,p->padded_h-1};
    lv_area_t scaled = {0,0,p->scaled_w-1,p->scaled_h-1};
    struct ge_bitblt copy = {0};
    copy.src_buf = transform_mpp(src,fmt);
    copy.dst_buf = transform_mpp(&transform_padded,MPP_FMT_ARGB_8888);
    copy.dst_buf.crop_en = 1;
    copy.dst_buf.crop = (struct mpp_rect){2,2,src->header.w,src->header.h};
    if(!image_is_premultiplied(src)) {
        copy.dst_buf.flags = MPP_BUF_IS_PREMULTIPLY;
        copy.ctrl.alpha_en = 1; copy.ctrl.alpha_rules = GE_PD_SRC;
        copy.ctrl.src_alpha_mode = 0; copy.ctrl.src_global_alpha = 255;
    }
    struct ge_bitblt scale = {0};
    scale.src_buf = transform_mpp(&transform_padded,MPP_FMT_ARGB_8888);
    scale.src_buf.crop_en = 1;
    scale.src_buf.crop = (struct mpp_rect){p->crop_x,p->crop_y,p->padded_w-p->crop_x,p->padded_h-p->crop_y};
    scale.dst_buf = transform_mpp(&transform_scaled,MPP_FMT_ARGB_8888);
    scale.dst_buf.crop_en = 1;
    scale.dst_buf.crop = (struct mpp_rect){2,2,p->scaled_w-4,p->scaled_h-4};
    scale.scale_phase.scale_phase_en = 1; scale.scale_phase.scaler_en = 1;
    scale.scale_phase.channel_num = 1;
    scale.scale_phase.dx_16[0] = p->step_x; scale.scale_phase.dy_16[0] = p->step_y;
    scale.scale_phase.h_phase_16[0] = p->phase_x; scale.scale_phase.v_phase_16[0] = p->phase_y;
    /* Validate ALL scale strips before even the preparation copy. */
    unsigned commands;
    if(!lv_aic_ge2d_stripe_count(&copy,&commands) || !lv_aic_ge2d_stripe_count(&scale,&commands)) return false;
    /* Existing premultiplied storage can be copied bit-for-bit. */
    lv_draw_aic_ge2d_prepare_src_cache(src,&source);
    lv_draw_aic_ge2d_prepare_dst_cache(&transform_padded,&padded);
    if(!transform_submit(ge,&copy)) return false;
    lv_draw_aic_ge2d_prepare_src_cache(&transform_padded,&padded);
    lv_draw_aic_ge2d_prepare_dst_cache(&transform_scaled,&scaled);
    return transform_submit(ge,&scale);
}

static bool lv_draw_aic_ge2d_rotate(const lv_layer_t *layer,
                                    const lv_draw_buf_t *src,
                                    const lv_draw_buf_t *dst,
                                    lv_color_format_t src_cf,
                                    enum mpp_pixel_format src_fmt,
                                    enum mpp_pixel_format dst_fmt,
                                    const lv_draw_image_dsc_t *draw_dsc,
                                    const lv_area_t *img_coords,
                                    const lv_area_t *dst_area_abs)
{
    struct ge_rotation rot = { 0 };
    lv_area_t src_area = { 0, 0, (int32_t)src->header.w - 1,
                           (int32_t)src->header.h - 1 };
    lv_area_t dst_area = {
        dst_area_abs->x1 - layer->buf_area.x1,
        dst_area_abs->y1 - layer->buf_area.y1,
        dst_area_abs->x2 - layer->buf_area.x1,
        dst_area_abs->y2 - layer->buf_area.y1,
    };
    struct mpp_ge *ge;
    int32_t angle;

    /* SDK check_format_and_size rejects either rectangle outside 4..4096.
     * Decline before cache/submission so narrow clips can render in software. */
    if (src->header.w < 4 || src->header.h < 4 ||
        src->header.w > 4096 || src->header.h > 4096 ||
        dst_area.x1 < 0 || dst_area.y1 < 0 ||
        dst_area.x2 >= (int32_t)dst->header.w ||
        dst_area.y2 >= (int32_t)dst->header.h ||
        lv_area_get_width(&dst_area) < 4 || lv_area_get_height(&dst_area) < 4 ||
        lv_area_get_width(&dst_area) > 4096 || lv_area_get_height(&dst_area) > 4096) {
        return false;
    }

    lv_point_t destination_center;
    if (!lv_aic_ge2d_rotation_center(&draw_dsc->pivot, img_coords,
                                     dst_area_abs, &destination_center)) {
        return false;
    }

    if (s_blit_validate_only) return true;
    lv_draw_aic_ge2d_prepare_src_cache(src, &src_area);
    lv_draw_aic_ge2d_prepare_dst_cache(dst, &dst_area);

    rot.src_buf.buf_type = MPP_PHY_ADDR;
    rot.src_buf.phy_addr[0] = (uint32_t)(ulong)src->data;
    rot.src_buf.stride[0] = src->header.stride;
    rot.src_buf.size.width = src->header.w;
    rot.src_buf.size.height = src->header.h;
    rot.src_buf.format = src_fmt;
    if (image_is_premultiplied(src)) rot.src_buf.flags |= MPP_BUF_IS_PREMULTIPLY;

    rot.src_rot_center.x = draw_dsc->pivot.x;
    rot.src_rot_center.y = draw_dsc->pivot.y;

    rot.dst_buf.buf_type = MPP_PHY_ADDR;
    rot.dst_buf.phy_addr[0] = (uint32_t)(ulong)dst->data;
    rot.dst_buf.stride[0] = dst->header.stride;
    rot.dst_buf.size.width = dst->header.w;
    rot.dst_buf.size.height = dst->header.h;
    rot.dst_buf.format = dst_fmt;
    rot.dst_buf.crop_en = 1U;
    rot.dst_buf.crop.x = dst_area.x1;
    rot.dst_buf.crop.y = dst_area.y1;
    rot.dst_buf.crop.width = (uint32_t)lv_area_get_width(&dst_area);
    rot.dst_buf.crop.height = (uint32_t)lv_area_get_height(&dst_area);

    /* GE rotation centers are relative to the cropped destination area. */
    rot.dst_rot_center.x = destination_center.x;
    rot.dst_rot_center.y = destination_center.y;

    angle = draw_dsc->rotation % 3600;
    if (angle < 0) angle += 3600;
    rot.angle_sin = lv_draw_aic_ge2d_angle_2_12(angle, false);
    rot.angle_cos = lv_draw_aic_ge2d_angle_2_12(angle, true);

    image_source_alpha(&rot.ctrl, src_cf, draw_dsc->opa, image_is_premultiplied(src));

    ge = lv_draw_aic_ge2d_device();
    if (ge == NULL) {
        LV_LOG_ERROR("GE2D device is not open");
        return false;
    }

    if (mpp_ge_rotate(ge, &rot) < 0) {
        s_blit_failed = true;
        LV_LOG_ERROR("GE2D rotate failed");
        return false;
    }
    if (mpp_ge_emit(ge) < 0) {
        s_blit_failed = true;
        LV_LOG_ERROR("GE2D emit failed");
        return false;
    }
    if (mpp_ge_sync(ge) < 0) {
        s_blit_failed = true;
        LV_LOG_ERROR("GE2D sync failed");
        return false;
    }

    return true;
}

/**
 * Build the GE2D blit descriptor and run it.
 *
 * Returns true when the engine completed the copy. A false return requests
 * fallback unless s_blit_failed records a real hardware failure.
 */
static bool lv_draw_aic_ge2d_blit(lv_draw_task_t *task,
                                  const lv_draw_image_dsc_t *draw_dsc,
                                  const lv_image_decoder_dsc_t *decoder_dsc,
                                  const lv_area_t *img_coords,
                                  const lv_area_t *clipped_img_area)
{
    lv_layer_t *layer = task->target_layer;
    const lv_draw_buf_t *src;
    const lv_draw_buf_t *dst;
    lv_area_t src_area;
    lv_area_t dst_area;
    struct ge_bitblt blt = { 0 };
    struct mpp_ge *ge;
    lv_color_format_t src_cf;
    enum mpp_pixel_format src_fmt;
    enum mpp_pixel_format dst_fmt;
    int32_t blit_w;
    int32_t blit_h;
    int64_t rx, ry, rw, rh;
    bool scaled = draw_dsc->scale_x != LV_SCALE_NONE || draw_dsc->scale_y != LV_SCALE_NONE;
    unsigned rotation_flags = 0;
    int32_t rotation_phase_x = 0, rotation_phase_y = 0;

    if (layer == NULL || layer->draw_buf == NULL) {
        return false;
    }

    /* decoder_dsc->decoded is the whole image: the MPP decoder never hands out
     * a partial buffer, so the helper always takes the "available" branch. */
    src = decoder_dsc->decoded;
    if (src == NULL || src->data == NULL) {
        return false;
    }
    dst = layer->draw_buf;

    /* Acceptance already checked the destination, but the source format is
     * only known after the decode, so it has to be re-checked here. */
    src_cf = (lv_color_format_t)src->header.cf;
    /* The MPP layout enum describes channel storage; the separate buffer flag
     * carries premultiplication. Do not broaden unrelated format consumers. */
    if (src_cf == LV_COLOR_FORMAT_ARGB8888_PREMULTIPLIED) src_cf = LV_COLOR_FORMAT_ARGB8888;
    if (dst->header.flags & LV_IMAGE_FLAGS_PREMULTIPLIED) return false;
    if (draw_dsc->colorkey != NULL &&
        (image_is_premultiplied(src) || src_cf == LV_COLOR_FORMAT_RGB565 ||
         (src_cf == LV_COLOR_FORMAT_ARGB8888 && draw_dsc->antialias))) {
        /* RGB565 expansion and premultiplied antialias filtering can change
         * the comparison space. Preserve LVGL's key semantics in software. */
        return false;
    }
    if (!lv_aic_pixel_format_is_ge2d_src(src_cf)) {
        LV_LOG_WARN("GE2D cannot read source format %d; falling back", (int)src_cf);
        return false;
    }
    if (!lv_aic_pixel_format_is_ge2d_dst((lv_color_format_t)dst->header.cf)) {
        return false;
    }
    if (!lv_aic_pixel_format_to_mpp(src_cf, &src_fmt)) {
        return false;
    }
    if (!lv_aic_pixel_format_to_mpp((lv_color_format_t)dst->header.cf, &dst_fmt)) {
        return false;
    }

    /* The GE block reaches memory through a fixed window. A heap source below
     * it cannot be read, and no amount of retrying will change that. */
    if (!lv_draw_aic_ge2d_buf_address_valid(src)) {
        LV_LOG_WARN("GE2D cannot address source %p; falling back", (void *)src->data);
        return false;
    }
    if (!lv_draw_aic_ge2d_buf_address_valid(dst)) {
        return false;
    }

    /* LVGL 9.6 has intersected the transformed area and task clip. Intersect
     * actual layer storage before inverse mapping; never drop an edge pixel. */
    if (!lv_area_intersect(&dst_area, clipped_img_area, &layer->buf_area)) {
        return false;
    }
    /* The bounded scratch path separates scale and rotation. Destination
     * centers keep the ORIGINAL pivot; the source center belongs to the
     * resampled integer grid. Reject translations before any target write. */
    if (scaled && draw_dsc->rotation % 900 != 0) {
        if(!transform_active) return false;
        int64_t x = (int64_t)img_coords->x1 + draw_dsc->pivot.x - transform_plan.pivot.x;
        int64_t y = (int64_t)img_coords->y1 + draw_dsc->pivot.y - transform_plan.pivot.y;
        if(x < INT32_MIN || y < INT32_MIN || x > INT32_MAX || y > INT32_MAX) return false;
        lv_area_t origin = {(int32_t)x,(int32_t)y,(int32_t)x,(int32_t)y};
        lv_draw_image_dsc_t rotate = *draw_dsc;
        rotate.pivot = transform_plan.pivot; rotate.scale_x = rotate.scale_y = LV_SCALE_NONE;
        return lv_draw_aic_ge2d_rotate(layer,&transform_scaled,dst,LV_COLOR_FORMAT_ARGB8888,
                                       MPP_FMT_ARGB_8888,dst_fmt,&rotate,&origin,&dst_area);
    }
    if (!scaled && draw_dsc->rotation % 900 != 0) {
        return lv_draw_aic_ge2d_rotate(layer, src, dst, src_cf, src_fmt, dst_fmt,
                                       draw_dsc, img_coords, &dst_area);
    }
    rx = (int64_t)dst_area.x1 - img_coords->x1;
    ry = (int64_t)dst_area.y1 - img_coords->y1;
    rw = (int64_t)dst_area.x2 - dst_area.x1 + 1;
    rh = (int64_t)dst_area.y2 - dst_area.y1 + 1;
    if (rx < INT32_MIN || ry < INT32_MIN || rw < 1 || rh < 1 ||
        rw > INT32_MAX || rh > INT32_MAX ||
        rx + rw - 1 > INT32_MAX || ry + rh - 1 > INT32_MAX) return false;
    src_area = (lv_area_t){(int32_t)rx, (int32_t)ry,
                           (int32_t)(rx+rw-1), (int32_t)(ry+rh-1)};
    if (!scaled && draw_dsc->rotation != 0) {
        if (!lv_aic_ge2d_rotation_crop(src->header.w, src->header.h,
                                        &src_area, &draw_dsc->pivot,
                                        draw_dsc->rotation, &src_area,
                                        &rotation_flags)) {
            return false;
        }
    }
    if (scaled && draw_dsc->rotation != 0) {
        if (!lv_aic_ge2d_rotation_scale_crop(src->header.w, src->header.h,
                                              &src_area, &draw_dsc->pivot,
                                              draw_dsc->rotation,
                                              draw_dsc->scale_x, draw_dsc->scale_y,
                                              &src_area, &rotation_flags,
                                              &rotation_phase_x, &rotation_phase_y)) {
            return false;
        }
    }
    if (scaled) {
        lv_aic_ge2d_scale_axis_t x, y;
        uint32_t sx = draw_dsc->scale_x;
        uint32_t sy = draw_dsc->scale_y;
        if (draw_dsc->skew_x != 0 || draw_dsc->skew_y != 0 ||
            (draw_dsc->rotation == 0 &&
             (!lv_aic_ge2d_scale_axis(src->header.w, src_area.x1, lv_area_get_width(&dst_area),
                                      draw_dsc->pivot.x, draw_dsc->scale_x, &x) ||
              !lv_aic_ge2d_scale_axis(src->header.h, src_area.y1, lv_area_get_height(&dst_area),
                                      draw_dsc->pivot.y, draw_dsc->scale_y, &y)))) {
            return false;
        }
        if (draw_dsc->rotation == 0) {
            src_area.x1 = x.crop; src_area.x2 = x.crop + x.extent - 1;
            src_area.y1 = y.crop; src_area.y2 = y.crop + y.extent - 1;
        } else {
            x.step_16 = (int32_t)(16777216 / sx);
            y.step_16 = (int32_t)(16777216 / sy);
            x.crop = src_area.x1; x.extent = lv_area_get_width(&src_area);
            y.crop = src_area.y1; y.extent = lv_area_get_height(&src_area);
        }
        blt.scale_phase.scale_phase_en = 1;
        blt.scale_phase.scaler_en = 1;
        blt.scale_phase.channel_num = 1;
        blt.scale_phase.dx_16[0] = x.step_16;
        blt.scale_phase.dy_16[0] = y.step_16;
        blt.scale_phase.h_phase_16[0] = draw_dsc->rotation ? rotation_phase_x : x.phase_16;
        blt.scale_phase.v_phase_16[0] = draw_dsc->rotation ? rotation_phase_y : y.phase_16;
    }
    rx = (int64_t)dst_area.x1 - layer->buf_area.x1;
    ry = (int64_t)dst_area.y1 - layer->buf_area.y1;
    if (rx < 0 || ry < 0 || rx+rw > dst->header.w || ry+rh > dst->header.h) return false;
    dst_area = (lv_area_t){(int32_t)rx, (int32_t)ry,
                           (int32_t)(rx+rw-1), (int32_t)(ry+rh-1)};

    blit_w = lv_area_get_width(&src_area);
    blit_h = lv_area_get_height(&src_area);
    if (blit_w <= 0 || blit_h <= 0) {
        /* Fully clipped: nothing to draw, and that is not a failure. */
        return true;
    }
    if (src_area.x1 < 0 || src_area.y1 < 0 ||
        src_area.x2 >= (int32_t)src->header.w || src_area.y2 >= (int32_t)src->header.h ||
        dst_area.x1 < 0 || dst_area.y1 < 0 ||
        dst_area.x2 >= (int32_t)dst->header.w || dst_area.y2 >= (int32_t)dst->header.h) {
        return false;
    }

    blt.src_buf.buf_type = MPP_PHY_ADDR;
    blt.src_buf.phy_addr[0] = (uint32_t)(ulong)src->data;
    blt.src_buf.stride[0] = src->header.stride;
    /* size describes the whole buffer; crop selects the rectangle to copy.
     * Using the header dimensions - not the crop extent - guarantees the crop
     * is always inside the declared buffer. */
    blt.src_buf.size.width = src->header.w;
    blt.src_buf.size.height = src->header.h;
    blt.src_buf.format = src_fmt;
    if (image_is_premultiplied(src)) blt.src_buf.flags |= MPP_BUF_IS_PREMULTIPLY;
    blt.src_buf.crop_en = 1U;
    blt.src_buf.crop.x = src_area.x1;
    blt.src_buf.crop.y = src_area.y1;
    blt.src_buf.crop.width = (uint32_t)blit_w;
    blt.src_buf.crop.height = (uint32_t)blit_h;

    blt.dst_buf.buf_type = MPP_PHY_ADDR;
    blt.dst_buf.phy_addr[0] = (uint32_t)(ulong)dst->data;
    blt.dst_buf.stride[0] = dst->header.stride;
    blt.dst_buf.size.width = dst->header.w;
    blt.dst_buf.size.height = dst->header.h;
    blt.dst_buf.format = dst_fmt;
    blt.dst_buf.crop_en = 1U;
    blt.dst_buf.crop.x = dst_area.x1;
    blt.dst_buf.crop.y = dst_area.y1;
    blt.dst_buf.crop.width = (uint32_t)lv_area_get_width(&dst_area);
    blt.dst_buf.crop.height = (uint32_t)lv_area_get_height(&dst_area);

    /* Straight sources use (As, 1-As). Premultiplied sources also publish the
     * MPP flag so SDK normal/CMDQ backends select the required conversion.
     * Never select SRC_OVER without also accounting for global opacity. */
    image_source_alpha(&blt.ctrl, src_cf, draw_dsc->opa, image_is_premultiplied(src));
    blt.ctrl.flags = rotation_flags;
    if (draw_dsc->colorkey != NULL) {
        blt.ctrl.ck_en = 1U;
        blt.ctrl.ck_value = lv_color_to_u32(draw_dsc->colorkey->low) & 0xffffffU;
    }
    unsigned commands;
    if(!lv_aic_ge2d_stripe_count(&blt,&commands)) return false;
    if(s_blit_validate_only) return true;
    lv_draw_aic_ge2d_prepare_src_cache(src,&src_area);
    lv_draw_aic_ge2d_prepare_dst_cache(dst,&dst_area);
    ge = lv_draw_aic_ge2d_device();
    if (ge == NULL) {
        LV_LOG_ERROR("GE2D device is not open");
        return false;
    }

    int result=lv_aic_ge2d_stripe_run(ge,&blt);
    if(result<0) {
        s_blit_failed=true;
        LV_LOG_ERROR("GE2D blit strip failed");
    }
    return result>0;
}

/* Keep the decoder open across both passes. No tile writes before every
 * clipped tile has passed geometry/address checks; never replay a partial
 * blend through software after an engine failure. */
static int lv_draw_aic_ge2d_tiles(lv_draw_task_t *task, const lv_draw_image_dsc_t *dsc,
                                 const lv_image_decoder_dsc_t *decoder, bool validate_only)
{
    int32_t w = dsc->header.w, h = dsc->header.h;
    lv_area_t visible;
    if (w <= 0 || h <= 0 || !task->target_layer || !task->target_layer->draw_buf ||
        !decoder->decoded || !lv_draw_aic_ge2d_device()) return 0;
    /* 2 means a successful no-op, distinct from an engine submission. */
    if (!lv_area_intersect(&visible, &task->area, &task->clip_area) ||
        !lv_area_intersect(&visible, &visible, &task->target_layer->buf_area)) return 2;
    lv_area_t anchor = lv_area_get_width(&dsc->image_area) >= 0 ? dsc->image_area : task->area;
    /* LVGL tiles step by untransformed source dimensions, even with scale or
     * rotation. Each cell clips its transformed image; do not scale the grid.
     * Match LVGL's initial anchor and positive stepping; jump over invisible
     * rows/columns without iterating an unbounded off-screen prefix. */
    int64_t x0 = anchor.x1, y0 = anchor.y1;
    if (x0 + w - 1 < visible.x1) x0 += ((visible.x1 - x0) / w) * w;
    if (y0 + h - 1 < visible.y1) y0 += ((visible.y1 - y0) / h) * h;
    if (x0 > visible.x2 || y0 > visible.y2) return 2;
    for (int pass = 0; pass < (validate_only ? 1 : 2); pass++) {
        s_blit_validate_only = pass == 0;
        for (int64_t y = y0; y <= visible.y2; y += h) {
            for (int64_t x = x0; x <= visible.x2; x += w) {
                if (x + w - 1 > INT32_MAX || y + h - 1 > INT32_MAX) {
                    s_blit_validate_only = false;
                    return pass ? -1 : 0;
                }
                lv_area_t tile = {(int32_t)x, (int32_t)y, (int32_t)(x+w-1), (int32_t)(y+h-1)};
                lv_area_t clip;
                if (!lv_area_intersect(&clip, &tile, &visible)) continue;
                if (!lv_draw_aic_ge2d_blit(task, dsc, decoder, &tile, &clip)) {
                    s_blit_validate_only = false;
                    return pass ? -1 : 0;
                }
            }
        }
    }
    s_blit_validate_only = false;
    return 1;
}

/* SDK .fake draws the transformed bounding rectangle, not rotated pixels.
 * blend=0 replaces even transparent ARGB; blend=1 uses the encoded alpha.
 * As in the SDK, image opacity/recolor/tile do not alter this pseudo-fill. */
static lv_result_t fake_image_draw(lv_draw_task_t *task, const lv_aic_fake_image_t *fake,
                                  lv_draw_aic_ge2d_outcome_t *outcome)
{
    const lv_draw_image_dsc_t *image = task->draw_dsc;
    lv_layer_t *layer = task->target_layer;
    lv_draw_buf_t *dst = layer ? layer->draw_buf : NULL;
    lv_area_t area, clip;
    int64_t width = (int64_t)task->area.x2 - task->area.x1 + 1;
    int64_t height = (int64_t)task->area.y2 - task->area.y1 + 1;
    if (!dst || !dst->data || width < 1 || width > 4096 || height < 1 || height > 4096 ||
        image->scale_x < 16 || image->scale_x > 4096 ||
        image->scale_y < 16 || image->scale_y > 4096 ||
        image->skew_x || image->skew_y ||
        image->pivot.x < -4096 || image->pivot.x > 4096 ||
        image->pivot.y < -4096 || image->pivot.y > 4096) return LV_RESULT_INVALID;
    lv_color_format_t cf = dst->header.cf;
    if (layer->color_format != cf || !lv_draw_aic_ge2d_dst_format_supported(cf)) return LV_RESULT_INVALID;
    uint32_t bpp = lv_color_format_get_size(cf);
    int64_t lw = (int64_t)layer->buf_area.x2 - layer->buf_area.x1 + 1;
    int64_t lh = (int64_t)layer->buf_area.y2 - layer->buf_area.y1 + 1;
    if (lw < 1 || lh < 1 || lw > dst->header.w || lh > dst->header.h ||
        (uint64_t)lw * bpp > dst->header.stride ||
        (uint64_t)dst->header.stride * lh > dst->data_size) return LV_RESULT_INVALID;
    lv_image_buf_get_transformed_area(&area, (int32_t)width, (int32_t)height,
                                     image->rotation, image->scale_x, image->scale_y, &image->pivot);
    if ((int64_t)area.x1 + task->area.x1 < INT32_MIN ||
        (int64_t)area.y1 + task->area.y1 < INT32_MIN ||
        (int64_t)area.x2 + task->area.x1 > INT32_MAX ||
        (int64_t)area.y2 + task->area.y1 > INT32_MAX) return LV_RESULT_INVALID;
    lv_area_move(&area, task->area.x1, task->area.y1);
    if (!lv_area_intersect(&clip, &area, &task->clip_area) ||
        !lv_area_intersect(&clip, &clip, &layer->buf_area)) return LV_RESULT_OK;
    lv_draw_fill_dsc_t fill;
    lv_draw_fill_dsc_init(&fill);
    fill.color = lv_color_hex(fake->argb);
    fill.opa = fake->argb >> 24;
    if (fake->blend && fill.opa <= LV_OPA_MIN) return LV_RESULT_OK;
    lv_draw_task_t fill_task = *task;
    fill_task.type = LV_DRAW_TASK_TYPE_FILL;
    fill_task.draw_dsc = &fill;
    fill_task.area = area;
    fill_task.clip_area = clip;
    if (lv_draw_aic_ge2d_device() && lv_draw_aic_ge2d_buf_address_valid(dst) &&
        (!fake->blend || fill.opa >= LV_OPA_MAX || cf != LV_COLOR_FORMAT_ARGB8888)) {
        lv_result_t result = fake->blend ? lv_draw_aic_ge2d_fill(&fill_task) :
                                          lv_draw_aic_ge2d_fill_replace(&fill_task, fake->argb);
        if (result == LV_RESULT_OK && outcome) *outcome = LV_DRAW_AIC_GE2D_OUTCOME_ENGINE;
        return result; /* Never replay a failed hardware operation. */
    }
    if (fake->blend) {
        lv_draw_sw_fill(&fill_task, &fill, &area);
    }
    else {
        /* No blending on blend=0: preserve all 32 bits on ARGB, including
         * zero alpha. Byte stores also allow unaligned/padded RGB888 rows. */
        uint16_t rgb565 = (uint16_t)(((fake->argb >> 8) & 0xf800) |
                                   ((fake->argb >> 5) & 0x07e0) |
                                   ((fake->argb >> 3) & 0x001f));
        int32_t x0 = clip.x1 - layer->buf_area.x1, y0 = clip.y1 - layer->buf_area.y1;
        int32_t cw = lv_area_get_width(&clip), ch = lv_area_get_height(&clip);
        for (int32_t y = 0; y < ch; y++) {
            uint8_t *p = dst->data + (uint32_t)(y0 + y) * dst->header.stride + (uint32_t)x0 * bpp;
            for (int32_t x = 0; x < cw; x++, p += bpp) {
                if (cf == LV_COLOR_FORMAT_RGB565) {
                    p[0] = rgb565 & 255; p[1] = rgb565 >> 8;
                }
                else {
                    p[0] = fake->argb; p[1] = fake->argb >> 8; p[2] = fake->argb >> 16;
                    if (bpp == 4) p[3] = fake->argb >> 24;
                }
            }
        }
    }
    if (outcome) *outcome = LV_DRAW_AIC_GE2D_OUTCOME_SOFTWARE;
    return LV_RESULT_OK;
}

static lv_result_t image_draw(lv_draw_task_t *task,
                                   lv_draw_aic_ge2d_outcome_t *outcome)
{
    const lv_draw_image_dsc_t *dsc;
    const lv_draw_image_dsc_t *blit_dsc;
    lv_draw_image_dsc_t layer_dsc;
    lv_layer_t *layer_to_draw;
    const lv_image_decoder_args_t decoder_args = {
        .stride_align = false,
        .premultiply = false,
    };

    if (outcome != NULL) {
        *outcome = LV_DRAW_AIC_GE2D_OUTCOME_NOTHING;
    }

    if (task == NULL) {
        return LV_RESULT_INVALID;
    }
    if (task->type != LV_DRAW_TASK_TYPE_IMAGE &&
        task->type != LV_DRAW_TASK_TYPE_LAYER) {
        return LV_RESULT_INVALID;
    }

    dsc = (const lv_draw_image_dsc_t *)task->draw_dsc;
    if (dsc == NULL || dsc->src == NULL) {
        return LV_RESULT_INVALID;
    }
    int yuv=lv_draw_aic_ge2d_yuv(task);
    if (yuv<0) return LV_RESULT_INVALID;
    if (yuv>0) {
        if (yuv==1 && outcome) *outcome=LV_DRAW_AIC_GE2D_OUTCOME_ENGINE;
        return LV_RESULT_OK;
    }
    lv_aic_fake_image_t fake;
    if (task->type == LV_DRAW_TASK_TYPE_IMAGE && lv_image_src_get_type(dsc->src) == LV_IMAGE_SRC_FILE &&
        lv_aic_fake_image_parse(dsc->src, &fake)) return fake_image_draw(task, &fake, outcome);

    /* A LAYER task's source is the child layer, and the layer's buffer is what
     * actually gets blitted. Wrapping it in an image descriptor lets the code
     * below run unchanged - the same trick lv_draw_sw_layer() uses, for the
     * same reason. */
    blit_dsc = dsc;
    if (task->type == LV_DRAW_TASK_TYPE_LAYER) {
        layer_to_draw = (lv_layer_t *)dsc->src;
        if (layer_to_draw == NULL) {
            return LV_RESULT_INVALID;
        }
        if (layer_to_draw->draw_buf == NULL) {
            /* Nothing was drawn on the layer, so LVGL never allocated its
             * buffer. There is nothing to blend, and that is not a failure:
             * lv_draw_sw_layer() returns here for exactly the same case. The
             * outcome stays OUTCOME_NOTHING - no pixels were needed, so this
             * must not be reported as either engine work or a software fallback. */
            return LV_RESULT_OK;
        }
        layer_dsc = *dsc;
        layer_dsc.src = layer_to_draw->draw_buf;
        blit_dsc = &layer_dsc;
    }

    s_blit_called = false;
    s_blit_ok = false;
    s_blit_failed = false;

    /* Own the decoder through completion. Upstream helpers unconditionally
     * close it after the callback, which is unsafe after uncertain DMA.
     * Whole-image decoders use GE; partial decoders retain the SW path. */
    lv_area_t draw_area = task->area, clipped;
    if (!blit_dsc->tile && (blit_dsc->rotation ||
        blit_dsc->scale_x != LV_SCALE_NONE || blit_dsc->scale_y != LV_SCALE_NONE)) {
        lv_image_buf_get_transformed_area(&draw_area,
            lv_area_get_width(&task->area), lv_area_get_height(&task->area),
            blit_dsc->rotation, blit_dsc->scale_x, blit_dsc->scale_y, &blit_dsc->pivot);
        lv_area_move(&draw_area, task->area.x1, task->area.y1);
    }
    if (!lv_area_intersect(&clipped, &draw_area, &task->clip_area)) return LV_RESULT_OK;
    if (lv_image_decoder_open(&image_decoder, blit_dsc->src, &decoder_args) == LV_RESULT_OK) {
        bool prepared = true;
        if(blit_dsc->rotation % 900 != 0 &&
           (blit_dsc->scale_x != LV_SCALE_NONE || blit_dsc->scale_y != LV_SCALE_NONE)) {
            const lv_draw_buf_t *source = image_decoder.decoded;
            prepared = source && lv_aic_ge2d_transform_plan(source->header.w,source->header.h,
                           blit_dsc,AIC_LVGL_GE2D_TRANSFORM_BYTES,&transform_plan);
            if(prepared) {
                /* Header-only preflight: no allocation, cache or DMA yet. */
                transform_scaled.header.w = transform_plan.scaled_w;
                transform_scaled.header.h = transform_plan.scaled_h;
                transform_active = true;
                s_blit_validate_only = true;
                if(blit_dsc->tile) {
                    int preflight = lv_draw_aic_ge2d_tiles(task,blit_dsc,&image_decoder,true);
                    if(preflight == 2) {
                        s_blit_validate_only = transform_active = false;
                        transform_release(); lv_image_decoder_close(&image_decoder);
                        return LV_RESULT_OK;
                    }
                    prepared = preflight > 0;
                }
                else prepared = lv_draw_aic_ge2d_blit(task,blit_dsc,&image_decoder,&task->area,&clipped);
                s_blit_validate_only = false;
                if(prepared) prepared = transform_prepare(source);
            }
        }
        if (prepared && blit_dsc->tile) {
            int tiled = lv_draw_aic_ge2d_tiles(task, blit_dsc, &image_decoder, false);
            s_blit_called = tiled != 2;
            s_blit_ok = tiled > 0;
            s_blit_failed = tiled < 0;
            if (tiled == 2) {
                transform_active = false; transform_release();
                lv_image_decoder_close(&image_decoder);
                return LV_RESULT_OK;
            }
        }
        else if (prepared && image_decoder.decoded) {
            s_blit_called = true;
            s_blit_ok = lv_draw_aic_ge2d_blit(task, blit_dsc, &image_decoder,
                                            &task->area, &clipped);
        }
        transform_active = false;
        if(!s_blit_failed) transform_release();
        if (s_blit_failed) {
            /* Retain decoder-owned pixels, cache references and file state.
             * Dispatcher retains the task and destination until reboot. */
            decoder_quarantined = true;
        }
        else lv_image_decoder_close(&image_decoder);
    }

    if (s_blit_failed) {
        /* GE may already have blended pixels: never retry those in software. */
        return LV_RESULT_INVALID;
    }
    if (s_blit_called && s_blit_ok) {
        if (outcome != NULL) {
            *outcome = LV_DRAW_AIC_GE2D_OUTCOME_ENGINE;
        }
        return LV_RESULT_OK;
    }

    /* Either the decode failed or the engine could not take the source. Hand
     * the whole task to the software renderer so the image is never dropped.
     * This re-decodes, which is wasteful but is the only path that is correct
     * for every rejection reason without duplicating the SW blend code.
     *
     * The fallback takes the ORIGINAL descriptor, not the wrapped one: the
     * layer path has to go through lv_draw_sw_layer() so the layer's own
     * bookkeeping stays in upstream's hands. */
    if (task->type == LV_DRAW_TASK_TYPE_LAYER) {
        lv_draw_sw_layer(task, dsc, &task->area);
    }
    else {
        lv_draw_sw_image(task, dsc, &task->area);
    }

    /* Report the fallback, so the dispatcher can record that an accepted task
     * was drawn by the CPU rather than by the engine. */
    if (outcome != NULL) {
        *outcome = LV_DRAW_AIC_GE2D_OUTCOME_SOFTWARE;
    }
    return LV_RESULT_OK;
}

lv_result_t lv_draw_aic_ge2d_image(lv_draw_task_t *task,lv_draw_aic_ge2d_outcome_t *outcome)
{
    if(outcome) *outcome=LV_DRAW_AIC_GE2D_OUTCOME_NOTHING;
    if(lv_draw_aic_ge2d_faulted()) return LV_RESULT_INVALID;
    lv_aic_rgb_image_t *lease=NULL;
    const lv_aic_rgb_frame_t *frame;
    if(task && task->type==LV_DRAW_TASK_TYPE_IMAGE && task->draw_dsc) {
        const lv_draw_image_dsc_t *dsc=task->draw_dsc;
        lease=lv_aic_rgb_image_acquire(dsc->src,&frame);
    }
    s_blit_failed=false;
    lv_result_t result=image_draw(task,outcome);
    if(lease) {
        /* Preserve the producer lease as well as the open decoder snapshot
         * if DMA completion is uncertain. */
        if(s_blit_failed) rgb_quarantined=lease;
        else lv_aic_rgb_image_release_lease(lease);
    }
    return result;
}

#endif /* AIC_LVGL_USE_GE2D && AIC_LVGL_BSP_MPP */
