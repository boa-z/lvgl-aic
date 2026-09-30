/* SPDX-License-Identifier: Apache-2.0 */
#ifndef LV_AIC_FONT_TEST_H
#define LV_AIC_FONT_TEST_H
#include "lvgl_aic.h"
#if LV_USE_FREETYPE
int lv_aic_font_probe(const char *latin, const char *cjk);
int lv_aic_font_test_create(const char *latin, const char *cjk);
void lv_aic_font_test_delete(void);
#endif
#endif
