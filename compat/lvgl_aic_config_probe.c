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

#if defined(AIC_LVGL_BUILD_DEMO_WIDGETS) && AIC_LVGL_BUILD_DEMO_WIDGETS
_Static_assert(LV_BUILD_DEMOS && LV_USE_DEMO_WIDGETS, "widgets demo config mismatch");
#endif
#if defined(AIC_LVGL_BUILD_DEMO_BENCHMARK) && AIC_LVGL_BUILD_DEMO_BENCHMARK
_Static_assert(LV_USE_DEMO_BENCHMARK && LV_USE_DEMO_WIDGETS && LV_USE_SYSMON &&
               LV_USE_PERF_MONITOR, "benchmark requires native measurements");
#endif
