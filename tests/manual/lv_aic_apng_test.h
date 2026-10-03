/* SPDX-License-Identifier: Apache-2.0 */
#ifndef LV_AIC_APNG_TEST_H
#define LV_AIC_APNG_TEST_H
#include "lv_aic_manual_test.h"
#if defined(AIC_LVGL_USE_APNG_WIDGET) && AIC_LVGL_USE_APNG_WIDGET
int lv_aic_apng_test_show(void);
void lv_aic_apng_test_delete(void);
void lv_aic_apng_test_poll(void);
void lv_aic_apng_test_deinit(void);
#endif
#endif
