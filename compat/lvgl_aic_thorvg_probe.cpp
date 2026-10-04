/* SPDX-License-Identifier: Apache-2.0 */
#include "lvgl_aic_thorvg_config.h"
static_assert(LV_AIC_LV_CONF_MARKER == 0x4C56414Du, "wrong vector configuration");
static_assert(LV_USE_OS == LV_OS_CUSTOM, "vector must use the application OS bridge");
static_assert(LV_USE_VECTOR_GRAPHIC && LV_USE_THORVG && LV_USE_THORVG_INTERNAL &&
              LV_USE_MATRIX && LV_USE_FLOAT && LV_DRAW_SW_DRAW_UNIT_CNT == 1,
              "vector C++ configuration mismatch");
static_assert(LV_USE_STDLIB_SPRINTF != LV_STDLIB_RTTHREAD, "vector needs floating-point formatting");
static_assert(__cplusplus >= 201402L, "ThorVG requires C++14");
#if defined(THORVG_THREAD_SUPPORT) || defined(THORVG_LOTTIE_LOADER_SUPPORT) || defined(THORVG_SVG_LOADER_SUPPORT)
#error "Unexpected vector-only ThorVG thread/loader feature"
#endif
extern "C" const char lvgl_aic_thorvg_config_probe[] = "lvgl-aic-thorvg-config-ok";
