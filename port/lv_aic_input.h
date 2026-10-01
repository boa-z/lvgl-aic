/* SPDX-License-Identifier: Apache-2.0 */
#ifndef LV_AIC_INPUT_H
#define LV_AIC_INPUT_H

#include "lvgl_aic.h"

int lv_aic_input_init(lv_display_t * display, lv_indev_type_t type,
                      const lv_aic_input_provider_t * provider,
                      lv_indev_t ** out);
void lv_aic_input_deinit(lv_indev_t * indev);

#endif
