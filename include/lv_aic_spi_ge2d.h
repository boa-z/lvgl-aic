/* SPDX-License-Identifier: Apache-2.0 */
#ifndef LV_AIC_SPI_GE2D_H
#define LV_AIC_SPI_GE2D_H
#include "lv_aic_spi_transfer.h"
#ifdef __cplusplus
extern "C" {
#endif
typedef struct lv_aic_spi_ge2d lv_aic_spi_ge2d_t;
/* SPI_SDK + GE2D + BSP_MPP. Dedicated CMDQ client, single worker ownership.
 * Owns two 64-byte-aligned CMA staging allocations. budget bounds their sum,
 * including padded rows; excludes metadata and SDK command-queue allocations.
 * Source dimensions may vary up to max_width/max_height; output is fixed.
 * Normal-mode GE is rejected (no independent-client arbitration contract).
 * Create/close and run must not race. No panel/bus ownership is established. */
lv_aic_spi_ge2d_t *lv_aic_spi_ge2d_create(uint32_t max_width,uint32_t max_height,
    uint32_t output_width,uint32_t output_height,size_t budget);
/* Copies borrowed source to private staging before GE reads it. Only after
 * successful bitblt/emit/sync and cache invalidation is output CPU-written.
 * Output is packed RGB565 with optional byte swap. Source/output must not overlap.
 * Scaled input/output axes must be >= 4 (SDK limit); unscaled rotation permits 1.
 * Scale is bounded to 1/16..16 per axis; vendor split-risk geometry is declined.
 * INVALID is pre-submission and leaves output unchanged; FAULT permanently retains both staging
 * allocations and client until reboot. Original source/output are NEVER GE DMA
 * targets and are reusable after any return. Filtering is SDK GE, not the CPU
 * nearest-neighbor oracle. No automatic CPU fallback on uncertain GE failure. */
lv_aic_spi_result_t lv_aic_spi_ge2d_convert(lv_aic_spi_ge2d_t *converter,
    const lv_aic_spi_rgb565_frame_t *source,uint8_t *output,size_t capacity,
    unsigned clockwise_degrees,bool swap_bytes);
/* Faulted converter cannot be closed/reset. BUSY protects synchronous reentry. */
lv_aic_spi_result_t lv_aic_spi_ge2d_close(lv_aic_spi_ge2d_t *converter);
#ifdef __cplusplus
}
#endif
#endif
