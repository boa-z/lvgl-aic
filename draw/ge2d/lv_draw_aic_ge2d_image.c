/**
 * @file lv_draw_aic_ge2d_image.c
 * @brief Plain image blit through the ArtInChip GE2D engine.
 *
 * Supports exactly one shape of work: an untransformed copy from a
 * GE-addressable source into a GE-addressable destination. No scale, no
 * rotation, no recolor, no tile, no mask. evaluate() rejects everything else,
 * so this function can assume the simple case.
 *
 * Source alpha is the one thing that is not "the simple case" and is handled
 * anyway: an ARGB8888 source is blended with its own per-pixel alpha through
 * GE_PD_SRC_OVER, and a source without an alpha channel is blended by global
 * alpha when the descriptor carries a partial opacity.
 *
 * Both IMAGE and LAYER tasks arrive here. A LAYER task carries an lv_layer_t in
 * place of the source, so its draw buffer is wrapped in a temporary image
 * descriptor and the identical blit runs - there is no second code path to keep
 * in step, and no separate "blend a layer" implementation to drift.
 *
 * The decode and the clip bookkeeping are left to LVGL: this file only supplies
 * the core callback, so the source coordinates, the palette handling and the
 * decoder lifetime stay in the upstream code that already gets them right. In
 * particular there is no local copy of the 9.1 image helpers.
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
 * Note on the LAYER source address: LVGL allocates layer buffers with
 * lv_malloc, which on this SDK is rt_malloc, i.e. the RT-Thread system heap.
 * With CONFIG_AIC_DEFAULT_SYS_HEAP_SRAM that heap is at 0x30040000, below the
 * 0x40000000 floor the D13x GE can reach, so a LAYER composite is declined here
 * and composited in software. That is a buffer-placement property, not a defect
 * in this file: moving layer buffers into CMA would make the same code run on
 * the engine, and the address gate already makes the transition safe.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#define AIC_LVGL_USE_PRIVATE_API 1
#include "lv_draw_aic_ge2d.h"
#include "lv_draw_aic_ge2d_utils.h"
#include "lv_aic_pixel_format.h"

#if AIC_LVGL_USE_GE2D && AIC_LVGL_BSP_MPP

#include "lvgl_aic_private.h"

#include <aic_core.h>
#include <mpp_ge.h>

/**
 * Outcome of one blit attempt.
 *
 * The core callback LVGL hands us returns void, so the result travels back
 * through this file-scope record. That is safe here because the unit is
 * strictly synchronous: dispatch() keeps at most one task in flight and there
 * is no render thread, so two attempts cannot overlap. The MPP decoder always
 * sets @c decoder_dsc->decoded for a whole image, which means the callback runs
 * exactly once per task and the record cannot be overwritten mid-attempt.
 */
static bool s_blit_called;
static bool s_blit_ok;

