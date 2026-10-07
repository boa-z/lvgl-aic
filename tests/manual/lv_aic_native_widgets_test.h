/* SPDX-License-Identifier: Apache-2.0 */
#ifndef LV_AIC_NATIVE_WIDGETS_TEST_H
#define LV_AIC_NATIVE_WIDGETS_TEST_H
#include "lvgl_aic.h"
#if defined(AIC_LVGL_USE_IMG_ROLLER) && AIC_LVGL_USE_IMG_ROLLER && \
    defined(AIC_LVGL_USE_SWIPE_V1) && AIC_LVGL_USE_SWIPE_V1 && \
    LV_USE_ANIMIMG && LV_USE_IMAGEBUTTON && LV_USE_SPINNER && LV_USE_SCALE && LV_USE_SPAN && LV_USE_WIN
#define LV_AIC_NATIVE_WIDGET_TEST 1
#else
#define LV_AIC_NATIVE_WIDGET_TEST 0
#endif
#if LV_AIC_NATIVE_WIDGET_TEST
lv_obj_t *lv_aic_native_widgets_create(lv_obj_t *parent, int32_t width, int32_t height);
void lv_aic_native_widgets_show(lv_obj_t *page, bool visible);
#endif
#endif
