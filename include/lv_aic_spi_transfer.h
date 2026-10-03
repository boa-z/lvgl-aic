/* SPDX-License-Identifier: Apache-2.0 */
#ifndef LV_AIC_SPI_TRANSFER_H
#define LV_AIC_SPI_TRANSFER_H
#include "lv_aic_spi_frame.h"
#ifdef __cplusplus
extern "C" {
#endif
typedef struct lv_aic_spi_transfer lv_aic_spi_transfer_t;
typedef enum {
    LV_AIC_SPI_OK, LV_AIC_SPI_INVALID, LV_AIC_SPI_BUSY, LV_AIC_SPI_FAULT
} lv_aic_spi_result_t;
typedef struct {
    /* start must clean/prepare DMA storage, then submit. true means accepted,
     * NOT completed. wait returns true only after DMA can no longer read it.
     * false from either callback is uncertain DMA and permanently faults the
     * session. Callbacks run synchronously on the caller's worker thread. */
    bool (*start)(void *context,const uint8_t *pixels,size_t bytes);
    bool (*wait)(void *context);
    void *context;
} lv_aic_spi_transfer_ops_t;
/* Caller supplies dedicated, DMA-suitable tx storage and exclusively owns the
 * transport. No bus/pin/panel/byte-order inference. Session borrows storage and
 * callback context until successful close; never mutate/reuse/free them while
 * live. width/height 1..4096. No allocation of pixel storage. Single owner worker;
 * reentrant callback calls return BUSY, but concurrent threads are unsupported.
 * SDK's void aic_spi_lcd_wait_completion is NOT a checked completion adapter. */
lv_aic_spi_transfer_t *lv_aic_spi_transfer_create(uint8_t *tx,size_t capacity,
    uint32_t width,uint32_t height,bool swap_bytes,const lv_aic_spi_transfer_ops_t *ops);
/* Wait previous transfer before repacking; input is borrowed only during call.
 * Uses CPU nearest resize/rotation from lv_aic_spi_pack_rgb565. */
lv_aic_spi_result_t lv_aic_spi_transfer_submit(lv_aic_spi_transfer_t *transfer,
    const lv_aic_spi_rgb565_frame_t *source,unsigned clockwise_degrees);
lv_aic_spi_result_t lv_aic_spi_transfer_drain(lv_aic_spi_transfer_t *transfer);
/* Frees only session metadata after confirmed completion. On BUSY/FAULT the
 * handle and borrowed pixels remain live; there is no force-close/reset API. */
lv_aic_spi_result_t lv_aic_spi_transfer_close(lv_aic_spi_transfer_t *transfer);
#ifdef __cplusplus
}
#endif
#endif
