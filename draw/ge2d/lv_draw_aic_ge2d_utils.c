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

void lv_draw_aic_ge2d_prepare_dst_cache(const lv_draw_buf_t *draw_buf,
                                        const lv_area_t *rel_area)
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

    /* Row start in bytes. stride is authoritative: it is not necessarily
     * width * bpp, so the row pitch must come from the header. */
    address = draw_buf->data
              + ((uint32_t)rel_area->x1 * bpp)
              + (stride * (uint32_t)rel_area->y1);
    line_bytes = width * (int32_t)bpp;

    for (row = 0; row < height; row++) {
        /* Widen to the containing cache lines. The first row is usually
         * misaligned, hence the extra head bytes. */
        int32_t head = (int32_t)((ulong)address & (CACHE_LINE_SIZE - 1U));
        aicos_dcache_clean_invalid_range(
            (void *)ALIGN_DOWN((ulong)address, CACHE_LINE_SIZE),
            (u32)ALIGN_UP((u32)(line_bytes + head), CACHE_LINE_SIZE));
        address += stride;
    }
}

#endif /* AIC_LVGL_USE_GE2D && AIC_LVGL_BSP_MPP */
