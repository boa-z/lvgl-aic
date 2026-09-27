/* SPDX-License-Identifier: Apache-2.0 */
#ifndef LV_DRAW_AIC_GE2D_SCALE_H
#define LV_DRAW_AIC_GE2D_SCALE_H
#include <stdbool.h>
#include <stdint.h>

/* One RGB axis, relative to the decoded image origin. No LVGL/SDK ABI here. */
typedef struct {
    int32_t crop;
    int32_t extent;
    int32_t step_16;
    int32_t phase_16;
} lv_aic_ge2d_scale_axis_t;

bool lv_aic_ge2d_scale_axis(int32_t source_size, int32_t dest_start,
                           int32_t dest_size, int32_t pivot, uint32_t scale,
                           lv_aic_ge2d_scale_axis_t *axis);
bool lv_aic_ge2d_scale_split_risk(int32_t step_16, int32_t dest_width);
#endif
