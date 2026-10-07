/* SPDX-License-Identifier: Apache-2.0 */
#ifndef LV_AIC_CANVAS_H
#define LV_AIC_CANVAS_H
#include "lvgl.h"
#ifndef AIC_LVGL_USE_CANVAS
#define AIC_LVGL_USE_CANVAS 0
#endif
#ifdef __cplusplus
extern "C" {
#endif
#if AIC_LVGL_USE_CANVAS
#if !LV_USE_CANVAS
#error "AIC canvas requires LV_USE_CANVAS"
#endif
extern const lv_obj_class_t lv_aic_canvas_class;
/* UI-owner thread only. Owned ARGB8888 buffer; use native canvas pixel/layer
 * APIs, but do not replace its source/buffer through native setters. */
lv_obj_t *lv_aic_canvas_create(lv_obj_t *parent);
/* Default peak budget 4 MiB, includes old + new storage during replacement. */
bool lv_aic_canvas_set_budget(lv_obj_t *obj, uint32_t bytes);
/* Dimensions 1..4096; transparent initialization; failure keeps old contents. */
lv_result_t lv_aic_canvas_alloc_buffer(lv_obj_t *obj, int32_t w, int32_t h);
void lv_aic_canvas_draw_text(lv_obj_t *obj, int32_t x, int32_t y, int32_t max_w,
                             lv_draw_label_dsc_t *dsc, const char *text);
/* Clears the canvas, then centers unwrapped text as in the SDK helper. */
void lv_aic_canvas_draw_text_to_center(lv_obj_t *obj, lv_draw_label_dsc_t *dsc,
                                       const char *text);
#endif
#ifdef __cplusplus
}
#endif
#endif
