/*
 * SPDX-License-Identifier: Apache-2.0
 *
 * Host-only LVGL configuration overlay for the official SDL demos.
 * The board configuration remains in ../../lv_conf.h; this file only enables
 * the upstream LVGL 9.6 demo sources for the interactive PC validation target.
 */

#ifndef LV_AIC_HOST_SDL_DEMOS_CONF_H
#define LV_AIC_HOST_SDL_DEMOS_CONF_H

#include "../../lv_conf.h"

#undef LV_BUILD_DEMOS
#define LV_BUILD_DEMOS 1

#undef LV_USE_DEMO_WIDGETS
#define LV_USE_DEMO_WIDGETS 1

#undef LV_USE_DEMO_BENCHMARK
#define LV_USE_DEMO_BENCHMARK 1

#undef LV_USE_DEMO_STRESS
#define LV_USE_DEMO_STRESS 1

#undef LV_USE_DEMO_MUSIC
#define LV_USE_DEMO_MUSIC 1

#undef LV_USE_DEMO_KEYPAD_AND_ENCODER
#define LV_USE_DEMO_KEYPAD_AND_ENCODER 1

/* The upstream widgets, benchmark, and music demos reference these fonts. */
#undef LV_FONT_MONTSERRAT_12
#define LV_FONT_MONTSERRAT_12 1
#undef LV_FONT_MONTSERRAT_14
#define LV_FONT_MONTSERRAT_14 1
#undef LV_FONT_MONTSERRAT_16
#define LV_FONT_MONTSERRAT_16 1
#undef LV_FONT_MONTSERRAT_18
#define LV_FONT_MONTSERRAT_18 1
#undef LV_FONT_MONTSERRAT_20
#define LV_FONT_MONTSERRAT_20 1
#undef LV_FONT_MONTSERRAT_22
#define LV_FONT_MONTSERRAT_22 1
#undef LV_FONT_MONTSERRAT_24
#define LV_FONT_MONTSERRAT_24 1
#undef LV_FONT_MONTSERRAT_26
#define LV_FONT_MONTSERRAT_26 1
#undef LV_FONT_MONTSERRAT_32
#define LV_FONT_MONTSERRAT_32 1

#endif /* LV_AIC_HOST_SDL_DEMOS_CONF_H */
