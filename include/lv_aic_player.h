/* SPDX-License-Identifier: Apache-2.0 */
#ifndef LV_AIC_PLAYER_H
#define LV_AIC_PLAYER_H
#include "lv_aic_player_playback.h"
#ifdef __cplusplus
extern "C" {
#endif
extern const lv_obj_class_t lv_aic_player_class;
typedef enum {
    LV_AIC_PLAYER_CLOSED, LV_AIC_PLAYER_OPENING, LV_AIC_PLAYER_READY,
    LV_AIC_PLAYER_PLAYING, LV_AIC_PLAYER_PAUSED, LV_AIC_PLAYER_TERMINAL,
    LV_AIC_PLAYER_STOPPING, LV_AIC_PLAYER_STOPPED, LV_AIC_PLAYER_FAULT
} lv_aic_player_state_t;
/* LVGL owner only, outside draw callbacks. Image subclass: use native image
 * scale/rotation/pivot/alignment APIs; do not call lv_image_set_src directly.
 * Initialize RGB and YUV decoders before
 * set_src. Explicit budget/extra-frame/colorimetry options are mandatory.
 * LV_RESULT_OK means request accepted, not media/device completion. */
lv_obj_t *lv_aic_player_create(lv_obj_t *parent);
lv_result_t lv_aic_player_configure(lv_obj_t *obj,const lv_aic_playback_options_t *options);
/* Native SDK URI (<=127 bytes). Replacing a live source closes it safely first;
 * latest queued source wins. New source is prepared but not auto-started.
 * start during replacement requests play once the new source is ready. */
lv_result_t lv_aic_player_set_src(lv_obj_t *obj,const char *uri);
lv_result_t lv_aic_player_start(lv_obj_t *obj);
lv_result_t lv_aic_player_stop(lv_obj_t *obj);
lv_result_t lv_aic_player_close(lv_obj_t *obj);
lv_result_t lv_aic_player_pause(lv_obj_t *obj);
lv_result_t lv_aic_player_resume(lv_obj_t *obj);
lv_result_t lv_aic_player_set_volume(lv_obj_t *obj,int volume);
lv_aic_player_state_t lv_aic_player_get_state(lv_obj_t *obj);
lv_aic_playback_status_t lv_aic_player_get_status(lv_obj_t *obj);
/* VALUE_CHANGED reports state/applied-volume changes. Handler may delete the
 * widget or queue a new source. TERMINAL is not proof of clean EOF. start on
 * TERMINAL/STOPPED/CLOSED reopens the saved URI; FAULT requires stop/close.
 * Deleted widgets retain cleanup timers until queued draws, image readers and
 * worker exit finish. Run timers until pending_cleanup==0 before lv_deinit;
 * also close/delete live widgets. Never force-release a GE-quarantined frame. */
unsigned lv_aic_player_pending_cleanup(void);
#ifdef __cplusplus
}
#endif
#endif
