/* SPDX-License-Identifier: Apache-2.0 */
#ifndef LV_AIC_PLAYER_CONTROL_H
#define LV_AIC_PLAYER_CONTROL_H
#include "lvgl.h"
#include "lv_aic_media_info.h"
#ifdef __cplusplus
extern "C" {
#endif
/* SDK command values/payload conventions. This checked adapter accepts either
 * native media-player or APNG widget. It is not the legacy void set_cmd ABI.
 * GET_MEDIA_INFO: lv_aic_media_info_t* (current SDK av_media_info layout).
 * ATTACH_GROUP is reserved/unsupported, with no writes.
 * SET/GET_VOLUME: int32_t*, SET/GET_PLAY_TIME: uint64_t*, PLAY_END: bool*,
 * ATTACH_SLAVE: slave object directly, SET/GET_PLAYBACK_RATE: float*.
 * Others ignore data. LVGL owner only, outside draw callbacks. */
typedef enum {
    LV_AIC_PLAYER_CMD_START,LV_AIC_PLAYER_CMD_STOP,LV_AIC_PLAYER_CMD_PAUSE,
    LV_AIC_PLAYER_CMD_RESUME,LV_AIC_PLAYER_CMD_PLAY_END,LV_AIC_PLAYER_CMD_GET_MEDIA_INFO,
    LV_AIC_PLAYER_CMD_SET_VOLUME,LV_AIC_PLAYER_CMD_GET_VOLUME,LV_AIC_PLAYER_CMD_SET_PLAY_TIME,
    LV_AIC_PLAYER_CMD_GET_PLAY_TIME,LV_AIC_PLAYER_CMD_ATTACH_SLAVE,LV_AIC_PLAYER_CMD_ATTACH_GROUP,
    LV_AIC_PLAYER_CMD_SET_PLAYBACK_RATE,LV_AIC_PLAYER_CMD_GET_PLAYBACK_RATE
} lv_aic_player_cmd_t;
/* OK means request accepted, not worker/device/display completion. Queries
 * return observed status and leave output untouched if unsupported/not ready.
 * Media rate is fixed 1x; SET rate is unsupported. APNG SET time accepts only
 * zero (replay), matching the SDK's animated PNG restriction. APNG has no
 * audio/position commands here. Float rate is rounded to 1e-5 then sent
 * as a reduced rational; NaN/Inf/out-of-range rates are rejected. */
lv_result_t lv_aic_player_control(lv_obj_t *obj,lv_aic_player_cmd_t command,void *data);
/* Prepared, active or terminal source only; refuse closing/replacement/fault
 * and leave output untouched. Queries never call the SDK or block on decode. */
lv_result_t lv_aic_player_get_media_info(lv_obj_t *obj,lv_aic_media_info_t *info);
#ifdef __cplusplus
}
#endif
#endif
