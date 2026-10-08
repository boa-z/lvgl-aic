/* SPDX-License-Identifier: Apache-2.0 */
/* Virtual-resolution geometry: LVGL renders a fixed design resolution (e.g.
 * the 1024x600 SDK demos) and the display scales it, aspect preserved, into
 * the panel. Pure integer math, no OS or LVGL state, so host tests cover it. */
#ifndef LV_AIC_DISPLAY_FIT_H
#define LV_AIC_DISPLAY_FIT_H

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    int32_t x, y; /* top-left of the scaled image on the panel */
    int32_t w, h; /* scaled size; the rest of the panel is a black border */
} lv_aic_display_fit_t;

/* Uniform fit of vw x vh into pw x ph, centered, rounded to whole pixels.
 * Returns false for non-positive sizes. */
bool lv_aic_display_fit(int32_t vw, int32_t vh, int32_t pw, int32_t ph,
                        lv_aic_display_fit_t *fit);

/* Map a panel pixel to the virtual pixel it shows (pixel centers), clamped
 * to the virtual area; points on the border snap to the nearest edge. */
void lv_aic_display_fit_map(const lv_aic_display_fit_t *fit, int32_t vw, int32_t vh,
                            int32_t *x, int32_t *y);

#ifdef __cplusplus
}
#endif

#endif /* LV_AIC_DISPLAY_FIT_H */
