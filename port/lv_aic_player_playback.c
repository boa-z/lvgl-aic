/* SPDX-License-Identifier: Apache-2.0 */
#include "lvgl_aic_feature_config.h"
#if defined(AIC_LVGL_USE_PLAYER_SESSION) && AIC_LVGL_USE_PLAYER_SESSION
#include "lv_aic_player_playback.h"
#include "lv_aic_player_frames.h"
#include "lv_aic_player_events.h"
#include <aic_osal.h>
#include <string.h>
#include <stddef.h>
/* Fail compilation if the SDK changes its command payload ABI. */
_Static_assert(sizeof(lv_aic_media_info_t)==sizeof(struct av_media_info),"media info size");
#define CHECK_INFO_OFFSET(field) _Static_assert(offsetof(lv_aic_media_info_t,field)==offsetof(struct av_media_info,field),"media info " #field)
CHECK_INFO_OFFSET(file_size);
CHECK_INFO_OFFSET(duration);
CHECK_INFO_OFFSET(has_video);
CHECK_INFO_OFFSET(has_audio);
CHECK_INFO_OFFSET(seek_able);
CHECK_INFO_OFFSET(video_stream.width);
CHECK_INFO_OFFSET(video_stream.height);
CHECK_INFO_OFFSET(audio_stream.nb_channel);
CHECK_INFO_OFFSET(audio_stream.bits_per_sample);
CHECK_INFO_OFFSET(audio_stream.sample_rate);
#undef CHECK_INFO_OFFSET
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
    lock(p); p->status.state=LV_AIC_PLAYBACK_FAULT; p->status.seek_pending=false; p->closing=true; unlock(p);
    lv_aic_player_frames_close(p->frames);
}
static bool open_session(lv_aic_player_playback_t *p)
{
    if(!lv_aic_player_session_open(&p->session,p->uri)) return false;
    p->events=lv_aic_player_events_create();
    if(!p->events || !lv_aic_player_events_attach(p->events,&p->session) ||
       !lv_aic_player_session_allocator(&p->session,lv_aic_player_allocator_sdk(p->allocator),p->options.extra_frames)) return false;
    lock(p);
    p->status.has_video=p->session.info.has_video; p->status.has_audio=p->session.info.has_audio;
    p->status.seekable=p->session.info.seek_able; p->status.duration_us=p->session.info.duration;
    p->status.width=p->session.info.video_stream.width; p->status.height=p->session.info.video_stream.height;
    p->status.volume=-1;
    memcpy(&p->status.media_info,&p->session.info,sizeof(p->status.media_info));
    p->status.media_info_valid=true;
    if(!p->closing && !p->status.seek_pending) p->status.state=LV_AIC_PLAYBACK_READY;
    unlock(p); return true;
}
static void worker(void *argument)
{
    lv_aic_player_playback_t *p=argument;
    uint64_t pending=0;
    bool video_eos=false;
    if(!open_session(p)) fault(p);
    for(;;) {
        lock(p);
        bool closing=p->closing, start=p->start_requested, paused=p->pause_requested;
        int volume=p->volume_requested, applied=p->status.volume;
        bool seeking=p->status.seek_pending; uint64_t target=p->status.seek_target_us;
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
        if(seeking) {
            /* UI has closed publication. A blocked get must return normally;
             * retired native/GE readers and pending put retries own teardown. */
            if(pending) {
                if(lv_aic_player_session_release(&p->session,pending)) pending=0;
                else { fault(p); continue; }
            }
            if(!lv_aic_player_frames_idle(p->frames)) { aicos_msleep(5); continue; }
            if(!lv_aic_player_session_close(&p->session) || !lv_aic_player_events_destroy(p->events)) {
                fault(p); continue;
            }
            p->events=NULL;
            lock(p); bool cancelled=p->closing; unlock(p);
            if(cancelled) continue;
            if(!open_session(p) || !lv_aic_player_session_seek(&p->session,target) ||
               !lv_aic_player_frames_reopen(p->frames)) { fault(p); continue; }
            video_eos=false;
            lock(p);
            p->status.seek_pending=false; p->status.seeks_completed++;
            p->status.sdk_terminal=p->status.video_eos=p->status.position_valid=false;
            if(!p->closing) p->status.state=LV_AIC_PLAYBACK_READY;
            unlock(p); continue;
        }
        lv_aic_player_event_snapshot_t events={0};
        if(!lv_aic_player_events_snapshot(p->events,&events) || events.registration_failed || events.format_failed) {
            fault(p); continue;
        }
        lock(p);
        if(!p->status.seek_pending) p->status.sdk_terminal=events.terminal;
        if(!p->status.seek_pending && p->status.has_audio && events.audio_valid) {
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
        if(!pending && (events.terminal || (video_eos && !p->session.info.has_audio))) {
            lock(p); if(!p->closing && !p->status.seek_pending) p->status.state=LV_AIC_PLAYBACK_TERMINAL; unlock(p);
            aicos_msleep(5); continue;
        }
        if(paused!=p->session.paused && !lv_aic_player_session_pause(&p->session,paused)) { fault(p); continue; }
        lock(p); if(!p->closing && !p->status.seek_pending) p->status.state=paused?LV_AIC_PLAYBACK_PAUSED:LV_AIC_PLAYBACK_PLAYING; unlock(p);
        if(paused || (!pending && video_eos) || !p->session.info.has_video) { aicos_msleep(5); continue; }
        if(lv_aic_player_frames_blocked(p->frames)) { aicos_msleep(5);continue; }
        if(!pending) {
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
            bool discard=p->closing || p->status.seek_pending;
            p->status.frames_received++;
            if(!discard && !p->status.has_audio) { p->status.position_us=frame->pts; p->status.position_valid=true; }
            video_eos=(frame->flags&FRAME_FLAG_EOS)!=0; p->status.video_eos=video_eos;
            unlock(p);
            if(discard) continue; /* closing branch returns pending safely */
        }
        lv_aic_player_submit_result_t submitted=lv_aic_player_frames_submit_checked(p->frames,pending);
        if(submitted!=LV_AIC_PLAYER_SUBMIT_OK) {
            lock(p); bool cancelled=p->closing || p->status.seek_pending; unlock(p);
            if(!cancelled && submitted==LV_AIC_PLAYER_SUBMIT_FAILED) fault(p);
            else aicos_msleep(5);
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
bool lv_aic_player_playback_preserve(lv_aic_player_playback_t *p,bool enabled)
{
    if(!p) return false;
    lock(p);bool ok=!p->closing && !p->status.finished;
    if(ok) lv_aic_player_frames_preserve(p->frames,enabled);
    unlock(p);return ok;
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
bool lv_aic_player_playback_seek(lv_aic_player_playback_t *p,uint64_t target)
{
    if(!p || target>INT64_MAX) return false;
    lock(p);
    bool accepted=!p->closing && !p->status.finished && !p->status.seek_pending &&
        p->status.state!=LV_AIC_PLAYBACK_OPENING && p->status.seekable &&
        p->status.seeks_completed<UINT64_MAX &&
        (p->status.duration_us<=0 || target<=(uint64_t)p->status.duration_us);
    if(accepted) {
        p->status.seek_pending=true; p->status.seek_target_us=target;
        p->status.position_valid=false; p->status.state=LV_AIC_PLAYBACK_SEEKING;
        /* Same lock serializes worker observation with publication shutdown. */
        lv_aic_player_frames_close(p->frames);
    }
    unlock(p); return accepted;
}
bool lv_aic_player_playback_poll(lv_aic_player_playback_t *p,lv_aic_player_image_t *image)
{
    if(!p) return false;
    lock(p); bool allowed=!p->closing && !p->status.finished && !p->status.seek_pending &&
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
    lock(p); p->closing=true; p->status.seek_pending=false;
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
