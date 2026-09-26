/**
 * @file lv_draw_aic_ge2d_fill.c
 * @brief Opaque solid fill through the ArtInChip GE2D engine.
 *
 * Phase 3A supports exactly one shape of work: no radius, no gradient, fully
 * opaque, supported destination format, GE-addressable buffer. evaluate()
 * rejects everything else, so this function can assume the simple case.
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

#include <aic_core.h>
#include <mpp_ge.h>

lv_result_t lv_draw_aic_ge2d_fill(lv_draw_task_t *task)
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

    if (task == NULL || task->type != LV_DRAW_TASK_TYPE_FILL) {
        return LV_RESULT_INVALID;
    }

    dsc = (const lv_draw_fill_dsc_t *)task->draw_dsc;
    layer = task->target_layer;
    if (dsc == NULL || layer == NULL || layer->draw_buf == NULL) {
        return LV_RESULT_INVALID;
    }
    draw_buf = layer->draw_buf;

    /* Use the clip area saved in the task. layer->_clip_area belongs to task
     * creation and may already describe a later task. */
    if (!lv_area_intersect(&blend_area, &task->area, &task->clip_area)) {
        /* Fully clipped: nothing to draw, and that is not a failure. */
        return LV_RESULT_OK;
    }

    /* GE addresses the destination buffer, so crop in buffer-relative space. */
    lv_area_move(&blend_area, -layer->buf_area.x1, -layer->buf_area.y1);

    dst_width = lv_area_get_width(&layer->buf_area);
    dst_height = lv_area_get_height(&layer->buf_area);
    if (dst_width <= 0 || dst_height <= 0) {
        return LV_RESULT_INVALID;
    }

    if (!lv_aic_pixel_format_to_mpp((lv_color_format_t)draw_buf->header.cf, &fmt)) {
        return LV_RESULT_INVALID;
    }

    /* Settle the cache before the engine writes, for the touched region only. */
    lv_draw_aic_ge2d_prepare_dst_cache(draw_buf, &blend_area);

    fill.type = GE_NO_GRADIENT;
    /* Opaque fill: GE ignores the alpha byte when ctrl.alpha_en is 0. */
    fill.start_color = lv_color_to_u32(dsc->color);
    fill.end_color = 0U;

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

    /* No source alpha and no destination read-modify-write in Phase 3A. */
    fill.ctrl.alpha_en = 0U;
    fill.ctrl.src_alpha_mode = 0U;

    ge = lv_draw_aic_ge2d_device();
    if (ge == NULL) {
        LV_LOG_ERROR("GE2D device is not open");
        return LV_RESULT_INVALID;
    }

    if (mpp_ge_fillrect(ge, &fill) < 0) {
        LV_LOG_ERROR("GE2D fillrect failed");
        return LV_RESULT_INVALID;
    }
    if (mpp_ge_emit(ge) < 0) {
        LV_LOG_ERROR("GE2D emit failed");
        return LV_RESULT_INVALID;
    }
    if (mpp_ge_sync(ge) < 0) {
        LV_LOG_ERROR("GE2D sync failed");
        return LV_RESULT_INVALID;
    }

    return LV_RESULT_OK;
}

#endif /* AIC_LVGL_USE_GE2D && AIC_LVGL_BSP_MPP */
