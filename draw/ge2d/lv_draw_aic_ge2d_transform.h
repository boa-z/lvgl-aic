/* SPDX-License-Identifier: Apache-2.0 */
#ifndef LV_DRAW_AIC_GE2D_TRANSFORM_H
#define LV_DRAW_AIC_GE2D_TRANSFORM_H
#include "lvgl_aic.h"

typedef struct {
    int32_t padded_w, padded_h, scaled_w, scaled_h;
    int32_t crop_x, crop_y, phase_x, phase_y, step_x, step_y;
    lv_point_t pivot;
    uint32_t padded_stride, scaled_stride, padded_bytes, scaled_bytes;
} lv_aic_ge2d_transform_plan_t;

/* Two transparent pixels surround both input and output of the scaler. Scale onto
 * an integer-pivot grid; explicit source phases retain fractional pivots.
 * budget covers BOTH simultaneously owned ARGB buffers, including padding. */
bool lv_aic_ge2d_transform_plan(uint32_t w, uint32_t h,
                                const lv_draw_image_dsc_t *dsc, uint32_t budget,
                                lv_aic_ge2d_transform_plan_t *plan);
#endif
