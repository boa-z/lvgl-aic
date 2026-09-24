/**
 * @file lv_aic_mpp_decoder.h
 * @brief ArtInChip MPP image decoder for LVGL 9.6 (Phase 2A: FILE JPEG/PNG).
 *
 * Boundary rules:
 * - GE2D must remain disabled; this decoder never includes GE2D headers.
 * - Only LVGL SW-consumable RGB outputs are produced (no YUV metadata hack).
 * - LVGL 9.6 private decoder structs are accessed only via
 *   compat/lvgl_aic_private.h inside lv_aic_mpp_decoder.c.
 * - Phase 2A supports LV_IMAGE_SRC_FILE (.jpg/.jpeg/.png) only.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef LV_AIC_MPP_DECODER_H
#define LV_AIC_MPP_DECODER_H

#include "lvgl_aic.h"
#include "lvgl_aic_compat.h"

#ifdef __cplusplus
extern "C" {
#endif

int lv_aic_mpp_decoder_init(lv_image_decoder_t **decoder);
void lv_aic_mpp_decoder_deinit(lv_image_decoder_t *decoder);

/** @brief Decode statistics for Phase 2 performance notes (no tuning yet). */
typedef struct {
    uint32_t decode_time_ms;
    uint32_t decoded_bytes;
    uint32_t cma_bytes;
    uint32_t width;
    uint32_t height;
    lv_color_format_t color_format;
} lv_aic_mpp_decode_stats_t;

#ifdef AIC_LVGL_USE_MPP_DEC
const lv_aic_mpp_decode_stats_t *lv_aic_mpp_decoder_last_stats(void);
#endif

#ifdef __cplusplus
}
#endif

#endif /* LV_AIC_MPP_DECODER_H */
