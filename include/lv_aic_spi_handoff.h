/* SPDX-License-Identifier: Apache-2.0 */
#ifndef LV_AIC_SPI_HANDOFF_H
#define LV_AIC_SPI_HANDOFF_H
#include "lv_aic_spi_session.h"
#ifdef __cplusplus
extern "C" {
#endif
typedef struct lv_aic_spi_handoff lv_aic_spi_handoff_t;
/* One producer (LVGL owner) and one worker. Borrows an idle session; all session
 * access belongs exclusively to run() until the worker is joined and handoff
 * closed. Does not create threads, wake them or call LVGL display APIs. */
lv_aic_spi_handoff_t *lv_aic_spi_handoff_create(lv_aic_spi_session_t *session);
/* Producer: one bounded slot. Snapshot descriptor, borrow immutable CPU source
 * until take() succeeds. Cookie is returned unchanged; no dereference. */
lv_aic_spi_result_t lv_aic_spi_handoff_put(lv_aic_spi_handoff_t *handoff,
    const lv_aic_spi_rgb565_frame_t *frame,unsigned degrees,void *cookie);
/* Worker: execute at most one frame, including checked DMA completion. false
 * means no queued job. Source is not referenced after publishing completion. */
bool lv_aic_spi_handoff_run(lv_aic_spi_handoff_t *handoff);
/* Producer: consume completion exactly once, including errors. OK means checked
 * transport completion, not visual acceptance. Then source may be reused even
 * on FAULT: DMA uses session-owned tx, never this CPU source. */
bool lv_aic_spi_handoff_take(lv_aic_spi_handoff_t *handoff,
    lv_aic_spi_result_t *result,void **cookie);
/* Producer stops admission permanently; queued work must still run/be consumed. */
void lv_aic_spi_handoff_stop(lv_aic_spi_handoff_t *handoff);
/* Producer only, after worker joined and completion consumed. Frees handoff
 * metadata only. Faulted session/tx/panel remain retained separately. */
bool lv_aic_spi_handoff_close(lv_aic_spi_handoff_t *handoff);
#ifdef __cplusplus
}
#endif
#endif
