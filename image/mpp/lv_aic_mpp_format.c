/**
 * @file lv_aic_mpp_format.c
 * @brief GE2D-independent LVGL <-> MPP format mapping.
 *
 * Only SW-consumable RGB paths are admitted. YUV is intentionally rejected
 * here so a YUV-only MPP configuration fails loudly instead of smuggling an
 * mpp_buf metadata pointer into the SW renderer.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "lv_aic_mpp_format.h"

#if defined(AIC_LVGL_USE_MPP_DEC) && AIC_LVGL_BSP_MPP

bool lv_aic_mpp_format_to_lvgl(enum mpp_pixel_format mpp_fmt,
                               lv_color_format_t *lv_fmt)
{
    if (lv_fmt == NULL) {
        return false;
    }

    switch (mpp_fmt) {
    case MPP_FMT_RGB_565:
        *lv_fmt = LV_COLOR_FORMAT_RGB565;
        return true;
    case MPP_FMT_BGR_565:
        /* BGR565 has no direct LVGL enum; reject rather than mislabel. */
        return false;
    case MPP_FMT_RGB_888:
        *lv_fmt = LV_COLOR_FORMAT_RGB888;
        return true;
    case MPP_FMT_BGR_888:
        return false;
    case MPP_FMT_ARGB_8888:
        *lv_fmt = LV_COLOR_FORMAT_ARGB8888;
        return true;
    case MPP_FMT_ABGR_8888:
    case MPP_FMT_RGBA_8888:
    case MPP_FMT_BGRA_8888:
    case MPP_FMT_XRGB_8888:
    case MPP_FMT_XBGR_8888:
    case MPP_FMT_RGBX_8888:
    case MPP_FMT_BGRX_8888:
        return false;
    default:
        return false;
    }
}

bool lv_aic_mpp_format_from_lvgl(lv_color_format_t lv_fmt,
                                 enum mpp_pixel_format *mpp_fmt)
{
    if (mpp_fmt == NULL) {
        return false;
    }

    switch (lv_fmt) {
    case LV_COLOR_FORMAT_RGB565:
        *mpp_fmt = MPP_FMT_RGB_565;
        return true;
    case LV_COLOR_FORMAT_RGB888:
        *mpp_fmt = MPP_FMT_RGB_888;
        return true;
    case LV_COLOR_FORMAT_ARGB8888:
        *mpp_fmt = MPP_FMT_ARGB_8888;
        return true;
    default:
        return false;
    }
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
