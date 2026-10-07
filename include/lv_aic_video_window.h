/* SPDX-License-Identifier: Apache-2.0 */
#ifndef LV_AIC_VIDEO_WINDOW_H
#define LV_AIC_VIDEO_WINDOW_H
#include "lvgl.h"
#ifndef AIC_LVGL_USE_VIDEO_WINDOW
#define AIC_LVGL_USE_VIDEO_WINDOW 0
#endif
#ifdef __cplusplus
extern "C" {
#endif
#if AIC_LVGL_USE_VIDEO_WINDOW
extern const lv_obj_class_t lv_aic_video_window_class;
/* SDK-compatible alpha-zero replacement window. Requires the MPP .fake
 * metadata bridge and AIC draw unit; no media device is opened. The output
 * layer must retain ARGB alpha for a lower video layer to show through. */
lv_obj_t *lv_aic_video_window_create(lv_obj_t *parent);
/* Dimensions 1..4096, total <= 8M pixels. Invalid sizes leave it unchanged. */
void lv_aic_video_window_set_size(lv_obj_t *obj, uint32_t width, uint32_t height);
void lv_aic_video_window_set_color(lv_obj_t *obj, lv_color_t color);
#endif
#ifdef __cplusplus
}
#endif
#endif