/**
 * Build the GE2D blit descriptor and run it.
 *
 * Returns true when the engine accepted and completed the copy. A false return
 * is a request to fall back, not an error: the caller still has to draw the
 * pixels somehow.
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

    /* Source crop, relative to the buffer origin. For an untransformed image
     * the decoded pixel (0,0) sits at img_coords->x1/y1. */
    src_area = *clipped_img_area;
    lv_area_move(&src_area, -img_coords->x1, -img_coords->y1);

    /* Destination crop, relative to the layer buffer origin. */
    dst_area = *clipped_img_area;
    lv_area_move(&dst_area, -layer->buf_area.x1, -layer->buf_area.y1);

    blit_w = lv_area_get_width(&src_area);
    blit_h = lv_area_get_height(&src_area);
    if (blit_w <= 0 || blit_h <= 0) {
        /* Fully clipped: nothing to draw, and that is not a failure. */
        return true;
    }
    /* A plain blit maps the source crop 1:1 onto the destination crop. */
    if (lv_area_get_width(&dst_area) != blit_w ||
        lv_area_get_height(&dst_area) != blit_h) {
        return false;
    }

    /* Source is read by the engine, so write back; destination is written. */
    lv_draw_aic_ge2d_prepare_src_cache(src, &src_area);
    lv_draw_aic_ge2d_prepare_dst_cache(dst, &dst_area);

    blt.src_buf.buf_type = MPP_PHY_ADDR;
    blt.src_buf.phy_addr[0] = (uint32_t)(ulong)src->data;
    blt.src_buf.stride[0] = src->header.stride;
    /* size describes the whole buffer; crop selects the rectangle to copy.
     * Using the header dimensions - not the crop extent - guarantees the crop
     * is always inside the declared buffer. */
    blt.src_buf.size.width = src->header.w;
    blt.src_buf.size.height = src->header.h;
    blt.src_buf.format = src_fmt;
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
    blt.dst_buf.crop.width = (uint32_t)blit_w;
    blt.dst_buf.crop.height = (uint32_t)blit_h;

    /* Blending is needed when either side can contribute transparency: the
     * source may carry a per-pixel alpha channel, and the draw descriptor may
     * carry a partial global opacity. Only a source with neither takes the
     * plain-copy path and skips the Porter/Duff datapath entirely.
     *
     * Note the D13x header comment inverts alpha_en's meaning - the HAL enables
     * blending when it is non-zero, which is what the vendor port relies on too.
     * src_alpha_mode 2 is the "mixed" mode, src_alpha = pixel alpha * global
     * alpha / 255, which is exactly LVGL's own composition rule when both are
     * present. A source with no alpha channel has an opaque pixel alpha, so the
     * product reduces to the global value.
     *
     * In practice the global-alpha half of this is reached by LAYER tasks:
     * accepts_image() still declines a partial opa, because driving image
     * opacity is Phase 3C work. The condition is written for the operation
     * rather than for the caller, so it stays correct when that lifts. */
    if (draw_dsc->opa >= LV_OPA_COVER && src_cf != LV_COLOR_FORMAT_ARGB8888) {
        blt.ctrl.alpha_en = 0U;
        blt.ctrl.src_alpha_mode = 0U;
    }
    else {
        blt.ctrl.alpha_en = 1U;
        blt.ctrl.alpha_rules = GE_PD_SRC_OVER;
        blt.ctrl.src_alpha_mode = 2U;
        blt.ctrl.src_global_alpha = draw_dsc->opa;
    }

    ge = lv_draw_aic_ge2d_device();
    if (ge == NULL) {
        LV_LOG_ERROR("GE2D device is not open");
        return false;
    }

    if (mpp_ge_bitblt(ge, &blt) < 0) {
        LV_LOG_ERROR("GE2D bitblt failed");
        return false;
    }
    if (mpp_ge_emit(ge) < 0) {
        LV_LOG_ERROR("GE2D emit failed");
        return false;
    }
    if (mpp_ge_sync(ge) < 0) {
        LV_LOG_ERROR("GE2D sync failed");
        return false;
    }

    return true;
}

/**
 * Core callback handed to lv_draw_image_normal_helper().
 *
 * Only records the outcome; the fallback decision belongs to the caller, which
 * still holds the original draw descriptor and the task.
 */
static void lv_draw_aic_ge2d_image_cb(lv_draw_task_t *task,
                                      const lv_draw_image_dsc_t *draw_dsc,
                                      const lv_image_decoder_dsc_t *decoder_dsc,
                                      lv_draw_image_sup_t *sup,
                                      const lv_area_t *img_coords,
                                      const lv_area_t *clipped_img_area)
{
    LV_UNUSED(sup);

    s_blit_called = true;
    s_blit_ok = lv_draw_aic_ge2d_blit(task, draw_dsc, decoder_dsc, img_coords,
                                      clipped_img_area);
}

lv_result_t lv_draw_aic_ge2d_image(lv_draw_task_t *task,
                                   lv_draw_aic_ge2d_outcome_t *outcome)
{
    const lv_draw_image_dsc_t *dsc;
    const lv_draw_image_dsc_t *blit_dsc;
    lv_draw_image_dsc_t layer_dsc;
    lv_layer_t *layer_to_draw;

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

    /* LVGL owns the decode, the clip intersection and the decoder lifetime. */
    lv_draw_image_normal_helper(task, blit_dsc, &task->area,
                                lv_draw_aic_ge2d_image_cb, NULL);

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

#endif /* AIC_LVGL_USE_GE2D && AIC_LVGL_BSP_MPP */
