/* SPDX-License-Identifier: Apache-2.0 */
#ifndef LV_AIC_METER_TEST_H
#define LV_AIC_METER_TEST_H

/* Pulls lvgl_aic.h -> lv_conf.h so bare rtconfig bools are normalized
 * to 0/1 before the feature guard below is evaluated. */
#include "lv_aic_manual_test.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Runs on the LVGL owner thread from the manual-test timer. */
void lv_aic_meter_test_poll(void);
void lv_aic_meter_test_deinit(void);

#ifdef __cplusplus
}
#endif

#endif /* LV_AIC_METER_TEST_H */
