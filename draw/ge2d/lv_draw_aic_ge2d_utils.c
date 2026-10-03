/**
 * @file lv_draw_aic_ge2d_utils.c
 * @brief GE2D acceptance helpers and destination cache preparation.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#define AIC_LVGL_USE_PRIVATE_API 1
#include "lv_draw_aic_ge2d_utils.h"
#include "lv_aic_pixel_format.h"

#if AIC_LVGL_USE_GE2D && AIC_LVGL_BSP_MPP

#include "lvgl_aic_private.h"

#include <aic_core.h>
#include <aic_osal.h>

#ifndef CACHE_LINE_SIZE
#define CACHE_LINE_SIZE 32U
#endif

/* Lowest address the D13x/G73x GE block can reach through its address window. */
#define LV_AIC_GE2D_GE_ADDR_MIN 0x40000000UL

bool lv_draw_aic_ge2d_buf_address_valid(const lv_draw_buf_t *draw_buf)
{
    if (draw_buf == NULL || draw_buf->data == NULL) {
        return false;
    }

#if defined(AIC_CHIP_D13X) || defined(AIC_CHIP_G73X)
    if ((uint32_t)(ulong)draw_buf->data < (uint32_t)LV_AIC_GE2D_GE_ADDR_MIN) {
        return false;
    }
#endif

    return true;
}

bool lv_draw_aic_ge2d_dst_format_supported(lv_color_format_t cf)
{
    return lv_aic_pixel_format_is_ge2d_dst(cf);
}

/**
 * Walk @p rel_area one row at a time and prepare the cache for the region the
 * GE engine is about to touch.
 *
 * @p clean_only selects the direction of the transfer:
 *   - false (destination): clean AND invalidate. The engine writes; the CPU
 *     must not keep stale lines of the old contents, and any CPU dirty line
 *     must not be written back on top of the engine's output.
 *   - true (source): clean only. The engine reads; the CPU's newest data must
 *     reach memory, but nothing may be discarded.
 *
 * stride is authoritative - it is not necessarily width * bpp - so the row
 * pitch comes from the header, never from the area width.
 */
static void lv_draw_aic_ge2d_prepare_cache(const lv_draw_buf_t *draw_buf,
                                           const lv_area_t *rel_area,
                                           bool clean_only)
{
    const lv_image_header_t *header;
    uint32_t stride;
    uint32_t bpp;
    int32_t width;
    int32_t height;
    int32_t line_bytes;
    uint8_t *address;
    int32_t row;

    if (draw_buf == NULL || rel_area == NULL || draw_buf->data == NULL) {
        return;
    }

    header = &draw_buf->header;
    stride = header->stride;
    bpp = lv_color_format_get_size((lv_color_format_t)header->cf);
    width = lv_area_get_width(rel_area);
    height = lv_area_get_height(rel_area);

    if (stride == 0U || bpp == 0U || width <= 0 || height <= 0) {
        return;
    }

    /* Row start in bytes. */
    address = draw_buf->data
              + ((uint32_t)rel_area->x1 * bpp)
              + (stride * (uint32_t)rel_area->y1);
    line_bytes = width * (int32_t)bpp;

    for (row = 0; row < height; row++) {
        /* Widen to the containing cache lines. The first row is usually
         * misaligned, hence the extra head bytes. */
        int32_t head = (int32_t)((ulong)address & (CACHE_LINE_SIZE - 1U));
        ulong base = ALIGN_DOWN((ulong)address, CACHE_LINE_SIZE);
        ulong span = (ulong)ALIGN_UP((u32)(line_bytes + head), CACHE_LINE_SIZE);

        if (clean_only) {
            aicos_dcache_clean_range((unsigned long *)base, span);
        }
        else {
            aicos_dcache_clean_invalid_range((void *)base, (u32)span);
        }
        address += stride;
    }
}

void lv_draw_aic_ge2d_prepare_dst_cache(const lv_draw_buf_t *draw_buf,
                                        const lv_area_t *rel_area)
{
    lv_draw_aic_ge2d_prepare_cache(draw_buf, rel_area, false);
}

void lv_draw_aic_ge2d_prepare_src_cache(const lv_draw_buf_t *draw_buf,
                                        const lv_area_t *rel_area)
{
    lv_draw_aic_ge2d_prepare_cache(draw_buf, rel_area, true);
}

void lv_draw_aic_ge2d_prepare_yuv_cache(const lv_aic_yuv_frame_t *frame)
{
    lv_aic_yuv_layout_t layout;
    if (!frame || !lv_aic_yuv_layout(frame->format,frame->width,frame->height,&layout)) return;
    for (unsigned i=0;i<layout.planes;i++) {
        uintptr_t address=(uintptr_t)frame->planes[i].data;
        uintptr_t base=address & ~(uintptr_t)(CACHE_LINE_SIZE-1);
        size_t bytes=(size_t)frame->planes[i].stride*layout.rows[i]+address-base;
        bytes=(bytes+CACHE_LINE_SIZE-1) & ~(size_t)(CACHE_LINE_SIZE-1);
        aicos_dcache_clean_range((unsigned long *)base,bytes);
    }
}

#endif /* AIC_LVGL_USE_GE2D && AIC_LVGL_BSP_MPP */
