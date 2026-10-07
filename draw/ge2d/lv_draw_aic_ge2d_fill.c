/**
 * @file lv_draw_aic_ge2d_fill.c
 * @brief Solid and bounded linear gradient fill through the ArtInChip GE2D engine.
 *
 * Partial opacity is supported on RGB565/RGB888/XRGB8888 destinations.
 * Partial straight ARGB8888 uses bounded GE staging plus native CPU blend.
 * Two-stop horizontal/vertical PAD gradients use the native GE fillrect
 * ramp. Rounded, complex, multi-stop and translucent-stop gradients remain
 * software tasks.
 * Validate direct callers as well as scheduler-selected tasks before DMA.
 *
 * Unlike the legacy port, every GE step is checked and a failure is returned
 * to the caller. A dropped rectangle must not be reported to the scheduler as
 * drawn.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#define AIC_LVGL_USE_PRIVATE_API 1
#include "lv_draw_aic_ge2d.h"
#include "lv_draw_aic_ge2d_utils.h"
#include "lv_aic_pixel_format.h"

#if AIC_LVGL_USE_GE2D && AIC_LVGL_BSP_MPP

#include "lvgl_aic_private.h"
#include "lv_draw_aic_ge2d_alpha.h"

#include <aic_core.h>
#include <mpp_ge.h>

/* No SDK reset primitive proves outstanding DMA is quiescent. */
static bool fill_dma_faulted;
/* Kept until reboot if submit/emit/sync cannot prove DMA is finished. */
static lv_draw_buf_t fill_alpha_surface;

bool lv_draw_aic_ge2d_fill_faulted(void)
{
    return fill_dma_faulted;
}

bool lv_draw_aic_ge2d_fill_dsc_supported(const lv_draw_fill_dsc_t *dsc)
{
    if(!dsc || dsc->radius != 0) return false;
    if(dsc->grad.dir == LV_GRAD_DIR_NONE) return true;
    if(dsc->grad.dir != LV_GRAD_DIR_HOR && dsc->grad.dir != LV_GRAD_DIR_VER) return false;
    if(dsc->grad.extend != LV_GRAD_EXTEND_PAD || dsc->grad.stops_count != 2) return false;
    if(dsc->grad.stops[0].frac != 0 || dsc->grad.stops[1].frac != 255 ||
       dsc->grad.stops[0].opa != LV_OPA_COVER || dsc->grad.stops[1].opa != LV_OPA_COVER) {
        return false;
    }
    return true;
}

static bool gradient_geometry_supported(const lv_draw_task_t *task,
                                         const lv_area_t *visible,
                                         const lv_draw_fill_dsc_t *dsc)
{
    if(dsc->grad.dir == LV_GRAD_DIR_NONE) return true;
    bool horizontal = dsc->grad.dir == LV_GRAD_DIR_HOR;
    int64_t first = horizontal ? task->area.x1 : task->area.y1;
    int64_t last = horizontal ? task->area.x2 : task->area.y2;
    int64_t span = last - first + 1;
    int64_t visible_first = horizontal ? visible->x1 : visible->y1;
    int64_t visible_last = horizontal ? visible->x2 : visible->y2;
    return span > 0 && span <= 256 && visible_first >= first && visible_last <= last;
}

static uint32_t gradient_color_at(const lv_grad_dsc_t *gradient, int32_t range, int32_t position)
{
    lv_color_t color;
    lv_opa_t opa;
    lv_draw_sw_grad_color_calculate(gradient, range, position, &color, &opa);
    return (lv_color_to_u32(color) & 0x00ffffffU) | ((uint32_t)opa << 24);
}

static void gradient_colors(const lv_draw_task_t *task, const lv_area_t *visible,
                            const lv_draw_fill_dsc_t *dsc, uint32_t *start, uint32_t *end)
{
    bool horizontal = dsc->grad.dir == LV_GRAD_DIR_HOR;
    int32_t origin = horizontal ? task->area.x1 : task->area.y1;
    int32_t range = horizontal ? lv_area_get_width(&task->area) : lv_area_get_height(&task->area);
    int32_t first = (horizontal ? visible->x1 : visible->y1) - origin;
    int32_t last = (horizontal ? visible->x2 : visible->y2) - origin;
    *start = gradient_color_at(&dsc->grad, range, first);
    *end = gradient_color_at(&dsc->grad, range, last);
}

