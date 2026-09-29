/*
 * SPDX-License-Identifier: Apache-2.0
 *
 * Compile-time proof that the reviewed lvgl-aic configuration was selected.
 */

#include "lvgl_aic.h"

#if !defined(LV_AIC_LV_CONF_MARKER)
#error "LVGL-AIC target configuration was not included"
#endif

_Static_assert(LV_AIC_LV_CONF_MARKER == 0x4C56414Du,
               "unexpected lvgl-aic configuration marker");

const char lvgl_aic_config_probe[] = "lvgl-aic-config-ok";
