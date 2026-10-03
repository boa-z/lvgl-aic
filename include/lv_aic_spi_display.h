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
lv_display_t *lv_aic_spi_display_get(lv_aic_spi_display_t *display);
/* Transport status, not visual acceptance. Initial value OK; updated per frame. */
lv_aic_spi_result_t lv_aic_spi_display_result(lv_aic_spi_display_t *display);
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
