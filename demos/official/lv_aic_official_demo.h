/* SPDX-License-Identifier: Apache-2.0 */
/* Runner for the official SDK demos vendored verbatim under demos/official.
 * Official demos draw on lv_scr_act() and never tear down; the runner gives
 * each one a fresh screen, hides the host UI's top-layer overlays, records
 * the LVGL timers the demo creates, and on close deletes exactly those
 * timers before deleting the screen. No demo source is modified.
 * LVGL owner thread only. Demos target 1024x600 (see AIC_LVGL_VIRTUAL_RES). */
#ifndef LV_AIC_OFFICIAL_DEMO_H
#define LV_AIC_OFFICIAL_DEMO_H

#include "lvgl.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    const char *name;  /* shell name, e.g. "meter" */
    const char *sdk;   /* SDK source, e.g. "aic_demo/meter_demo" */
    void (*entry)(void);
} lv_aic_official_demo_t;

/* Demos built into this image (Kconfig AIC_LVGL_OFFICIAL_DEMO_*). */
size_t lv_aic_official_demo_count(void);
const lv_aic_official_demo_t *lv_aic_official_demo_get(size_t index);
const lv_aic_official_demo_t *lv_aic_official_demo_find(const char *name);

/* Show a demo, closing the active one first. 0 on success, -1 unknown name,
 * -2 out of memory. */
int lv_aic_official_demo_show(const char *name);
/* Delete the demo's timers and screen and restore the previous screen. */
void lv_aic_official_demo_close(void);
/* Active demo name or NULL; number of timers it owns. */
const char *lv_aic_official_demo_active(void);
uint32_t lv_aic_official_demo_timer_count(void);

#ifdef __cplusplus
}
#endif

#endif /* LV_AIC_OFFICIAL_DEMO_H */
