/**
 * @file lv_aic_display.h
 * @brief Internal ArtInChip display backend interface.
 */

#ifndef LV_AIC_DISPLAY_H
#define LV_AIC_DISPLAY_H

#include "lvgl_aic.h"
#include "lvgl_aic_compat.h"

#ifdef __cplusplus
extern "C" {
#endif

int lv_aic_display_init(lv_display_t **display);
/* After any uncertain shared GE DMA, deinit retains display/buffers until reboot. */
void lv_aic_display_deinit(lv_display_t *display);
void lv_aic_display_flush_count_reset(void);
uint32_t lv_aic_display_flush_count_get(void);
/* LVGL owner thread only. Caller releases the independent CMA copy.
 * Captures scanout memory after rotation, not a software re-render. */
int lv_aic_display_snapshot(lv_display_t *display, lv_draw_buf_t *copy, uint32_t *frame);
void lv_aic_display_snapshot_free(lv_draw_buf_t *copy);
/* Panel (scanout) size; differs from the LVGL resolution in virtual mode. */
void lv_aic_display_panel_size(lv_display_t *display, int32_t *width, int32_t *height);
/* Map a panel point to LVGL coordinates (identity unless virtual mode). */
void lv_aic_display_panel_to_logical(lv_display_t *display, int32_t *x, int32_t *y);
/* Frames presented during the last full second (0 before the first). */
int lv_aic_display_fps(void);

#ifdef __cplusplus
}
#endif

#endif /* LV_AIC_DISPLAY_H */
