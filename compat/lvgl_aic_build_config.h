/*
 * SPDX-License-Identifier: Apache-2.0
 *
 * Build-only configuration bridge for the Luban-Lite custom SCons layer.
 * SCons force-includes this header before every target translation unit.
 */

#ifndef LVGL_AIC_BUILD_CONFIG_H
#define LVGL_AIC_BUILD_CONFIG_H

#define LV_AIC_LVGL_CONFIG_HEADER "lvgl_aic_target_config.h"
#define LV_AIC_LVGL_KCONFIG_HEADER "lv_conf_kconfig_external.h"
#define LV_CONF_PATH LV_AIC_LVGL_CONFIG_HEADER
#define LV_CONF_KCONFIG_EXTERNAL_INCLUDE LV_AIC_LVGL_KCONFIG_HEADER

#endif /* LVGL_AIC_BUILD_CONFIG_H */
