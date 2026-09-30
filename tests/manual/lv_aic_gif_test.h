/* SPDX-License-Identifier: Apache-2.0 */
#ifndef LV_AIC_GIF_TEST_H
#define LV_AIC_GIF_TEST_H
#include "lvgl_aic.h"
#if LV_USE_GIF
/* UI thread only. The source string must outlive the panel. */
int lv_aic_gif_test_show(const char *path);
void lv_aic_gif_test_delete(void);
#if AIC_LVGL_BSP_RTTHREAD
void lv_aic_gif_test_poll(void);
void lv_aic_gif_test_deinit(void);
#endif
#endif
#endif
