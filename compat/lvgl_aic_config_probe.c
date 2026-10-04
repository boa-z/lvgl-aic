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

#if defined(AIC_LVGL_BUILD_DEMO_MUSIC) && AIC_LVGL_BUILD_DEMO_MUSIC
_Static_assert(LV_BUILD_DEMOS && LV_USE_DEMO_MUSIC, "music demo config mismatch");
#endif

#if AIC_LVGL_USE_VECTOR
_Static_assert(LV_USE_VECTOR_GRAPHIC && LV_USE_THORVG && LV_USE_THORVG_INTERNAL &&
               LV_USE_MATRIX && LV_DRAW_SW_DRAW_UNIT_CNT == 1, "vector profile mismatch");
#endif

#if AIC_LVGL_USE_LOTTIE
_Static_assert(LV_USE_LOTTIE && LV_USE_CANVAS && AIC_LVGL_USE_VECTOR, "Lottie profile mismatch");
#endif

#if AIC_LVGL_USE_SVG
_Static_assert(LV_USE_SVG && LV_USE_VECTOR_GRAPHIC && AIC_LVGL_USE_VECTOR, "SVG profile mismatch");
#endif
