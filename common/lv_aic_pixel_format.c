/**
 * @file lv_aic_pixel_format.c
 * @brief Shared LVGL <-> MPP pixel format mapping.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "lv_aic_pixel_format.h"

#if AIC_LVGL_BSP_MPP

bool lv_aic_pixel_format_to_mpp(lv_color_format_t lv_fmt,
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
    case LV_COLOR_FORMAT_XRGB8888:
        *mpp_fmt = MPP_FMT_XRGB_8888;
        return true;
    default:
        return false;
    }
}

bool lv_aic_pixel_format_from_mpp(enum mpp_pixel_format mpp_fmt,
                                  lv_color_format_t *lv_fmt)
{
    if (lv_fmt == NULL) {
        return false;
    }

    switch (mpp_fmt) {
    case MPP_FMT_RGB_565:
        *lv_fmt = LV_COLOR_FORMAT_RGB565;
        return true;
    case MPP_FMT_RGB_888:
        *lv_fmt = LV_COLOR_FORMAT_RGB888;
        return true;
    case MPP_FMT_ARGB_8888:
        *lv_fmt = LV_COLOR_FORMAT_ARGB8888;
        return true;
    case MPP_FMT_XRGB_8888:
        *lv_fmt = LV_COLOR_FORMAT_XRGB8888;
        return true;
    /* BGR-family outputs have no distinct LVGL enum. Rejecting them keeps a
     * channel swap from being silently labeled as a valid format. */
    default:
        return false;
    }
}

bool lv_aic_pixel_format_is_ge2d_dst(lv_color_format_t lv_fmt)
{
    switch (lv_fmt) {
    case LV_COLOR_FORMAT_RGB565:
    case LV_COLOR_FORMAT_RGB888:
    case LV_COLOR_FORMAT_ARGB8888:
    case LV_COLOR_FORMAT_XRGB8888:
        return true;
    default:
        return false;
    }
}

#endif /* AIC_LVGL_BSP_MPP */
