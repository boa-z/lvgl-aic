/**
 * @file lv_aic_pixel_format.h
 * @brief Shared LVGL <-> MPP pixel format mapping.
 *
 * This module belongs to neither consumer. Both the MPP image decoder and the
 * GE2D draw unit depend on it, so neither has to include the other and the
 * translation switch exists exactly once.
 *
 * The mapping here is the general one. Feature-specific acceptance policy
 * (which formats a decoder may request, which formats GE2D may write) stays
 * with the feature that owns it.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef LV_AIC_PIXEL_FORMAT_H
#define LV_AIC_PIXEL_FORMAT_H

#include "lvgl_aic.h"
#include "lvgl_aic_compat.h"

#if AIC_LVGL_BSP_MPP
#include <mpp_types.h>
#endif

#ifdef __cplusplus
extern "C" {
#endif

#if AIC_LVGL_BSP_MPP

/**
 * @brief Translate an LVGL color format into the equivalent MPP pixel format.
 * @param lv_fmt   LVGL color format.
 * @param mpp_fmt  Receives the MPP format on success.
 * @return true when a 1:1 mapping exists.
 */
bool lv_aic_pixel_format_to_mpp(lv_color_format_t lv_fmt,
                                enum mpp_pixel_format *mpp_fmt);

/**
 * @brief Translate an MPP pixel format into the equivalent LVGL color format.
 * @param mpp_fmt  MPP pixel format.
 * @param lv_fmt   Receives the LVGL format on success.
 * @return true when a 1:1 mapping exists. BGR-family formats are rejected
 *         rather than mislabeled as their RGB counterparts.
 */
bool lv_aic_pixel_format_from_mpp(enum mpp_pixel_format mpp_fmt,
                                  lv_color_format_t *lv_fmt);

/**
 * @brief True when GE2D can use the format as a destination buffer format.
 *
 * Phase 3A accepts the formats the D13x GE block writes without a conversion
 * step. Everything else must go to the software renderer.
 */
bool lv_aic_pixel_format_is_ge2d_dst(lv_color_format_t lv_fmt);

#endif /* AIC_LVGL_BSP_MPP */

#ifdef __cplusplus
}
#endif

#endif /* LV_AIC_PIXEL_FORMAT_H */
