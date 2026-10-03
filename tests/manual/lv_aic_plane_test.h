/* SPDX-License-Identifier: Apache-2.0 */
#ifndef LV_AIC_PLANE_TEST_H
#define LV_AIC_PLANE_TEST_H
#include "lv_aic_manual_test.h"
#if AIC_LVGL_BSP_RTTHREAD && defined(AIC_LVGL_USE_PLAYER) && AIC_LVGL_USE_PLAYER && defined(AIC_LVGL_USE_APNG) && AIC_LVGL_USE_APNG && defined(AIC_LVGL_USE_VIDEO_PLANE) && AIC_LVGL_USE_VIDEO_PLANE
#define LV_AIC_PLANE_TEST_ENABLED 1
void lv_aic_plane_test_poll(void);
void lv_aic_plane_test_deinit(void);
#else
#define LV_AIC_PLANE_TEST_ENABLED 0
#endif
#endif
