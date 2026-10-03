/* SPDX-License-Identifier: Apache-2.0 */
#ifndef LVGL_AIC_FEATURE_CONFIG_H
#define LVGL_AIC_FEATURE_CONFIG_H
/* Load application Kconfig before SDK adapter feature guards. Explicit source
 * inclusion also makes the dependency visible to the SDK SCons scanner. */
#if defined(AIC_LVGL_BSP_RTTHREAD)
#include "lvgl_aic_target_config.h"
#endif
#endif
