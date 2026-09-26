/**
 * @file lv_aic_mpp_format.c
 * @brief Decoder-facing LVGL <-> MPP format policy.
 *
 * The translation itself lives in common/lv_aic_pixel_format.c, which GE2D
 * depends on too. This file keeps only what is specific to the image decoder:
 * which formats the decoder is willing to request or accept, and the pixel
 * size helper used for stride validation.
 *
 * Only SW-consumable RGB paths are admitted. YUV is intentionally rejected
 * here so a YUV-only MPP configuration fails loudly instead of smuggling an
 * mpp_buf metadata pointer into the SW renderer.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "lv_aic_mpp_format.h"
#include "lv_aic_pixel_format.h"

#if AIC_LVGL_USE_MPP_DEC && AIC_LVGL_BSP_MPP

bool lv_aic_mpp_format_to_lvgl(enum mpp_pixel_format mpp_fmt,
                               lv_color_format_t *lv_fmt)
{
    /* XRGB8888 maps cleanly in the shared table but no decoder entry point
     * asks for it, and Phase 2 validated the decoder against RGB565/RGB888/
     * ARGB8888 only. Keep the accepted set symmetric with from_lvgl() so this
     * translation never widens decoder behaviour by accident. */
    if (mpp_fmt == MPP_FMT_XRGB_8888) {
        return false;
    }

    return lv_aic_pixel_format_from_mpp(mpp_fmt, lv_fmt);
}

bool lv_aic_mpp_format_from_lvgl(lv_color_format_t lv_fmt,
                                 enum mpp_pixel_format *mpp_fmt)
{
    /* XRGB8888 is a valid GE2D destination but no decoder entry point requests
     * it, so it stays out of the decoder's accepted set. */
    if (lv_fmt == LV_COLOR_FORMAT_XRGB8888) {
        return false;
    }

    return lv_aic_pixel_format_to_mpp(lv_fmt, mpp_fmt);
}

bool lv_aic_mpp_format_lvgl_has_alpha(lv_color_format_t lv_fmt)
{
    return lv_fmt == LV_COLOR_FORMAT_ARGB8888;
}

uint32_t lv_aic_mpp_format_lvgl_bpp(lv_color_format_t lv_fmt)
{
    switch (lv_fmt) {
    case LV_COLOR_FORMAT_RGB565:
        return 2U;
    case LV_COLOR_FORMAT_RGB888:
        return 3U;
    case LV_COLOR_FORMAT_ARGB8888:
        return 4U;
    default:
        return 0U;
    }
}

#endif /* AIC_LVGL_USE_MPP_DEC && AIC_LVGL_BSP_MPP */
