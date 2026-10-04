/* SPDX-License-Identifier: Apache-2.0 */
#ifndef LVGL_AIC_THORVG_CONFIG_H
#define LVGL_AIC_THORVG_CONFIG_H
/* This forced header can precede the global -include on C++ command lines. */
#if defined(KERNEL_RTTHREAD) || defined(__RTTHREAD__)
#include "lvgl_aic_build_config.h"
#endif
#ifndef LV_KCONFIG_IGNORE
#define LV_KCONFIG_IGNORE 1
#endif
/* Upstream config defines loader flags to zero, but its loader registry tests
 * them with #ifdef. Normalize only this component's vector-only C++ profile. */
#include "../../lvgl/src/libs/thorvg/config.h"
#if LV_USE_LOTTIE
#error "The application vector profile does not build Lottie/SVG loaders"
#endif
#undef THORVG_SVG_LOADER_SUPPORT
#undef THORVG_LOTTIE_LOADER_SUPPORT
#endif
