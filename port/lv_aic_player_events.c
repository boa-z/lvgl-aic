/* SPDX-License-Identifier: Apache-2.0 */
#include "lvgl_aic_feature_config.h"
#if defined(AIC_LVGL_USE_PLAYER_SESSION) && AIC_LVGL_USE_PLAYER_SESSION
#include "lv_aic_player_events.h"
#include <aic_osal.h>
#include <aic_time.h>
#include <stdlib.h>
struct lv_aic_player_events {
    aicos_mutex_t mutex;
    lv_aic_player_session_t *session;
    lv_aic_player_event_snapshot_t snapshot;
};
static void lock(lv_aic_player_events_t *e) { aicos_mutex_take(e->mutex,AICOS_WAIT_FOREVER); }
static void unlock(lv_aic_player_events_t *e) { aicos_mutex_give(e->mutex); }
static s32 event(void *context,s32 kind,s32 high,s32 low)
{
    lv_aic_player_events_t *e=context;
    lock(e);
    switch(kind) {
    case AIC_PLAYER_EVENT_PLAY_END: e->snapshot.terminal=true; break;
    case AIC_PLAYER_EVENT_DEMUXER_FORMAT_DETECTED: e->snapshot.format_detected=true; break;
    case AIC_PLAYER_EVENT_DEMUXER_FORMAT_NOT_DETECTED: e->snapshot.format_failed=true; break;
    case AIC_PLAYER_EVENT_PLAY_TIME: {
        uint64_t bits=((uint64_t)(uint32_t)high<<32)|(uint32_t)low;
        e->snapshot.audio_pts=bits<=INT64_MAX?(int64_t)bits:-1-(int64_t)(UINT64_MAX-bits);
        e->snapshot.audio_sample_us=aic_get_time_us(); e->snapshot.audio_valid=true;
        break;
    }
    default: unlock(e); return 0;
    }
    if(e->snapshot.sequence<UINT64_MAX) e->snapshot.sequence++;
    unlock(e); return 0;
}
lv_aic_player_events_t *lv_aic_player_events_create(void)
{
    lv_aic_player_events_t *e=calloc(1,sizeof(*e));
    if(!e) return NULL;
    e->mutex=aicos_mutex_create();
    if(!e->mutex) { free(e); return NULL; }
    return e;
}
bool lv_aic_player_events_attach(lv_aic_player_events_t *e,lv_aic_player_session_t *s)
{
    if(!e || e->session || !s || !s->player || s->started || s->faulted || s->quarantined) return false;
    e->session=s; /* Retain storage even when registration fails partway. */
    if(aic_player_set_event_callback(s->player,e,event)) {
        lock(e); e->snapshot.registration_failed=true; unlock(e); return false;
    }
    return true;
}
bool lv_aic_player_events_snapshot(lv_aic_player_events_t *e,lv_aic_player_event_snapshot_t *snapshot)
{
    if(!e || !snapshot) return false;
    lock(e); *snapshot=e->snapshot; unlock(e); return true;
}
bool lv_aic_player_events_destroy(lv_aic_player_events_t *e)
{
    if(!e) return true;
    if(e->session && e->session->player) return false;
    aicos_mutex_delete(e->mutex); free(e); return true;
}
#endif
