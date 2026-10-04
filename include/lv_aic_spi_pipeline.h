/* SPDX-License-Identifier: Apache-2.0 */
#ifndef LV_AIC_SPI_PIPELINE_H
#define LV_AIC_SPI_PIPELINE_H
#include "lv_aic_spi_handoff.h"
#ifdef __cplusplus
extern "C" {
#endif
typedef struct lv_aic_spi_pipeline lv_aic_spi_pipeline_t;
typedef enum {
    LV_AIC_SPI_SOURCE_RELEASED,
    LV_AIC_SPI_COMPLETED
} lv_aic_spi_event_kind_t;
typedef struct {
    lv_aic_spi_event_kind_t kind;
    void *cookie;
    lv_aic_spi_result_t result;
    lv_aic_spi_timing_t timing;
} lv_aic_spi_event_t;
/* Opt-in OSAL worker with two bounded slots. Borrows an idle session exclusively
 * until stop, consumption of ALL events, and successful close. Enable session
 * overlap before create for conversion/DMA overlap; a one-tx session is safe but
 * serializes conversion. No setup or direct session calls while worker lives.
 * Metadata only; stack/priority are explicit, no panel/bus/pixel allocation. */
lv_aic_spi_pipeline_t *lv_aic_spi_pipeline_create(lv_aic_spi_session_t *session,
    uint32_t stack_bytes,uint32_t priority);
/* Single producer/UI-owner thread for all APIs except internal worker. Snapshot
 * descriptor, borrow immutable CPU source until SOURCE_RELEASED (even on fault).
 * BUSY means both slots are held; consume events before retrying. Accepted frames
 * are never dropped. Each produces exactly one release and one completion. */
lv_aic_spi_result_t lv_aic_spi_pipeline_submit(lv_aic_spi_pipeline_t *pipeline,
    const lv_aic_spi_rgb565_frame_t *frame,unsigned degrees,void *cookie);
/* Nonblocking, consume one event. Release events are in submission order;
 * completion events are separately in submission order and always follow their
 * own release. A later release may precede an earlier completion. RELEASED/OK
 * means source reuse is safe, NOT completed DMA. Only COMPLETED/OK verifies SPI.
 * Cookie remains unchanged; keep any object it names alive through completion.
 * Timing.submit_ms covers that frame's conversion/submit (may wait prior DMA).
 * drain_ms covers an explicit final wait only; zero when verified by a successor.
 * Tick intervals include scheduling; they are not pure hardware timings. */
bool lv_aic_spi_pipeline_take(lv_aic_spi_pipeline_t *pipeline,lv_aic_spi_event_t *event);
/* Closes admission permanently. Worker processes accepted jobs and drains final
 * DMA, using a bounded 10 ms idle wait for successors even after failed wakes.
 * Fault/BUSY from the exclusively owned session halts further session calls and
 * reports queued work as FAULT. Session/DMA allocations remain retained on fault.
 * No forced thread deletion, replay or bus reset. Stop/close never close session.
 * Retry close after consuming events; only succeeds after worker relinquishes
 * all resources and both slots are empty. */
void lv_aic_spi_pipeline_stop(lv_aic_spi_pipeline_t *pipeline);
bool lv_aic_spi_pipeline_close(lv_aic_spi_pipeline_t *pipeline);
#ifdef __cplusplus
}
#endif
#endif
