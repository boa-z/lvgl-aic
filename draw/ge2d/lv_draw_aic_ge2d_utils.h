/**
 * @file lv_draw_aic_ge2d_utils.h
 * @brief GE2D acceptance helpers and destination cache preparation.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef LV_DRAW_AIC_GE2D_UTILS_H
#define LV_DRAW_AIC_GE2D_UTILS_H

#include "lvgl_aic.h"
#include "lvgl_aic_compat.h"

#if AIC_LVGL_USE_GE2D
#include "lvgl_aic_private.h"
#endif

#ifdef __cplusplus
extern "C" {
#endif

#if AIC_LVGL_USE_GE2D

/**
 * @brief True when the GE block can address the buffer.
 *
 * The D13x/G73x GE engine reaches memory through a fixed window; a buffer
 * below 0x40000000 is not reachable. This threshold is inherited from the
 * vendor-validated port instead of being re-derived from the register manual.
 */
bool lv_draw_aic_ge2d_buf_address_valid(const lv_draw_buf_t *draw_buf);

/**
 * @brief True when GE2D may use @p cf as a fill destination.
 */
bool lv_draw_aic_ge2d_dst_format_supported(lv_color_format_t cf);

/**
 * @brief Clean and invalidate the destination cache lines covering @p rel_area.
 *
 * @p rel_area is relative to the buffer origin. Lines are handled one row at a
 * time and aligned to CACHE_LINE_SIZE, matching the vendor port. Only the
 * region the GE engine is about to touch is prepared; the global LVGL
 * draw-buffer handlers are deliberately left untouched so the rest of LVGL
 * keeps its own cache policy.
 */
void lv_draw_aic_ge2d_prepare_dst_cache(const lv_draw_buf_t *draw_buf,
                                        const lv_area_t *rel_area);

#endif /* AIC_LVGL_USE_GE2D */

#ifdef __cplusplus
}
#endif

#endif /* LV_DRAW_AIC_GE2D_UTILS_H */
