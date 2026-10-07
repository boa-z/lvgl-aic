/* SPDX-License-Identifier: Apache-2.0 */
/*
 * Meter cluster demo adapted from
 * packages/artinchip/lvgl-ui/aic_demo/meter_demo/meter_ui.c for LVGL 9.6.
 *
 * Kept from the reference: widget tree, timer callbacks and periods,
 * rot_mode_list sweep, asset subpaths, meter_ui_init() entry.
 * Adapted: lv_image API, int32_t coordinates, uniform letterbox fit of the
 * 1024x600 design (meter_fit + image zoom), injected asset root / FPS
 * source instead of aic_ui.h / mpp_fb.h, built-in font instead of the
 * vendor generated ui_font_regular, dedicated test hooks.
 * Not yet ported: the 150-frame needle strip (pointer stays on the
 * reference LV_METER_SIMPLE_POINT rotation path; strip assets are vendored
 * under assets/point for phase 2 with scaled-rotation board validation).
 * ui_init() is intentionally omitted so several demos can link together;
 * meter_ui_init() is the entry.
 */
#ifndef METER_UI_H
#define METER_UI_H

#include "lvgl.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Return current FPS, or a negative value to hide the FPS label. */
typedef int (*meter_ui_fps_fn)(void);

typedef struct {
    /* e.g. "L:/rodata/lvgl_data". Empty/NULL selects built-in fallback
     * shapes and never touches the filesystem (host contracts). */
    const char *asset_root;
    /* NULL hides the FPS label. */
    meter_ui_fps_fn fps;
} meter_ui_config_t;

/* Must be called before meter_ui_init() when non-defaults are wanted. */
void meter_ui_configure(const meter_ui_config_t *config);
void meter_ui_init(void);
void meter_ui_destroy(void);

/* Test hooks: cumulative timer firings across all meter timers. */
uint32_t meter_ui_timer_fires(void);
/* Current speed digit 0..9, or -1 before init. */
int meter_ui_get_speed_step(void);
/* Needle angle in 0.1 degree units, matching lv_img_set_angle semantics. */
int32_t meter_ui_get_needle_angle(void);

#ifdef __cplusplus
}
#endif

#endif //METER_UI_H
