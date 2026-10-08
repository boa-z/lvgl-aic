/* SPDX-License-Identifier: Apache-2.0 */
/* Compat entry for the official SDK demos. aic_ui_sdk.h is the SDK's
 * packages/artinchip/lvgl-ui/aic_ui.h, unmodified. Each demo is compiled with
 * -DAIC_OFFICIAL_DEMO_NAME=<name> (a bare token: string defines do not survive
 * the SCons/Windows command line reliably), which selects that demo's own
 * asset directory so several demos fit in one rodata image without their
 * identically named files colliding. */
#ifndef AIC_UI_COMPAT_H
#define AIC_UI_COMPAT_H

#if defined(AIC_OFFICIAL_DEMO_NAME)
#define AIC_OFFICIAL_DEMO_STR_(x) #x
#define AIC_OFFICIAL_DEMO_STR(x) AIC_OFFICIAL_DEMO_STR_(x)
/* The board rtconfig.h already defines LVGL_STORAGE_PATH (SDK Kconfig) for
 * the single-demo SDK layout; override it. The base matches the component
 * SConscript install destination rodata/lvgl_data/<name>. Adjacent literals
 * concatenate wherever the SDK macros use the path. */
#undef LVGL_STORAGE_PATH
#define LVGL_STORAGE_PATH "/rodata/lvgl_data/" AIC_OFFICIAL_DEMO_STR(AIC_OFFICIAL_DEMO_NAME)
#endif

#include "aic_ui_sdk.h"
/* Every official demo includes aic_ui.h after lvgl.h: the place to complete
 * the v8 API map without touching the vendored sources. */
#include "lv_aic_v8_compat.h"

#endif /* AIC_UI_COMPAT_H */
