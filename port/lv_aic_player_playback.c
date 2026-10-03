/* SPDX-License-Identifier: Apache-2.0 */
#include "lvgl_aic_feature_config.h"
#if defined(AIC_LVGL_USE_PLAYER_SESSION) && AIC_LVGL_USE_PLAYER_SESSION
#include "lv_aic_player_playback.h"
#include "lv_aic_player_frames.h"
#include "lv_aic_player_events.h"
#include <aic_osal.h>
#include <string.h>
struct lv_aic_player_playback {
    aicos_mutex_t mutex;
    lv_aic_player_session_t session;
    lv_aic_player_allocator_t *allocator;
    lv_aic_player_frames_t *frames;
    lv_aic_player_events_t *events;
    lv_aic_playback_status_t status;
    lv_aic_playback_options_t options;
    char uri[128];
    bool closing, start_requested, pause_requested;
    int volume_requested;
};
/* UI-owned reservation; released only after worker and readers finish. */
static lv_aic_player_playback_t *active;
static void lock(lv_aic_player_playback_t *p) { aicos_mutex_take(p->mutex,AICOS_WAIT_FOREVER); }
static void unlock(lv_aic_player_playback_t *p) { aicos_mutex_give(p->mutex); }
static void fault(lv_aic_player_playback_t *p)
{
    lock(p); p->status.state=LV_AIC_PLAYBACK_FAULT; p->closing=true; unlock(p);
    lv_aic_player_frames_close(p->frames);
}
static void worker(void *argument)
{
    lv_aic_player_playback_t *p=argument;
    uint64_t pending=0;
    bool video_eos=false;
    if(!lv_aic_player_session_open(&p->session,p->uri)) fault(p);
    else {
        p->events=lv_aic_player_events_create();
        if(!p->events || !lv_aic_player_events_attach(p->events,&p->session) ||
           !lv_aic_player_session_allocator(&p->session,lv_aic_player_allocator_sdk(p->allocator),p->options.extra_frames)) fault(p);
        else {
            lock(p);
            p->status.has_video=p->session.info.has_video; p->status.has_audio=p->session.info.has_audio;
            p->status.seekable=p->session.info.seek_able; p->status.duration_us=p->session.info.duration;
            p->status.width=p->session.info.video_stream.width; p->status.height=p->session.info.video_stream.height;
            if(!p->closing) p->status.state=LV_AIC_PLAYBACK_READY;
            unlock(p);
        }
    }
    for(;;) {
        lock(p);
        bool closing=p->closing, start=p->start_requested, paused=p->pause_requested;
        int volume=p->volume_requested, applied=p->status.volume;
        unlock(p);
        if(!lv_aic_player_frames_drain(p->frames)) { fault(p); closing=true; }
        if(closing) {
            lv_aic_player_frames_close(p->frames);
            if(pending) {
                if(lv_aic_player_session_release(&p->session,pending)) pending=0;
                else fault(p);
            }
            if(!pending && lv_aic_player_frames_idle(p->frames) && lv_aic_player_session_close(&p->session) &&
               lv_aic_player_events_destroy(p->events)) {
                p->events=NULL;
                lock(p);
                if(p->status.state!=LV_AIC_PLAYBACK_FAULT) p->status.state=LV_AIC_PLAYBACK_CLOSED;
                p->status.finished=true; unlock(p);
                return; /* No more access: UI may now destroy storage. */
            }
            if(p->session.faulted) fault(p);
            aicos_msleep(5); continue;
        }
        lv_aic_player_event_snapshot_t events={0};
        if(!lv_aic_player_events_snapshot(p->events,&events) || events.registration_failed || events.format_failed) {
            fault(p); continue;
        }
        lock(p);
        p->status.sdk_terminal=events.terminal;
        if(p->status.has_audio && events.audio_valid) {
            p->status.position_us=events.audio_pts; p->status.position_valid=true;
        }
        unlock(p);
        if(volume>=0 && volume!=applied) {
            if(!lv_aic_player_session_volume(&p->session,volume)) { fault(p); continue; }
            lock(p); p->status.volume=volume; unlock(p);
        }
        if(!start) { aicos_msleep(5); continue; }
        if(!p->session.started && !lv_aic_player_session_start(&p->session)) { fault(p); continue; }
        /* PLAY_END may arrive synchronously inside get_frame on its last frame.
         * That frame was already submitted below before this next iteration. */
        if(events.terminal || (video_eos && !p->session.info.has_audio)) {
            lock(p); if(!p->closing) p->status.state=LV_AIC_PLAYBACK_TERMINAL; unlock(p);
            aicos_msleep(5); continue;
        }
        if(paused!=p->session.paused && !lv_aic_player_session_pause(&p->session,paused)) { fault(p); continue; }
        lock(p); if(!p->closing) p->status.state=paused?LV_AIC_PLAYBACK_PAUSED:LV_AIC_PLAYBACK_PLAYING; unlock(p);
        if(paused || video_eos || !p->session.info.has_video) { aicos_msleep(5); continue; }
        unsigned held=0;
        for(unsigned i=0;i<LV_AIC_PLAYER_LEASES;i++) held+=(p->session.held>>i)&1U;
        if(held>=p->options.extra_frames) { aicos_msleep(5); continue; }
        const struct mpp_frame *frame;
        if(!lv_aic_player_session_acquire(&p->session,&pending,&frame)) {
            if(p->session.faulted) fault(p);
            aicos_msleep(5); continue;
        }
        /* SDK external-render get_buffer owns PTS delay/drop and audio clock
         * synchronization. Publish the returned frame without a second clock. */
        lock(p);
        bool discard=p->closing;
        p->status.frames_received++;
        if(!p->status.has_audio) { p->status.position_us=frame->pts; p->status.position_valid=true; }
        video_eos=(frame->flags&FRAME_FLAG_EOS)!=0; p->status.video_eos=video_eos;
        unlock(p);
        if(discard) continue; /* closing branch returns pending safely */
        if(!lv_aic_player_frames_submit(p->frames,pending)) {
            lock(p); bool cancelled=p->closing; unlock(p);
            if(!cancelled) fault(p);
            continue;
        }
        pending=0;
        lock(p); p->status.frames_queued++; unlock(p);
    }
}
lv_aic_player_playback_t *lv_aic_player_playback_prepare(const char *uri,const lv_aic_playback_options_t *options)
{
    if(active || !uri || !uri[0] || strlen(uri)>=128 || !options || !options->cma_budget ||
       options->extra_frames<2 || options->extra_frames>LV_AIC_PLAYER_LEASES ||
       options->color_space<LV_AIC_YUV_BT601_LIMITED || options->color_space>LV_AIC_YUV_BT709_FULL) return NULL;
    lv_aic_player_playback_t *p=lv_malloc_zeroed(sizeof(*p)); if(!p) return NULL;
    p->mutex=aicos_mutex_create();
    if(!p->mutex) { lv_free(p); return NULL; }
    p->options=*options; memcpy(p->uri,uri,strlen(uri)+1);
    p->status.state=LV_AIC_PLAYBACK_OPENING; p->status.volume=p->volume_requested=-1;
    p->allocator=lv_aic_player_allocator_create(options->cma_budget);
    if(p->allocator) p->frames=lv_aic_player_frames_create(&p->session,p->allocator,options->color_space);
    if(!p->frames) {
        (void)lv_aic_player_allocator_destroy(p->allocator); aicos_mutex_delete(p->mutex); lv_free(p); return NULL;
    }
    active=p;
    if(!aicos_thread_create("aic_playback",8192,20,worker,p)) {
        active=NULL; lv_aic_player_frames_close(p->frames); (void)lv_aic_player_frames_destroy(p->frames);
        (void)lv_aic_player_allocator_destroy(p->allocator); aicos_mutex_delete(p->mutex); lv_free(p); return NULL;
    }
    return p;
}
bool lv_aic_player_playback_start(lv_aic_player_playback_t *p)
{
    if(!p) return false;
    lock(p); bool accepted=!p->closing && !p->status.finished && p->status.state!=LV_AIC_PLAYBACK_TERMINAL;
    if(accepted) p->start_requested=true;
    unlock(p); return accepted;
}
bool lv_aic_player_playback_pause(lv_aic_player_playback_t *p,bool paused)
{
    if(!p) return false;
    lock(p); bool accepted=!p->closing && !p->status.finished && p->status.state!=LV_AIC_PLAYBACK_TERMINAL;
    if(accepted) p->pause_requested=paused;
    unlock(p); return accepted;
}
bool lv_aic_player_playback_volume(lv_aic_player_playback_t *p,int volume)
{
    if(!p || volume<0 || volume>100) return false;
    lock(p); bool accepted=!p->closing && !p->status.finished;
    if(accepted) p->volume_requested=volume;
    unlock(p); return accepted;
}
bool lv_aic_player_playback_poll(lv_aic_player_playback_t *p,lv_aic_player_image_t *image)
{
    if(!p) return false;
    lock(p); bool allowed=!p->closing && !p->status.finished &&
        (!p->pause_requested || p->status.state==LV_AIC_PLAYBACK_TERMINAL); unlock(p);
    return allowed && lv_aic_player_frames_poll_image(p->frames,image);
}
lv_aic_playback_status_t lv_aic_player_playback_status(lv_aic_player_playback_t *p)
{
    if(!p) return (lv_aic_playback_status_t){.state=LV_AIC_PLAYBACK_CLOSED,.finished=true,.volume=-1};
    lock(p); lv_aic_playback_status_t status=p->status; unlock(p); return status;
}
void lv_aic_player_playback_close(lv_aic_player_playback_t *p)
{
    if(!p) return;
    lock(p); p->closing=true;
    if(!p->status.finished && p->status.state!=LV_AIC_PLAYBACK_FAULT) p->status.state=LV_AIC_PLAYBACK_CLOSING;
    unlock(p); lv_aic_player_frames_close(p->frames);
}
bool lv_aic_player_playback_destroy(lv_aic_player_playback_t *p)
{
    if(!p) return true;
    if(!lv_aic_player_playback_status(p).finished || !lv_aic_player_frames_idle(p->frames)) return false;
    if(!lv_aic_player_allocator_destroy(p->allocator)) return false;
    p->allocator=NULL;
    if(!lv_aic_player_frames_destroy(p->frames)) return false;
    aicos_mutex_delete(p->mutex); active=NULL; lv_free(p); return true;
}
#endif
