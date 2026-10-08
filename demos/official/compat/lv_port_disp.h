/* SPDX-License-Identifier: Apache-2.0 */
/* Compat for the official SDK demos: the SDK's lvgl_v9 lv_port_disp.h exposes
 * fbdev_draw_fps() over its own display driver. Here the application-owned
 * display port supplies the same number (frames presented in the last full
 * second). Only what the demos use is provided. */
#ifndef LV_PORT_DISP_H
#define LV_PORT_DISP_H

#include "lvgl.h"

#ifdef __cplusplus
extern "C" {
#endif

int lv_aic_display_fps(void);

static inline int fbdev_draw_fps(void)
{
    return lv_aic_display_fps();
}

#ifdef __cplusplus
}
#endif

#endif /* LV_PORT_DISP_H */
