/**
 * @file lvgl_aic_target_config.h
 * @brief Stable target-build entry header for the reviewed LVGL configuration.
 *
 * The SCons glue passes this unique header name through LV_CONF_PATH.  Keeping
 * the path in a wrapper avoids Windows command-line quote stripping while still
 * selecting one explicit configuration file rather than an include-order
 * accident.
 */

#ifndef LV_AIC_TARGET_CONFIG_H
#define LV_AIC_TARGET_CONFIG_H

#include "../lv_conf.h"

#endif /* LV_AIC_TARGET_CONFIG_H */
