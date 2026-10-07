/* SPDX-License-Identifier: Apache-2.0 */
#ifndef LV_AIC_SPI_WORKER_H
#define LV_AIC_SPI_WORKER_H
#include "lv_aic_spi_handoff.h"
#ifdef __cplusplus
extern "C" {
#endif
typedef struct lv_aic_spi_worker lv_aic_spi_worker_t;
/* RT-Thread OSAL worker, borrowing an idle session exclusively. Explicit stack
 * bytes and scheduler priority; no panel/device defaults. Producer APIs below
 * all belong to one UI thread. Session/context survive until close succeeds. */
lv_aic_spi_worker_t *lv_aic_spi_worker_create(lv_aic_spi_session_t *session,
    uint32_t stack_bytes,uint32_t priority);
lv_aic_spi_result_t lv_aic_spi_worker_submit(lv_aic_spi_worker_t *worker,
    const lv_aic_spi_rgb565_frame_t *frame,unsigned degrees,void *cookie);
bool lv_aic_spi_worker_take(lv_aic_spi_worker_t *worker,lv_aic_spi_result_t *result,void **cookie);
/* Same consume, optionally returning worker stage timings. */
bool lv_aic_spi_worker_take_timed(lv_aic_spi_worker_t *worker,lv_aic_spi_result_t *result,
    void **cookie,lv_aic_spi_timing_t *timing);
/* Stop admission, finish queued work, then relinquish all application resources.
 * Nonblocking. UI must consume any completion, then retry close until true.
 * No forced thread deletion. Faulted sessions/DMA storage remain retained. */
void lv_aic_spi_worker_stop(lv_aic_spi_worker_t *worker);
bool lv_aic_spi_worker_close(lv_aic_spi_worker_t *worker);
#ifdef __cplusplus
}
#endif
#endif
