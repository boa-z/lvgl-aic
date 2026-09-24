/**
 * @file lv_aic_manual_test.h
 * @brief Optional platform-only manual test page.
 */

#ifndef LV_AIC_MANUAL_TEST_H
#define LV_AIC_MANUAL_TEST_H

#include "lvgl_aic.h"

#ifdef __cplusplus
extern "C" {
#endif

int lv_aic_manual_test_create(void);
void lv_aic_manual_test_deinit(void);

#ifdef __cplusplus
}
#endif

#endif /* LV_AIC_MANUAL_TEST_H */