static lv_result_t fill_core(lv_draw_task_t *task, bool replace, uint32_t argb)
{
    const lv_draw_fill_dsc_t *dsc;
    const lv_draw_buf_t *draw_buf;
    lv_layer_t *layer;
    lv_area_t blend_area;
    struct ge_fillrect fill = { 0 };
    struct mpp_ge *ge;
    enum mpp_pixel_format fmt;
    int32_t dst_width;
    int32_t dst_height;
    uint32_t bpp;
    lv_color_format_t cf;

    if (lv_draw_aic_ge2d_faulted() || task == NULL || task->type != LV_DRAW_TASK_TYPE_FILL) {
        return LV_RESULT_INVALID;
    }

    dsc = (const lv_draw_fill_dsc_t *)task->draw_dsc;
    layer = task->target_layer;
    if (dsc == NULL || layer == NULL || layer->draw_buf == NULL) {
        return LV_RESULT_INVALID;
    }
    draw_buf = layer->draw_buf;
    if (!lv_draw_aic_ge2d_fill_dsc_supported(dsc)) {
        return LV_RESULT_INVALID;
    }
    if (!replace && dsc->opa <= LV_OPA_MIN) {
        return LV_RESULT_OK;
    }
    cf = (lv_color_format_t)draw_buf->header.cf;
    if ((draw_buf->header.flags & LV_IMAGE_FLAGS_PREMULTIPLIED) ||
        !lv_draw_aic_ge2d_dst_format_supported(cf) ||
        !lv_draw_aic_ge2d_buf_address_valid(draw_buf)) {
        return LV_RESULT_INVALID;
    }
    dst_width = lv_area_get_width(&layer->buf_area);
    dst_height = lv_area_get_height(&layer->buf_area);
    bpp = lv_color_format_get_size(cf);
    if (dst_width <= 0 || dst_height <= 0 || bpp == 0 ||
        (uint32_t)dst_width > draw_buf->header.w ||
        (uint32_t)dst_height > draw_buf->header.h ||
        (uint64_t)dst_width * bpp > draw_buf->header.stride ||
        (uint64_t)draw_buf->header.stride * dst_height > draw_buf->data_size) {
        return LV_RESULT_INVALID;
    }

    /* Use the clip area saved in the task. layer->_clip_area belongs to task
     * creation and may already describe a later task. */
    if (!lv_area_intersect(&blend_area, &task->area, &task->clip_area) ||
        !lv_area_intersect(&blend_area, &blend_area, &layer->buf_area)) {
        /* Fully clipped: nothing to draw, and that is not a failure. */
        return LV_RESULT_OK;
    }

    if (!gradient_geometry_supported(task, &blend_area, dsc)) return LV_RESULT_INVALID;

    uint32_t gradient_start = 0, gradient_end = 0;
    if (dsc->grad.dir != LV_GRAD_DIR_NONE) {
        gradient_colors(task, &blend_area, dsc, &gradient_start, &gradient_end);
    }

    if (!replace && dsc->opa < LV_OPA_MAX && cf == LV_COLOR_FORMAT_ARGB8888) {
        lv_layer_t staged_layer;
        lv_draw_task_t staged_task;
        /* Original destination validation and visible clipping precede allocation.
         * Never submit with a missing engine or leave scratch after preflight failure. */
        if (!lv_draw_aic_ge2d_device() ||
            !lv_aic_ge2d_alpha_prepare(task, &blend_area, &fill_alpha_surface,
                                       &staged_layer, &staged_task)) return LV_RESULT_INVALID;
        /* Store opaque raw RGB. Global opacity is applied exactly once by the
         * native tail, after all GE commands are known to have completed. */
        lv_result_t result = fill_core(&staged_task, true,
                                       lv_color_to_u32(dsc->color) | 0xff000000U);
        /* Solid fills have no source alpha. XRGB avoids the ARGB image kernel's
         * extra alpha*opacity quantization near LV_OPA_MIN. */
        if (result == LV_RESULT_OK)
            lv_aic_ge2d_alpha_finish(task, &fill_alpha_surface, &staged_layer,
                                    dsc->opa, LV_COLOR_FORMAT_XRGB8888, false);
        if (!fill_dma_faulted) lv_aic_ge2d_alpha_release(&fill_alpha_surface);
        return result;
    }

    /* GE addresses the destination buffer, so crop in buffer-relative space. */
    lv_area_move(&blend_area, -layer->buf_area.x1, -layer->buf_area.y1);

    if (!lv_aic_pixel_format_to_mpp(cf, &fmt)) {
        return LV_RESULT_INVALID;
    }
    ge = lv_draw_aic_ge2d_device();
    if (ge == NULL) {
        LV_LOG_ERROR("GE2D device is not open");
        return LV_RESULT_INVALID;
    }

    /* Preserve the background before GE reads it for a partial-alpha blend. */
    lv_draw_aic_ge2d_prepare_dst_cache(draw_buf, &blend_area);

    fill.type = dsc->grad.dir == LV_GRAD_DIR_HOR ? GE_H_LINEAR_GRADIENT :
                dsc->grad.dir == LV_GRAD_DIR_VER ? GE_V_LINEAR_GRADIENT : GE_NO_GRADIENT;
    if (dsc->grad.dir == LV_GRAD_DIR_NONE) {
        fill.start_color = lv_color_to_u32(dsc->color);
        fill.end_color = 0U;
    }
    else {
        fill.start_color = gradient_start;
        fill.end_color = gradient_end;
    }
    if (!replace && dsc->opa < LV_OPA_MAX) {
        fill.start_color = (fill.start_color & 0x00ffffffU) | ((uint32_t)dsc->opa << 24);
        if(dsc->grad.dir != LV_GRAD_DIR_NONE) fill.end_color = (fill.end_color & 0x00ffffffU) | ((uint32_t)dsc->opa << 24);
    }
    if (replace && dsc->grad.dir == LV_GRAD_DIR_NONE) {
        fill.start_color = argb;
        fill.end_color = 0U;
    }

    fill.dst_buf.buf_type = MPP_PHY_ADDR;
    fill.dst_buf.phy_addr[0] = (uint32_t)(ulong)draw_buf->data;
    /* stride is authoritative. It is not necessarily width * bpp. */
    fill.dst_buf.stride[0] = draw_buf->header.stride;
    fill.dst_buf.size.width = dst_width;
    fill.dst_buf.size.height = dst_height;
    fill.dst_buf.format = fmt;
    fill.dst_buf.crop_en = 1U;
    fill.dst_buf.crop.x = blend_area.x1;
    fill.dst_buf.crop.y = blend_area.y1;
    fill.dst_buf.crop.width = lv_area_get_width(&blend_area);
    fill.dst_buf.crop.height = lv_area_get_height(&blend_area);

    /* GE_PD_NONE is straight alpha (sa, 1-sa); SRC_OVER is premultiplied. */
    fill.ctrl.alpha_en = !replace && dsc->opa < LV_OPA_MAX;
    fill.ctrl.alpha_rules = GE_PD_NONE;
    fill.ctrl.src_alpha_mode = 0U;

    if (mpp_ge_fillrect(ge, &fill) < 0) {
        fill_dma_faulted = true;
        LV_LOG_ERROR("GE2D fillrect failed");
        return LV_RESULT_INVALID;
    }
    if (mpp_ge_emit(ge) < 0) {
        fill_dma_faulted = true;
        LV_LOG_ERROR("GE2D emit failed");
        return LV_RESULT_INVALID;
    }
    if (mpp_ge_sync(ge) < 0) {
        fill_dma_faulted = true;
        LV_LOG_ERROR("GE2D sync failed");
        return LV_RESULT_INVALID;
    }

    return LV_RESULT_OK;
}

lv_result_t lv_draw_aic_ge2d_fill(lv_draw_task_t *task)
{
    return fill_core(task, false, 0);
}

lv_result_t lv_draw_aic_ge2d_fill_replace(lv_draw_task_t *task, uint32_t argb)
{
    /* Replacement is an exact solid color API, including alpha-zero clears. */
    if(!task || !task->draw_dsc ||
       ((const lv_draw_fill_dsc_t *)task->draw_dsc)->grad.dir != LV_GRAD_DIR_NONE)
        return LV_RESULT_INVALID;
    return fill_core(task, true, argb);
}

#endif /* AIC_LVGL_USE_GE2D && AIC_LVGL_BSP_MPP */
