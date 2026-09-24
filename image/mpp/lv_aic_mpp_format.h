/**
 * @file lv_aic_mpp_format.h
 * @brief LVGL <-> MPP pixel format adapter (GE2D-independent).
 *
 * Phase 2 owns this mapping. It must not include any GE2D header.
 * Only RGB formats consumable by the LVGL software renderer are supported.
 * YUV outputs are rejected: the SW renderer cannot consume MPP metadata.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef LV_AIC_MPP_FORMAT_H
#define LV_AIC_MPP_FORMAT_H

#include "lvgl_aic.h"
#include "lvgl_aic_compat.h"

#if defined(AIC_LVGL_USE_MPP_DEC) && AIC_LVGL_BSP_MPP
#include <mpp_types.h>
#endif

#ifdef __cplusplus
extern "C" {
#endif

#if defined(AIC_LVGL_USE_MPP_DEC) && AIC_LVGL_BSP_MPP

/**
 * @brief Map an MPP output format to an LVGL SW-consumable color format.
 * @return true when the MPP format has a direct SW path.
 */
bool lv_aic_mpp_format_to_lvgl(enum mpp_pixel_format mpp_fmt,
                               lv_color_format_t *lv_fmt);

/**
 * @brief Map an LVGL color format to the MPP output format to request.
 * @return true when the LVGL format can be produced natively by MPP.
 */
bool lv_aic_mpp_format_from_lvgl(lv_color_format_t lv_fmt,
                                 enum mpp_pixel_format *mpp_fmt);

/** @brief True when an LVGL format carries alpha. */
bool lv_aic_mpp_format_lvgl_has_alpha(lv_color_format_t lv_fmt);

/** @brief Bytes per pixel for an SW-consumable LVGL format, 0 if unsupported. */
uint32_t lv_aic_mpp_format_lvgl_bpp(lv_color_format_t lv_fmt);

#endif /* AIC_LVGL_USE_MPP_DEC && AIC_LVGL_BSP_MPP */

#ifdef __cplusplus
}
#endif

#endif /* LV_AIC_MPP_FORMAT_H */
