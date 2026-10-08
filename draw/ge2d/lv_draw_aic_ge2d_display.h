/* SPDX-License-Identifier: Apache-2.0 */
#ifndef LV_DRAW_AIC_GE2D_DISPLAY_H
#define LV_DRAW_AIC_GE2D_DISPLAY_H
#include "lvgl_aic.h"
/* 1 = synchronous hardware completion; 0 = declined before submission;
 * -1 = hardware failure/quarantine: retain source and destination until reboot;
 * neither buffer may be reused, freed, CPU-retried or presented. */
int lv_draw_aic_ge2d_display_rotate(const lv_draw_buf_t *src, lv_draw_buf_t *dst,
                                   lv_display_rotation_t rotation);
/* Scale the whole of src into the dst rectangle (x, y, w, h) as a plain copy
 * (no blending), for the virtual-resolution display. Same return contract. */
int lv_draw_aic_ge2d_display_scale(const lv_draw_buf_t *src, lv_draw_buf_t *dst,
                                  int32_t x, int32_t y, int32_t w, int32_t h);
#endif
