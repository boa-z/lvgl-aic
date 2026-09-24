/**
 * @file lv_conf_kconfig_external.h
 * @brief Deliberately empty LVGL Kconfig bridge for the Luban-Lite integration.
 *
 * The Luban-Lite SCons glue points LVGL 9.6 at this header and supplies the
 * reviewed lv_conf.h through LV_CONF_PATH.  Keeping the bridge empty prevents
 * LVGL's upstream RT-Thread/Kconfig defaults from silently becoming a second
 * configuration source.
 */

#ifndef LV_AIC_CONF_KCONFIG_EXTERNAL_H
#define LV_AIC_CONF_KCONFIG_EXTERNAL_H

#endif /* LV_AIC_CONF_KCONFIG_EXTERNAL_H */
