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
/* Optional setup-only transform. Configure once before the first submit attempt.
 * Callback must synchronously finish output writes and must never expose caller
 * source/tx to uncertain DMA. FAULT latches transfer quarantine; other results
 * suppress transport submission. Context lives until successful transfer close.
 * An INVALID result must be pre-submission and leave output unchanged. */
typedef lv_aic_spi_result_t (*lv_aic_spi_transform_cb_t)(void *context,
    const lv_aic_spi_rgb565_frame_t *source,uint8_t *output,size_t capacity,
    uint32_t width,uint32_t height,unsigned degrees,bool swap_bytes);
bool lv_aic_spi_transfer_set_transform(lv_aic_spi_transfer_t *transfer,
    lv_aic_spi_transform_cb_t transform,void *context);
/* Optional second dedicated DMA buffer, configured once before any submit.
 * Full supplied capacity ranges must not overlap. Both buffers remain borrowed
 * until successful close (forever on FAULT). The transform can then run before
 * waiting for active transport, and must not access that transport or its active
 * buffer. INVALID/BUSY conversion leaves the previous transfer outstanding. */
bool lv_aic_spi_transfer_set_back_buffer(lv_aic_spi_transfer_t *transfer,
    uint8_t *pixels,size_t capacity);
/* Normally wait previous transfer before repacking. With a back buffer, convert
 * first, then wait previous DMA, then start the new transfer. Input is borrowed
 * only during this call.
 * Defaults to CPU nearest resize/rotation from lv_aic_spi_pack_rgb565. */
lv_aic_spi_result_t lv_aic_spi_transfer_submit(lv_aic_spi_transfer_t *transfer,
    const lv_aic_spi_rgb565_frame_t *source,unsigned clockwise_degrees);
/* Same submit, with an optional receipt for the PREVIOUS frame. Initialized to
 * false even on INVALID/BUSY/FAULT. True only if this call verified an outstanding
 * DMA completion; it stays true if the NEW frame subsequently fails. False does
 * not imply failure: conversion may reject the new frame before the wait. The
 * return value describes new submission, never completion of the new frame.
 * Receipt storage belongs to the caller, must not alias source/tx or be shared
 * with a reentrant callback, and must survive this synchronous call. */
lv_aic_spi_result_t lv_aic_spi_transfer_submit_ex(lv_aic_spi_transfer_t *transfer,
    const lv_aic_spi_rgb565_frame_t *source,unsigned clockwise_degrees,bool *previous_completed);
lv_aic_spi_result_t lv_aic_spi_transfer_drain(lv_aic_spi_transfer_t *transfer);
/* Frees only session metadata after confirmed completion. On BUSY/FAULT the
 * handle and borrowed pixels remain live; there is no force-close/reset API. */
lv_aic_spi_result_t lv_aic_spi_transfer_close(lv_aic_spi_transfer_t *transfer);
#ifdef __cplusplus
}
#endif
#endif
