/**
 * @file lv_aic_indev.h
 * @brief Internal ArtInChip input backend interface.
 */

#ifndef LV_AIC_INDEV_H
#define LV_AIC_INDEV_H

#include "lvgl_aic.h"
#include "lvgl_aic_compat.h"

#ifdef __cplusplus
extern "C" {
#endif

int lv_aic_indev_init(lv_display_t *display, lv_indev_t **indev);
void lv_aic_indev_deinit(lv_indev_t *indev);

#ifdef __cplusplus
}
#endif

#endif /* LV_AIC_INDEV_H */
