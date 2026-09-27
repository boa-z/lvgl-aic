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

typedef struct {
    uint32_t irqs, reads, events, empty_reads, invalid_reads, deliveries, recovered;
    int32_t range_x, range_y;
    int16_t x, y;
    lv_indev_state_t state;
} lv_aic_touch_diagnostics_t;
/* Read from the LVGL owner thread, before deinitializing the input device. */
int lv_aic_indev_get_diagnostics(lv_indev_t *indev, lv_aic_touch_diagnostics_t *out);

#ifdef __cplusplus
}
#endif

#endif /* LV_AIC_INDEV_H */
