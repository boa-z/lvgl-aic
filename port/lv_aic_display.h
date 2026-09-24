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

#ifdef __cplusplus
}
#endif

#endif /* LV_AIC_DISPLAY_H */
