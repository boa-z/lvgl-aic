/* SPDX-License-Identifier: Apache-2.0 */
#if defined(AIC_LVGL_USE_PLAYER_SESSION) && AIC_LVGL_USE_PLAYER_SESSION
#ifndef AIC_MPP_PLAYER_VIDEO_EXT_RENDER
#error "Player frame ownership requires SDK external video rendering"
#endif
#include "lv_aic_player_session.h"
#include <string.h>
static bool usable(const lv_aic_player_session_t *s)
{ return s && s->player && !s->faulted && !s->quarantined; }
static bool prepare(lv_aic_player_session_t *s)
{
    struct av_media_info info={0};
    if(aic_player_set_uri(s->player,s->uri) || aic_player_prepare_sync(s->player) ||
       aic_player_get_media_info(s->player,&info) || (!info.has_video && !info.has_audio) ||
       (info.has_video && (info.video_stream.width<1 || info.video_stream.height<1 ||
        info.video_stream.width>4096 || info.video_stream.height>4096 ||
        (uint64_t)info.video_stream.width*info.video_stream.height>8U*1024U*1024U))) {
        s->faulted=true; return false;
    }
    s->info=info; s->prepared=true; return true;
}
bool lv_aic_player_session_open(lv_aic_player_session_t *s,const char *uri)
{
    if(!s || s->player || s->held || s->quarantined || !uri || !uri[0] || strlen(uri)>=sizeof(s->uri)) return false;
    uint64_t ticket=s->next_ticket;
    memset(s,0,sizeof(*s)); s->next_ticket=ticket; memcpy(s->uri,uri,strlen(uri)+1);
    s->player=aic_player_create(NULL);
    if(!s->player) return false;
    if(!prepare(s)) { (void)lv_aic_player_session_close(s); return false; }
    return true;
}
bool lv_aic_player_session_allocator(lv_aic_player_session_t *s,
    struct frame_allocator *allocator,unsigned extra_frames)
{
    if(!usable(s) || s->started || s->held || !allocator || !extra_frames ||
       extra_frames>LV_AIC_PLAYER_LEASES) return false;
    s32 count=(s32)extra_frames;
    if(aic_player_control(s->player,AIC_PLAYER_CMD_SET_VDEC_EXT_FRAME_ALLOCATOR,allocator) ||
       aic_player_control(s->player,AIC_PLAYER_CMD_SET_VDEC_EXT_FRAME_NUM,&count)) {
        s->faulted=true; return false;
    }
    return true;
}
bool lv_aic_player_session_start(lv_aic_player_session_t *s)
{
    if(!usable(s)) return false;
    if(s->started) return !s->paused;
    if(!s->prepared && !prepare(s)) return false; /* Stop destroyed the decoder. */
    s->started=true; /* Failed start may have partially constructed components. */
    if(aic_player_start(s->player)) { s->faulted=true; return false; }
    return true;
}
bool lv_aic_player_session_pause(lv_aic_player_session_t *s,bool paused)
{
    if(!usable(s) || !s->started) return false;
    if(s->paused==paused) return true; /* SDK pause toggles when already paused. */
    if(paused ? aic_player_pause(s->player) : aic_player_play(s->player)) {
        s->faulted=true; return false;
    }
    s->paused=paused; return true;
}
bool lv_aic_player_session_acquire(lv_aic_player_session_t *s,uint64_t *lease,const struct mpp_frame **frame)
{
    if(!usable(s) || !s->started || s->paused || !s->info.has_video || !lease || !frame) return false;
    unsigned slot=0;
    while(slot<LV_AIC_PLAYER_LEASES && (s->held&(1U<<slot))) slot++;
    if(slot==LV_AIC_PLAYER_LEASES || s->next_ticket==UINT64_MAX) return false;
    struct mpp_frame next={0};
    if(aic_player_get_frame(s->player,&next)) return false; /* Timeout/no frame. */
    for(unsigned i=0;i<LV_AIC_PLAYER_LEASES;i++) {
        if((s->held&(1U<<i)) && s->frames[i].id==next.id) {
            /* Returning either copy could recycle pixels still held by a
             * reader. No guessed decrement/requeue can repair this contract. */
            s->quarantined=s->faulted=true; return false;
        }
    }
    s->frames[slot]=next; s->held|=1U<<slot;
    s->tickets[slot]=++s->next_ticket;
    *lease=s->tickets[slot]; *frame=&s->frames[slot]; return true;
}
bool lv_aic_player_session_release(lv_aic_player_session_t *s,uint64_t lease)
{
    if(!s || !s->player || s->quarantined || !lease) return false;
    unsigned slot=0;
    while(slot<LV_AIC_PLAYER_LEASES && (!(s->held&(1U<<slot)) || s->tickets[slot]!=lease)) slot++;
    if(slot==LV_AIC_PLAYER_LEASES) return false;
    if(aic_player_put_frame(s->player,&s->frames[slot])) { s->faulted=true; return false; }
    s->held&=~(1U<<slot); return true;
}
bool lv_aic_player_session_seek(lv_aic_player_session_t *s,uint64_t us)
{
    if(!usable(s) || !s->prepared || s->held || !s->info.seek_able ||
       us>INT64_MAX || (s->info.duration>0 && us>(uint64_t)s->info.duration)) return false;
    if(aic_player_seek(s->player,us)) { s->faulted=true; return false; }
    return true;
}
bool lv_aic_player_session_volume(lv_aic_player_session_t *s,int volume)
{
    return usable(s) && volume>=0 && volume<=100 && !aic_player_set_volum(s->player,volume);
}
bool lv_aic_player_session_get_volume(lv_aic_player_session_t *s,int *volume)
{
    s32 next;
    if(!usable(s) || !volume || aic_player_get_volum(s->player,&next) || next<0 || next>100) return false;
    *volume=next; return true;
}
bool lv_aic_player_session_time(lv_aic_player_session_t *s,int64_t *us)
{
    if(!usable(s) || !s->started || !us) return false;
    s64 next=aic_player_get_play_time(s->player);
    if(next<0) return false;
    *us=next; return true;
}
bool lv_aic_player_session_stop(lv_aic_player_session_t *s)
{
    if(!s || s->held || s->quarantined) return false;
    if(!s->player) return true;
    if(aic_player_stop(s->player)) { s->faulted=true; return false; }
    s->prepared=s->started=s->paused=false; return true;
}
bool lv_aic_player_session_close(lv_aic_player_session_t *s)
{
    if(!s || !lv_aic_player_session_stop(s)) return false;
    if(s->player && aic_player_destroy(s->player)) { s->faulted=true; return false; }
    uint64_t ticket=s->next_ticket;
    memset(s,0,sizeof(*s)); s->next_ticket=ticket; return true;
}
#endif
