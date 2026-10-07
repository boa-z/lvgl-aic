/* SPDX-License-Identifier: Apache-2.0 */
#ifndef LV_AIC_APNG_WIDGET_H
#define LV_AIC_APNG_WIDGET_H
#include "lv_aic_apng_playback.h"
#ifdef __cplusplus
extern "C" {
#endif
extern const lv_obj_class_t lv_aic_apng_class;
extern const lv_obj_class_t lv_aic_apng_slave_class;
/* Display-only image sharing, LVGL owner only. No decoder or pixel copy.
 * Attach/detach apply on an idle draw pass. NULL detaches; master deletion
 * clears links immediately, retaining old pixels until safe timer cleanup.
 * Normal image transforms are supported; never set the slave source directly.
 * pending_cleanup includes deleted masters and slaves. No cross-display
 * scanout synchronization is promised. */
lv_obj_t *lv_aic_apng_slave_create(lv_obj_t *parent);
lv_result_t lv_aic_apng_slave_set_master(lv_obj_t *slave,lv_obj_t *master);
lv_obj_t *lv_aic_apng_slave_get_master(lv_obj_t *slave);
/* LVGL owner only, outside draw callbacks. Native image subclass; use normal
 * transforms/alignment, never set its image source directly. Initialize the
 * RGB decoder and configure explicit budgets before set_src. No auto-start.
 * VALUE_CHANGED reports state/applied-rate/replay changes; handler may delete
 * or replace the widget. Pump LVGL timers through pending_cleanup()==0 after
 * deleting widgets before lv_deinit. GE-quarantined readers may defer forever. */
lv_obj_t *lv_aic_apng_create(lv_obj_t *parent);
lv_result_t lv_aic_apng_configure(lv_obj_t *obj,const lv_aic_apng_playback_options_t *options);
/* Native file path <=127 bytes. Latest replacement wins after safe cleanup.
 * A new source clears start/pause intent, retaining configured rate. */
lv_result_t lv_aic_apng_set_src(lv_obj_t *obj,const char *path);
/* Start after close reopens saved path; start at terminal replays it. */
lv_result_t lv_aic_apng_start(lv_obj_t *obj);
lv_result_t lv_aic_apng_pause(lv_obj_t *obj,bool paused);
lv_result_t lv_aic_apng_set_rate(lv_obj_t *obj,uint32_t numerator,uint32_t denominator);
lv_result_t lv_aic_apng_restart(lv_obj_t *obj);
/* Nonblocking close cancels queued replacement, keeps saved path for start. */
lv_result_t lv_aic_apng_close(lv_obj_t *obj);
lv_aic_apng_playback_status_t lv_aic_apng_get_status(lv_obj_t *obj);
unsigned lv_aic_apng_pending_cleanup(void);
#ifdef __cplusplus
}
#endif
#endif
