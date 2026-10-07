/**
 * @file lv_aic_mpp_decoder.h
 * @brief ArtInChip MPP JPEG/PNG decoder with FILE/RAW sources and bounded cache.
 *
 * Boundary rules:
 * - This decoder never includes GE2D headers.
 * - Only LVGL SW-consumable RGB outputs are produced (no YUV metadata hack).
 * - LVGL 9.6 private decoder structs are accessed only via
 *   compat/lvgl_aic_private.h inside lv_aic_mpp_decoder.c.
 * - FILE extensions and borrowed RAW/RAW_ALPHA JPEG/PNG payloads are supported.
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
bool lv_aic_mpp_decoder_can_deinit(void);

/** @brief Decode statistics for Phase 2 performance notes (no tuning yet). */
typedef struct {
    uint32_t decode_time_ms;
    uint32_t decoded_bytes;
    uint32_t cma_bytes;
    uint32_t width;
    uint32_t height;
    lv_color_format_t color_format;
} lv_aic_mpp_decode_stats_t;

#if AIC_LVGL_USE_MPP_DEC
const lv_aic_mpp_decode_stats_t *lv_aic_mpp_decoder_last_stats(void);
#endif

/** @brief Cumulative CMA lifecycle counters for the decoder's own buffers.
 *
 * Counted at this wrapper's MEM_CMA alloc/free sites only; CMA held inside the
 * SDK MPP engine is not visible here. After closing all readers and dropping
 * cache, a run started with no live buffers must have current_cma_bytes == 0
 * and matching alloc_count/free_count. Cached pixels intentionally remain live. */
typedef struct {
    uint32_t current_cma_bytes;
    uint32_t peak_cma_bytes;
    uint32_t alloc_count;
    uint32_t free_count;
} lv_aic_mpp_cma_stats_t;

#if AIC_LVGL_USE_MPP_DEC
const lv_aic_mpp_cma_stats_t *lv_aic_mpp_cma_stats(void);
/* Reset interval counters, retaining current live bytes and baseline peak. */
void lv_aic_mpp_cma_stats_reset(void);
#endif

#ifdef __cplusplus
}
#endif

#endif /* LV_AIC_MPP_DECODER_H */
