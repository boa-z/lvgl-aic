/* SPDX-License-Identifier: Apache-2.0 */
#ifndef LV_DRAW_AIC_GE2D_DISPLAY_H
#define LV_DRAW_AIC_GE2D_DISPLAY_H
#include "lvgl_aic.h"
/* 1 = synchronous hardware completion; 0 = declined before submission;
 * -1 = hardware failure, destination must not be presented or CPU-retried. */
int lv_draw_aic_ge2d_display_rotate(const lv_draw_buf_t *src, lv_draw_buf_t *dst,
                                   lv_display_rotation_t rotation);
#endif
