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
void lv_aic_display_deinit(lv_display_t *display);
void lv_aic_display_flush_count_reset(void);
uint32_t lv_aic_display_flush_count_get(void);
/* LVGL owner thread only. Caller releases the independent CMA copy.
 * Captures scanout memory after rotation, not a software re-render. */
int lv_aic_display_snapshot(lv_display_t *display, lv_draw_buf_t *copy, uint32_t *frame);
void lv_aic_display_snapshot_free(lv_draw_buf_t *copy);

#ifdef __cplusplus
}
#endif

#endif /* LV_AIC_DISPLAY_H */
