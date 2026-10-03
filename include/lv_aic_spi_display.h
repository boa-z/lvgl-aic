/* SPDX-License-Identifier: Apache-2.0 */
#ifndef LV_AIC_SPI_DISPLAY_H
#define LV_AIC_SPI_DISPLAY_H
#include "lv_aic_spi_worker.h"
#include "lvgl.h"
#ifdef __cplusplus
extern "C" {
#endif
typedef struct lv_aic_spi_display lv_aic_spi_display_t;
/* UI-thread API. Borrow idle session exclusively; own one full RGB565 draw
 * buffer and worker. Budget covers stride*height draw pixels (metadata and
 * allocator alignment overhead excluded), separately from session tx budget.
 * Source dimensions 1..4096, rotation 0/90/180/270; session defines output size.
 * Do not change display buffers/format/mode/rotation/callbacks after creation. */
lv_aic_spi_display_t *lv_aic_spi_display_create(lv_aic_spi_session_t *session,
    uint32_t width,uint32_t height,unsigned degrees,size_t pixel_budget,
    uint32_t stack_bytes,uint32_t priority);
/* Select one or two full draw buffers. Budget covers the sum of pixel spans.
 * Two buffers let LVGL render the next frame while the worker reads the previous
 * one. The transmit session still owns its separate serialized DMA buffer. */
lv_aic_spi_display_t *lv_aic_spi_display_create_buffered(lv_aic_spi_session_t *session,
    uint32_t width,uint32_t height,unsigned degrees,size_t pixel_budget,
    unsigned buffer_count,uint32_t stack_bytes,uint32_t priority);
lv_display_t *lv_aic_spi_display_get(lv_aic_spi_display_t *display);
/* Transport status, not visual acceptance. Initial value OK; updated per frame. */
lv_aic_spi_result_t lv_aic_spi_display_result(lv_aic_spi_display_t *display);
typedef struct {
    uint32_t accepted,completed,failed,rejected;
    uint32_t last_observed_ms,max_observed_ms;
    lv_aic_spi_result_t last_completion;
    bool pending,blit_owned,closing;
} lv_aic_spi_display_stats_t;
/* UI-owner snapshot, never consumes completion. Counters saturate at UINT32_MAX.
 * completed means checked transport OK, not panel acceptance. observed_ms spans
 * accepted submit to UI result collection (includes scheduling/poll delay), not
 * pure DMA time. Tick wrap is supported for frames shorter than one tick period.
 * Rejected counts failed flush/blit submissions, not claim/poll/close attempts. */
bool lv_aic_spi_display_stats(lv_aic_spi_display_t *display,lv_aic_spi_display_stats_t *stats);
/* Permanently claim this display for direct RGB565 blits. UI-owner thread only,
 * outside refresh/events. First call pauses LVGL; BUSY means prior LVGL frame
 * still owns its source, so retry. No automatic return to LVGL mode. */
lv_aic_spi_result_t lv_aic_spi_display_claim_blit(lv_aic_spi_display_t *display);
/* Same producer/UI-owner thread. Borrow immutable CPU-coherent source until
 * blit_take succeeds or display_close succeeds. One outstanding frame; errors
 * from packing/transport arrive asynchronously through take. No raw tx access. */
lv_aic_spi_result_t lv_aic_spi_display_blit(lv_aic_spi_display_t *display,
    const lv_aic_spi_rgb565_frame_t *frame,unsigned degrees);
bool lv_aic_spi_display_blit_take(lv_aic_spi_display_t *display,lv_aic_spi_result_t *result);
/* UI thread outside refresh/events: stops refresh/admission and consumes result.
 * Retry until true; no forced worker deletion. Session/panel/tx are borrowed,
 * close them separately after success (retain them on transport FAULT).
 * Direct lv_display_delete stops admission but retains this handle and pixels;
 * still call this close API to finish worker/resource cleanup. */
bool lv_aic_spi_display_close(lv_aic_spi_display_t *display);
#ifdef __cplusplus
}
#endif
#endif
