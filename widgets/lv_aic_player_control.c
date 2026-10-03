/* SPDX-License-Identifier: Apache-2.0 */
#include "lv_aic_player_control.h"
#if (defined(AIC_LVGL_USE_PLAYER) && AIC_LVGL_USE_PLAYER) || \
    (defined(AIC_LVGL_USE_APNG_WIDGET) && AIC_LVGL_USE_APNG_WIDGET)
#if defined(AIC_LVGL_USE_PLAYER) && AIC_LVGL_USE_PLAYER
#include "lv_aic_player.h"
#endif
#if defined(AIC_LVGL_USE_APNG_WIDGET) && AIC_LVGL_USE_APNG_WIDGET
#include "lv_aic_apng_widget.h"
#endif
#include <string.h>
#if defined(AIC_LVGL_USE_PLAYER) && AIC_LVGL_USE_PLAYER
static bool media_status(lv_obj_t *obj,lv_aic_playback_status_t *out)
{
    lv_aic_player_state_t state=lv_aic_player_get_state(obj);
    if(state!=LV_AIC_PLAYER_READY && state!=LV_AIC_PLAYER_PLAYING &&
       state!=LV_AIC_PLAYER_PAUSED && state!=LV_AIC_PLAYER_TERMINAL) return false;
    lv_aic_playback_status_t s=lv_aic_player_get_status(obj);
    if((s.state!=LV_AIC_PLAYBACK_READY && s.state!=LV_AIC_PLAYBACK_PLAYING &&
        s.state!=LV_AIC_PLAYBACK_PAUSED && s.state!=LV_AIC_PLAYBACK_TERMINAL) || s.seek_pending) return false;
    *out=s;return true;
}
#endif
lv_result_t lv_aic_player_get_media_info(lv_obj_t *obj,lv_aic_media_info_t *info)
{
    if(!obj || !info) return LV_RESULT_INVALID;
#if defined(AIC_LVGL_USE_APNG_WIDGET) && AIC_LVGL_USE_APNG_WIDGET
    if(lv_obj_check_type(obj,&lv_aic_apng_class)) {
        lv_aic_apng_playback_status_t s=lv_aic_apng_get_status(obj);
        if(s.state!=LV_AIC_APNG_READY && s.state!=LV_AIC_APNG_PLAYING &&
           s.state!=LV_AIC_APNG_PLAYBACK_PAUSED && s.state!=LV_AIC_APNG_TERMINAL) return LV_RESULT_INVALID;
        if(!s.width || !s.height || s.width>INT32_MAX || s.height>INT32_MAX || s.file_bytes>INT64_MAX) return LV_RESULT_INVALID;
        lv_aic_media_info_t value={.file_size=(int64_t)s.file_bytes,.has_video=1,
            .video_stream={(int32_t)s.width,(int32_t)s.height}};
        memcpy(info,&value,sizeof(value));return LV_RESULT_OK;
    }
#endif
#if defined(AIC_LVGL_USE_PLAYER) && AIC_LVGL_USE_PLAYER
    if(lv_obj_check_type(obj,&lv_aic_player_class)) {
        lv_aic_playback_status_t s;
        if(!media_status(obj,&s) || !s.media_info_valid) return LV_RESULT_INVALID;
        memcpy(info,&s.media_info,sizeof(*info));return LV_RESULT_OK;
    }
#endif
    return LV_RESULT_INVALID;
}
lv_result_t lv_aic_player_control(lv_obj_t *obj,lv_aic_player_cmd_t cmd,void *data)
{
    if(!obj) return LV_RESULT_INVALID;
    if(cmd==LV_AIC_PLAYER_CMD_GET_MEDIA_INFO) return lv_aic_player_get_media_info(obj,data);
#if defined(AIC_LVGL_USE_APNG_WIDGET) && AIC_LVGL_USE_APNG_WIDGET
    if(lv_obj_check_type(obj,&lv_aic_apng_class)) {
        switch(cmd) {
        case LV_AIC_PLAYER_CMD_START:return lv_aic_apng_start(obj);
        case LV_AIC_PLAYER_CMD_STOP:return lv_aic_apng_close(obj);
        case LV_AIC_PLAYER_CMD_PAUSE:return lv_aic_apng_pause(obj,true);
        case LV_AIC_PLAYER_CMD_RESUME:return lv_aic_apng_pause(obj,false);
        case LV_AIC_PLAYER_CMD_PLAY_END:
            if(!data) return LV_RESULT_INVALID;
            *(bool *)data=lv_aic_apng_get_status(obj).state==LV_AIC_APNG_TERMINAL;return LV_RESULT_OK;
        case LV_AIC_PLAYER_CMD_SET_PLAY_TIME:
            return data && !*(uint64_t *)data?lv_aic_apng_restart(obj):LV_RESULT_INVALID;
        case LV_AIC_PLAYER_CMD_ATTACH_SLAVE:
            return data?lv_aic_apng_slave_set_master(data,obj):LV_RESULT_INVALID;
        case LV_AIC_PLAYER_CMD_SET_PLAYBACK_RATE: {
            if(!data) return LV_RESULT_INVALID;
            float rate=*(float *)data;
            if(!(rate>=0.1f && rate<=10.0f)) return LV_RESULT_INVALID;
            uint32_t n=(uint32_t)(rate*100000.0f+0.5f),d=100000,a=n,b=d;
            while(b) { uint32_t r=a%b;a=b;b=r; }
            return lv_aic_apng_set_rate(obj,n/a,d/a);
        }
        case LV_AIC_PLAYER_CMD_GET_PLAYBACK_RATE: {
            lv_aic_apng_playback_status_t s=lv_aic_apng_get_status(obj);
            if(!data || !s.rate_den || !s.rate_num || (s.state!=LV_AIC_APNG_READY &&
               s.state!=LV_AIC_APNG_PLAYING && s.state!=LV_AIC_APNG_PLAYBACK_PAUSED &&
               s.state!=LV_AIC_APNG_TERMINAL)) return LV_RESULT_INVALID;
            *(float *)data=(float)s.rate_num/s.rate_den;return LV_RESULT_OK;
        }
        default:return LV_RESULT_INVALID;
        }
    }
#endif
#if defined(AIC_LVGL_USE_PLAYER) && AIC_LVGL_USE_PLAYER
    if(lv_obj_check_type(obj,&lv_aic_player_class)) {
        switch(cmd) {
        case LV_AIC_PLAYER_CMD_START:return lv_aic_player_start(obj);
        case LV_AIC_PLAYER_CMD_STOP:return lv_aic_player_stop(obj);
        case LV_AIC_PLAYER_CMD_PAUSE:return lv_aic_player_pause(obj);
        case LV_AIC_PLAYER_CMD_RESUME:return lv_aic_player_resume(obj);
        case LV_AIC_PLAYER_CMD_PLAY_END:
            if(!data) return LV_RESULT_INVALID;
            *(bool *)data=lv_aic_player_get_state(obj)==LV_AIC_PLAYER_TERMINAL;return LV_RESULT_OK;
        case LV_AIC_PLAYER_CMD_SET_VOLUME:
            return data?lv_aic_player_set_volume(obj,*(int32_t *)data):LV_RESULT_INVALID;
        case LV_AIC_PLAYER_CMD_GET_VOLUME: {
            lv_aic_playback_status_t s;
            if(!data || !media_status(obj,&s) || s.volume<0 || s.volume>100) return LV_RESULT_INVALID;
            *(int32_t *)data=s.volume;return LV_RESULT_OK;
        }
        case LV_AIC_PLAYER_CMD_SET_PLAY_TIME:
            return data?lv_aic_player_seek(obj,*(uint64_t *)data):LV_RESULT_INVALID;
        case LV_AIC_PLAYER_CMD_GET_PLAY_TIME: {
            lv_aic_playback_status_t s;
            if(!data || !media_status(obj,&s) || !s.position_valid || s.position_us<0) return LV_RESULT_INVALID;
            *(uint64_t *)data=(uint64_t)s.position_us;return LV_RESULT_OK;
        }
        case LV_AIC_PLAYER_CMD_ATTACH_SLAVE:
            return data?lv_aic_slave_player_set_master(data,obj):LV_RESULT_INVALID;
        case LV_AIC_PLAYER_CMD_ATTACH_GROUP:
            return lv_aic_player_set_group(obj,data);
        case LV_AIC_PLAYER_CMD_SET_PLAYBACK_RATE: {
            if(!data) return LV_RESULT_INVALID;
            float rate=*(float *)data;
            if(!(rate>=0.1f && rate<=10.0f)) return LV_RESULT_INVALID;
            uint32_t n=(uint32_t)(rate*100000.0f+0.5f),d=100000,a=n,b=d;
            while(b) { uint32_t r=a%b;a=b;b=r; }
            return lv_aic_player_set_rate(obj,n/a,d/a);
        }
        case LV_AIC_PLAYER_CMD_GET_PLAYBACK_RATE: {
            uint32_t n,d;
            if(!data || lv_aic_player_get_rate(obj,&n,&d)!=LV_RESULT_OK) return LV_RESULT_INVALID;
            *(float *)data=(float)n/d;return LV_RESULT_OK;
        }
        default:return LV_RESULT_INVALID;
        }
    }
#endif
    return LV_RESULT_INVALID;
}
#endif
