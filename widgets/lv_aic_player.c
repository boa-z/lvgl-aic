/* SPDX-License-Identifier: Apache-2.0 */
#include "lv_aic_player.h"
#include "lv_aic_player_control.h"
#define AIC_LVGL_USE_PRIVATE_API 1
#include "lvgl_aic_private.h"
#if defined(AIC_LVGL_USE_PLAYER) && AIC_LVGL_USE_PLAYER
#include <string.h>
#include <limits.h>
#include <stdio.h>
#if defined(AIC_LVGL_USE_VIDEO_PLANE) && AIC_LVGL_USE_VIDEO_PLANE
#include "lv_aic_plane_window.h"
#endif
#if !LV_USE_IMAGE
#error "The AIC player widget requires LV_USE_IMAGE"
#endif
typedef struct player_binding player_binding_t;
typedef struct slave_binding slave_binding_t;
typedef struct player_group player_group_t;
/* One immutable image descriptor shared by widget owners; decoder/GE readers
 * independently retain its storage after the last widget owner releases. */
typedef struct { lv_aic_player_image_t image; size_t owners; } player_frame_t;
struct player_binding {
    lv_obj_t *obj;
    uint32_t requested_width,requested_height;
    lv_timer_t *timer;
    lv_aic_player_playback_t *playback;
#if defined(AIC_LVGL_USE_APNG) && AIC_LVGL_USE_APNG
    lv_aic_apng_playback_t *apng;
    lv_aic_apng_playback_options_t apng_options;
    bool apng_configured;
    uint32_t rate_num,rate_den;
#endif
    player_frame_t *frame;
#if defined(AIC_LVGL_USE_VIDEO_PLANE) && AIC_LVGL_USE_VIDEO_PLANE
    lv_aic_plane_window_t window;
    bool plane_enabled,plane_failed;
#endif
    slave_binding_t *slaves;
    player_group_t *group;
    player_binding_t *group_next;
    bool group_presented;
    lv_aic_playback_options_t options;
    lv_aic_playback_status_t status;
    lv_aic_player_state_t state,reported;
    char uri[128];
    int volume,reported_volume;
    uint64_t reported_seek,auto_restarts;
    uint32_t repeat_frames;
    bool auto_restart;
    bool configured,closing,stopped,reopen,start_requested;
};
typedef struct { lv_image_t image; player_binding_t *binding; } player_widget_t;
struct slave_binding {
    lv_obj_t *obj;
    lv_timer_t *timer;
    player_binding_t *master;
    slave_binding_t *next;
    player_frame_t *frame;
};
typedef struct { lv_image_t image; slave_binding_t *binding; } slave_widget_t;
struct player_group {
    lv_obj_t obj;
    player_binding_t *members;
};
static bool backend_preserve(player_binding_t *b,bool enabled);
static void group_reset(player_group_t *group)
{
    if(group) for(player_binding_t *b=group->members;b;b=b->group_next) b->group_presented=false;
}
static void group_unlink(player_binding_t *b)
{
    player_group_t *group=b->group;
    if(group) {
        player_binding_t **entry=&group->members;
        while(*entry && *entry!=b) entry=&(*entry)->group_next;
        if(*entry) *entry=b->group_next;
        group_reset(group);
    }
    b->group=NULL; b->group_next=NULL; b->group_presented=false;
}
/* Like the SDK's successful-GET_FRAME barrier: each member publishes at most
 * once per round. This is not a timestamp or cross-display scanout barrier.
 * A paused/starved/terminal member holds the round until replay or detachment. */
static bool group_wait(player_binding_t *b)
{
    if(!b->group) return false;
    bool complete=true;
    for(player_binding_t *m=b->group->members;m;m=m->group_next)
        if(!m->group_presented) complete=false;
    if(complete) group_reset(b->group);
    return b->group_presented;
}
static void group_destructor(const lv_obj_class_t *class_p,lv_obj_t *obj)
{
    (void)class_p;
    player_group_t *g=(player_group_t *)obj;
    while(g->members) {
        player_binding_t *b=g->members;group_unlink(b);
        (void)backend_preserve(b,false);
    }
}
const lv_obj_class_t lv_aic_player_group_class={
    .base_class=&lv_obj_class,.instance_size=sizeof(player_group_t),.destructor_cb=group_destructor,
    .width_def=LV_SIZE_CONTENT,.height_def=LV_SIZE_CONTENT,.name="aic_player_group",
};
lv_obj_t *lv_aic_player_group_create(lv_obj_t *parent)
{
    lv_obj_t *obj=lv_obj_class_create_obj(&lv_aic_player_group_class,parent);
    if(obj) lv_obj_class_init_obj(obj);
    return obj;
}
lv_result_t lv_aic_player_set_group(lv_obj_t *obj,lv_obj_t *group)
{
    LV_CHECK_OBJ(obj,&lv_aic_player_class,return LV_RESULT_INVALID);
    if(group) { LV_CHECK_OBJ(group,&lv_aic_player_group_class,return LV_RESULT_INVALID); }
    player_binding_t *b=((player_widget_t *)obj)->binding;
    if(b->group==(player_group_t *)group) return LV_RESULT_OK;
    group_unlink(b);
    if(group) {
        b->group=(player_group_t *)group;
        b->group_next=b->group->members; b->group->members=b;
        group_reset(b->group);
    }
    /* Reassignment must not briefly disable preservation between groups. */
    (void)backend_preserve(b,group!=NULL);
    return LV_RESULT_OK;
}
lv_obj_t *lv_aic_player_get_group(lv_obj_t *obj)
{
    LV_CHECK_OBJ(obj,&lv_aic_player_class,return NULL);
    return (lv_obj_t *)((player_widget_t *)obj)->binding->group;
}
lv_result_t lv_aic_player_group_add(lv_obj_t *group,lv_obj_t *player)
{
    LV_CHECK_OBJ(group,&lv_aic_player_group_class,return LV_RESULT_INVALID);
    return lv_aic_player_set_group(player,group);
}
lv_result_t lv_aic_player_group_remove(lv_obj_t *group,lv_obj_t *player)
{
    LV_CHECK_OBJ(group,&lv_aic_player_group_class,return LV_RESULT_INVALID);
    LV_CHECK_OBJ(player,&lv_aic_player_class,return LV_RESULT_INVALID);
    if(lv_aic_player_get_group(player)!=group) return LV_RESULT_INVALID;
    return lv_aic_player_set_group(player,NULL);
}
size_t lv_aic_player_group_get_count(lv_obj_t *group)
{
    LV_CHECK_OBJ(group,&lv_aic_player_group_class,return 0);
    size_t count=0;
    for(player_binding_t *b=((player_group_t *)group)->members;b;b=b->group_next) count++;
    return count;
}
lv_result_t lv_aic_player_group_control(lv_obj_t *group,lv_aic_player_cmd_t cmd,void *data,size_t *accepted)
{
    if(accepted) *accepted=0;
    LV_CHECK_OBJ(group,&lv_aic_player_group_class,return LV_RESULT_INVALID);
    /* Query fan-out would overwrite one payload with unrelated member data.
     * Membership changes during traversal are intentionally not broadcast. */
    switch(cmd) {
    case LV_AIC_PLAYER_CMD_START: case LV_AIC_PLAYER_CMD_STOP:
    case LV_AIC_PLAYER_CMD_PAUSE: case LV_AIC_PLAYER_CMD_RESUME: break;
    case LV_AIC_PLAYER_CMD_SET_VOLUME: case LV_AIC_PLAYER_CMD_SET_PLAYBACK_RATE:
        if(!data) return LV_RESULT_INVALID;
        break;
    case LV_AIC_PLAYER_CMD_SET_PLAY_TIME:
        if(!data || *(uint64_t *)data) return LV_RESULT_INVALID;
        break;
    default: return LV_RESULT_INVALID;
    }
    player_group_t *g=(player_group_t *)group;
    if(!g->members) return LV_RESULT_INVALID;
    size_t done=0; bool ok=true;
    /* Controls enqueue requests and never emit synchronous widget events.
     * Workers cannot mutate LVGL membership; all list access is owner-only. */
    for(player_binding_t *b=g->members;b;b=b->group_next) {
        if(lv_aic_player_control(b->obj,cmd,data)==LV_RESULT_OK) done++;
        else ok=false;
    }
    if(accepted) *accepted=done;
    return ok?LV_RESULT_OK:LV_RESULT_INVALID;
}
static unsigned orphans;
static bool draws_idle(void)
{
    for(lv_display_t *d=lv_display_get_next(NULL);d;d=lv_display_get_next(d))
        for(lv_layer_t *l=d->layer_head;l;l=l->next) if(l->draw_task_head) return false;
    return true;
}
static void release_frame(player_frame_t *frame)
{
    if(frame && --frame->owners==0) {
        lv_aic_player_image_destroy(&frame->image); lv_free(frame);
    }
}
static void update_slave_frame(slave_binding_t *s,player_frame_t *next)
{
    if(s->frame==next) return;
    if(next) next->owners++;
    if(s->obj) lv_image_set_src(s->obj,next?lv_aic_player_image_source(&next->image):NULL);
    release_frame(s->frame); s->frame=next;
}
static void publish_slaves(player_binding_t *b,player_frame_t *frame)
{
    slave_binding_t *s=b->slaves;
    while(s) {
        slave_binding_t *next=s->next;
        if(s->obj && s->master==b) update_slave_frame(s,frame);
        s=next;
    }
}
static bool retire(player_binding_t *b)
{
#if defined(AIC_LVGL_USE_VIDEO_PLANE) && AIC_LVGL_USE_VIDEO_PLANE
    if(!lv_aic_plane_window_close(&b->window)) return false;
#endif
    publish_slaves(b,NULL);
    if(b->frame) {
        if(b->obj) lv_image_set_src(b->obj,NULL);
        release_frame(b->frame); b->frame=NULL;
    }
    return true;
}
static void unlink_slave(slave_binding_t *s)
{
    if(s->master) {
        slave_binding_t **entry=&s->master->slaves;
        while(*entry && *entry!=s) entry=&(*entry)->next;
        if(*entry) *entry=s->next;
    }
    s->master=NULL; s->next=NULL;
}
static void slave_tick(lv_timer_t *timer)
{
    slave_binding_t *s=lv_timer_get_user_data(timer);
    if(!draws_idle()) return;
    player_frame_t *next=s->obj && s->master?s->master->frame:NULL;
    update_slave_frame(s,next);
    if(!s->obj) { orphans--; lv_timer_delete(timer); lv_free(s); }
    else if(!s->master) lv_timer_pause(timer);
}
static void slave_destructor(const lv_obj_class_t *class_p,lv_obj_t *obj)
{
    (void)class_p; slave_binding_t *s=((slave_widget_t *)obj)->binding;
    if(!s) return;
    unlink_slave(s); s->obj=NULL; orphans++; lv_timer_resume(s->timer);
}
const lv_obj_class_t lv_aic_slave_class={
    .base_class=&lv_image_class,.instance_size=sizeof(slave_widget_t),.destructor_cb=slave_destructor,
    .width_def=LV_SIZE_CONTENT,.height_def=LV_SIZE_CONTENT,.name="aic_slave_player",
};
lv_obj_t *lv_aic_slave_player_create(lv_obj_t *parent)
{
    lv_obj_t *obj=lv_obj_class_create_obj(&lv_aic_slave_class,parent); if(!obj) return NULL;
    lv_obj_class_init_obj(obj);
    slave_binding_t *s=lv_malloc_zeroed(sizeof(*s));
    if(!s) { lv_obj_delete(obj); return NULL; }
    s->timer=lv_timer_create(slave_tick,20,s);
    if(!s->timer) { lv_free(s); lv_obj_delete(obj); return NULL; }
    s->obj=obj; ((slave_widget_t *)obj)->binding=s; lv_timer_pause(s->timer); return obj;
}
lv_result_t lv_aic_slave_player_set_master(lv_obj_t *obj,lv_obj_t *master)
{
    LV_CHECK_OBJ(obj,&lv_aic_slave_class,return LV_RESULT_INVALID);
    if(master) { LV_CHECK_OBJ(master,&lv_aic_player_class,return LV_RESULT_INVALID); }
    slave_binding_t *s=((slave_widget_t *)obj)->binding;
    unlink_slave(s);
    if(master) {
        s->master=((player_widget_t *)master)->binding;
        s->next=s->master->slaves; s->master->slaves=s;
    }
    lv_timer_resume(s->timer); return LV_RESULT_OK;
}
lv_obj_t *lv_aic_slave_player_get_master(lv_obj_t *obj)
{
    LV_CHECK_OBJ(obj,&lv_aic_slave_class,return NULL);
    slave_binding_t *s=((slave_widget_t *)obj)->binding;
    return s->master?s->master->obj:NULL;
}
/* Match the SDK's case-sensitive native-path suffix selection. */
static bool png_source(const char *uri)
{
    size_t n=strlen(uri);
    return (n>=4 && !strcmp(uri+n-4,".png")) || (n>=5 && !strcmp(uri+n-5,".apng"));
}
static bool backend_active(player_binding_t *b)
{
#if defined(AIC_LVGL_USE_APNG) && AIC_LVGL_USE_APNG
    if(b->apng) return true;
#endif
    return b->playback!=NULL;
}
static bool source_configured(player_binding_t *b,const char *uri)
{
    if(!png_source(uri)) return b->configured;
#if defined(AIC_LVGL_USE_APNG) && AIC_LVGL_USE_APNG
    return b->apng_configured;
#else
    return false;
#endif
}
static void backend_close(player_binding_t *b)
{
#if defined(AIC_LVGL_USE_APNG) && AIC_LVGL_USE_APNG
    if(b->apng) { lv_aic_apng_playback_close(b->apng); return; }
#endif
    lv_aic_player_playback_close(b->playback);
}
static bool backend_destroy(player_binding_t *b)
{
#if defined(AIC_LVGL_USE_APNG) && AIC_LVGL_USE_APNG
    if(b->apng) {
        if(!lv_aic_apng_playback_destroy(b->apng)) return false;
        b->apng=NULL; return true;
    }
#endif
    if(!lv_aic_player_playback_destroy(b->playback)) return false;
    b->playback=NULL; return true;
}
static lv_aic_playback_status_t backend_status(player_binding_t *b)
{
#if defined(AIC_LVGL_USE_APNG) && AIC_LVGL_USE_APNG
    if(b->apng) {
        lv_aic_apng_playback_status_t a=lv_aic_apng_playback_status(b->apng);
        static const lv_aic_playback_state_t states[]={
            LV_AIC_PLAYBACK_OPENING,LV_AIC_PLAYBACK_READY,LV_AIC_PLAYBACK_PLAYING,
            LV_AIC_PLAYBACK_PAUSED,LV_AIC_PLAYBACK_TERMINAL,LV_AIC_PLAYBACK_CLOSING,
            LV_AIC_PLAYBACK_CLOSED,LV_AIC_PLAYBACK_FAULT};
        lv_aic_playback_status_t s={.state=states[a.state],.finished=a.finished,.volume=-1,
            .has_video=true,.width=a.width,.height=a.height,.seek_pending=a.restart_pending,
            .seeks_completed=a.restarts,.video_eos=a.state==LV_AIC_APNG_TERMINAL,
            .frames_received=a.composed>UINT32_MAX?UINT32_MAX:(uint32_t)a.composed,
            .frames_queued=a.published>UINT32_MAX?UINT32_MAX:(uint32_t)a.published};
        if(a.restart_pending) s.state=LV_AIC_PLAYBACK_SEEKING;
        if(!a.restart_pending && a.state>=LV_AIC_APNG_READY && a.state<=LV_AIC_APNG_TERMINAL &&
           a.width && a.height && a.width<=INT32_MAX && a.height<=INT32_MAX && a.file_bytes<=INT64_MAX) {
            s.media_info_valid=true;
            s.media_info=(lv_aic_media_info_t){.file_size=(int64_t)a.file_bytes,.has_video=1,
                .video_stream={(int32_t)a.width,(int32_t)a.height}};
        }
        return s;
    }
#endif
    return lv_aic_player_playback_status(b->playback);
}
static bool backend_preserve(player_binding_t *b,bool enabled)
{
#if defined(AIC_LVGL_USE_APNG) && AIC_LVGL_USE_APNG
    if(b->apng) return lv_aic_apng_playback_preserve(b->apng,enabled);
#endif
    return lv_aic_player_playback_preserve(b->playback,enabled);
}
static bool backend_start(player_binding_t *b)
{
#if defined(AIC_LVGL_USE_APNG) && AIC_LVGL_USE_APNG
    if(b->apng) return lv_aic_apng_playback_start(b->apng);
#endif
    return lv_aic_player_playback_start(b->playback);
}
static bool backend_pause(player_binding_t *b,bool paused)
{
#if defined(AIC_LVGL_USE_APNG) && AIC_LVGL_USE_APNG
    if(b->apng) return lv_aic_apng_playback_pause(b->apng,paused);
#endif
    return lv_aic_player_playback_pause(b->playback,paused);
}
static bool backend_seek(player_binding_t *b,uint64_t position_us)
{
#if defined(AIC_LVGL_USE_APNG) && AIC_LVGL_USE_APNG
    if(b->apng) return !position_us && lv_aic_apng_playback_restart(b->apng);
#endif
    return lv_aic_player_playback_seek(b->playback,position_us);
}
static bool backend_poll(player_binding_t *b,lv_aic_player_image_t *out)
{
#if defined(AIC_LVGL_USE_APNG) && AIC_LVGL_USE_APNG
    if(b->apng) { uint64_t sequence; return lv_aic_apng_playback_poll(b->apng,&out->rgb,&sequence); }
#endif
    return lv_aic_player_playback_poll(b->playback,out);
}
static bool can_repeat(player_binding_t *b)
{
#if defined(AIC_LVGL_USE_APNG) && AIC_LVGL_USE_APNG
    if(b->apng) return b->status.video_eos && b->status.frames_queued!=b->repeat_frames;
#endif
    return b->status.seekable &&
       ((b->status.has_video && b->status.video_eos && b->status.frames_queued!=b->repeat_frames) ||
        (!b->status.has_video && b->status.has_audio && b->status.position_valid));
}
static bool prepare(player_binding_t *b)
{
#if defined(AIC_LVGL_USE_VIDEO_PLANE) && AIC_LVGL_USE_VIDEO_PLANE
    b->plane_failed=false;
#endif
#if defined(AIC_LVGL_USE_APNG) && AIC_LVGL_USE_APNG
    if(png_source(b->uri)) b->apng=lv_aic_apng_playback_prepare(b->uri,&b->apng_options);
    else
#endif
        b->playback=lv_aic_player_playback_prepare(b->uri,&b->options);
    b->status=lv_aic_player_playback_status(NULL);
    if(!backend_active(b)) { b->state=LV_AIC_PLAYER_FAULT; return false; }
    b->state=LV_AIC_PLAYER_OPENING; b->stopped=false; b->repeat_frames=0;
    bool setup=backend_preserve(b,b->group!=NULL);
#if defined(AIC_LVGL_USE_APNG) && AIC_LVGL_USE_APNG
    if(b->apng) setup=setup && lv_aic_apng_playback_rate(b->apng,b->rate_num,b->rate_den);
    else
#endif
        if(b->volume>=0) setup=setup && lv_aic_player_playback_volume(b->playback,b->volume);
    if(!setup || (b->start_requested && !backend_start(b))) {
        backend_close(b); b->state=LV_AIC_PLAYER_FAULT; return false;
    }
    return true;
}
#if defined(AIC_LVGL_USE_VIDEO_PLANE) && AIC_LVGL_USE_VIDEO_PLANE
static bool present_plane(player_binding_t *b)
{
    if(!b->plane_enabled || !b->frame) return true;
    return lv_aic_plane_window_present(&b->window,b->obj,lv_aic_player_image_source(&b->frame->image));
}

#endif
static void apply_requested_size(player_binding_t *b)
{
    if(!b->obj || b->closing || !b->frame) return;
    /* Native video-plane output replaces the object's source with a fake
     * window. That window describes destination geometry, not decoder pixels. */
    const lv_image_dsc_t *image=lv_aic_player_image_source(&b->frame->image);
    if(!image) return;
    uint32_t w=image->header.w,h=image->header.h;
    if(!w || !h) return;
    bool plane=false;
#if defined(AIC_LVGL_USE_VIDEO_PLANE) && AIC_LVGL_USE_VIDEO_PLANE
    plane=b->plane_enabled;
#endif
    /* DE scales the decoded frame into the fake destination window. Applying
     * image scaling too would transform that window a second time. */
    if(b->requested_width) {
        uint32_t scale=plane?LV_SCALE_NONE:(uint32_t)(((uint64_t)b->requested_width*256)/w);
        lv_image_set_scale_x(b->obj,scale?scale:1);
        if(!b->obj || b->closing) return;
        lv_obj_set_width(b->obj,(int32_t)b->requested_width);
    }
    if(!b->obj || b->closing) return;
    if(b->requested_height) {
        uint32_t scale=plane?LV_SCALE_NONE:(uint32_t)(((uint64_t)b->requested_height*256)/h);
        lv_image_set_scale_y(b->obj,scale?scale:1);
        if(!b->obj || b->closing) return;
        lv_obj_set_height(b->obj,(int32_t)b->requested_height);
    }
}

static void tick(lv_timer_t *timer)
{
    player_binding_t *b=lv_timer_get_user_data(timer);
    if(!draws_idle()) return;
    if(!b->obj || b->closing) {
        if(!retire(b)) return;
        if(backend_active(b)) b->status=backend_status(b);
        if(!backend_destroy(b)) return;
        b->playback=NULL; b->closing=false;
        if(!b->obj) { orphans--; lv_timer_delete(timer); lv_free(b); return; }
        if(b->reopen) { b->reopen=false; (void)prepare(b); }
        else { b->state=b->stopped?LV_AIC_PLAYER_STOPPED:LV_AIC_PLAYER_CLOSED; lv_timer_pause(timer); }
    }
    else if(backend_active(b)) {
        b->status=backend_status(b);
        if(b->status.seek_pending && !retire(b)) return;
        switch(b->status.state) {
        case LV_AIC_PLAYBACK_OPENING: b->state=LV_AIC_PLAYER_OPENING; break;
        case LV_AIC_PLAYBACK_READY: b->state=LV_AIC_PLAYER_READY; break;
        case LV_AIC_PLAYBACK_PLAYING: b->state=LV_AIC_PLAYER_PLAYING; break;
        case LV_AIC_PLAYBACK_PAUSED: b->state=LV_AIC_PLAYER_PAUSED; break;
        case LV_AIC_PLAYBACK_TERMINAL: b->state=LV_AIC_PLAYER_TERMINAL; break;
        case LV_AIC_PLAYBACK_SEEKING: b->state=LV_AIC_PLAYER_SEEKING; break;
        case LV_AIC_PLAYBACK_FAULT: b->state=LV_AIC_PLAYER_FAULT; break;
        default: break;
        }
#if defined(AIC_LVGL_USE_VIDEO_PLANE) && AIC_LVGL_USE_VIDEO_PLANE
        if(b->plane_failed) b->state=LV_AIC_PLAYER_FAULT;
#endif
        if(b->state==LV_AIC_PLAYER_FAULT) (void)retire(b);
        else if(b->state==LV_AIC_PLAYER_PLAYING || b->state==LV_AIC_PLAYER_TERMINAL) {
            if(!group_wait(b)) {
                /* Reserve the widget owner before consuming the mailbox so an
                 * allocation failure cannot drop a preserved frame. */
                player_frame_t *frame=lv_malloc_zeroed(sizeof(*frame));
                if(frame && backend_poll(b,&frame->image)) {
                    frame->owners=1;
                    player_frame_t *old=b->frame; b->frame=frame;
#if defined(AIC_LVGL_USE_VIDEO_PLANE) && AIC_LVGL_USE_VIDEO_PLANE
                    if(!b->plane_enabled || !old)
#endif
                        lv_image_set_src(b->obj,lv_aic_player_image_source(&frame->image));
                    apply_requested_size(b);
                    publish_slaves(b,frame);
                    b->group_presented=true;
                    release_frame(old);
                } else lv_free(frame);
            }
        }
    }
#if defined(AIC_LVGL_USE_VIDEO_PLANE) && AIC_LVGL_USE_VIDEO_PLANE
    if(b->obj && !b->closing && !b->plane_failed && !present_plane(b)) {
        b->plane_failed=true;b->state=LV_AIC_PLAYER_FAULT;backend_close(b);
        (void)retire(b);
    }
#endif
    /* Notify terminal on a separate tick first, so its handler can disable
     * repeat, replace source, stop or delete without post-event object access. */
    if(b->obj && !b->closing && b->state==LV_AIC_PLAYER_TERMINAL &&
       b->reported==LV_AIC_PLAYER_TERMINAL && b->auto_restart &&
       b->auto_restarts<UINT64_MAX && can_repeat(b) && !group_wait(b)) {
        if(backend_seek(b,0)) {
            b->repeat_frames=b->status.frames_queued; b->auto_restarts++;
            b->state=LV_AIC_PLAYER_SEEKING; group_reset(b->group); retire(b);
        }
    }
    /* Final action only: callback can delete the widget or replace its source. */
    if(b->obj && (b->state!=b->reported || b->status.volume!=b->reported_volume || b->status.seeks_completed!=b->reported_seek)) {
        b->reported=b->state; b->reported_volume=b->status.volume; b->reported_seek=b->status.seeks_completed;
        lv_obj_send_event(b->obj,LV_EVENT_VALUE_CHANGED,NULL);
    }
}
static void destructor(const lv_obj_class_t *class_p,lv_obj_t *obj)
{
    (void)class_p; player_binding_t *b=((player_widget_t *)obj)->binding;
    if(!b) return;
    /* Break object links now; slave image owners survive until their own idle
     * draw pass, and native readers may outlive both widget objects. */
    while(b->slaves) {
        slave_binding_t *s=b->slaves; unlink_slave(s); lv_timer_resume(s->timer);
    }
    group_unlink(b);
    b->obj=NULL; b->reopen=false; orphans++;
    backend_close(b); lv_timer_resume(b->timer);
}
const lv_obj_class_t lv_aic_player_class={
    .base_class=&lv_image_class,.instance_size=sizeof(player_widget_t),.destructor_cb=destructor,
    .width_def=LV_SIZE_CONTENT,.height_def=LV_SIZE_CONTENT,.name="aic_player",
};
lv_obj_t *lv_aic_player_create(lv_obj_t *parent)
{
    lv_obj_t *obj=lv_obj_class_create_obj(&lv_aic_player_class,parent); if(!obj) return NULL;
    lv_obj_class_init_obj(obj);
    player_binding_t *b=lv_malloc_zeroed(sizeof(*b));
    if(!b) { lv_obj_delete(obj); return NULL; }
    b->timer=lv_timer_create(tick,20,b);
    if(!b->timer) { lv_free(b); lv_obj_delete(obj); return NULL; }
#if defined(AIC_LVGL_USE_APNG) && AIC_LVGL_USE_APNG
    b->rate_num=b->rate_den=1;
#endif
    b->obj=obj; b->volume=b->reported_volume=-1; b->status=lv_aic_player_playback_status(NULL);
    ((player_widget_t *)obj)->binding=b; lv_timer_pause(b->timer); return obj;
}
lv_result_t lv_aic_player_set_video_plane(lv_obj_t *obj,bool enabled)
{
    LV_CHECK_OBJ(obj,&lv_aic_player_class,return LV_RESULT_INVALID);
#if defined(AIC_LVGL_USE_VIDEO_PLANE) && AIC_LVGL_USE_VIDEO_PLANE
    player_binding_t *b=((player_widget_t *)obj)->binding;
    if(backend_active(b) || b->closing || b->frame || b->window.plane) return LV_RESULT_INVALID;
    if(enabled && lv_display_get_color_format(lv_obj_get_display(obj))!=LV_COLOR_FORMAT_ARGB8888) return LV_RESULT_INVALID;
    b->plane_enabled=enabled;return LV_RESULT_OK;
#else
    return enabled?LV_RESULT_INVALID:LV_RESULT_OK;
#endif
}
lv_result_t lv_aic_player_set_video_plane_rotation_budget(lv_obj_t *obj,size_t bytes)
{
    LV_CHECK_OBJ(obj,&lv_aic_player_class,return LV_RESULT_INVALID);
#if defined(AIC_LVGL_USE_VIDEO_PLANE) && AIC_LVGL_USE_VIDEO_PLANE
    player_binding_t *b=((player_widget_t *)obj)->binding;
    if(backend_active(b) || b->closing || b->frame || b->window.plane) return LV_RESULT_INVALID;
    b->window.budget=bytes;return LV_RESULT_OK;
#else
    return bytes?LV_RESULT_INVALID:LV_RESULT_OK;
#endif
}
lv_result_t lv_aic_player_configure(lv_obj_t *obj,const lv_aic_playback_options_t *options)
{
    LV_CHECK_OBJ(obj,&lv_aic_player_class,return LV_RESULT_INVALID);
    player_binding_t *b=((player_widget_t *)obj)->binding;
    if(backend_active(b) || b->closing || !options || !options->cma_budget || options->extra_frames<2 || options->extra_frames>8 ||
       options->color_space<LV_AIC_YUV_BT601_LIMITED || options->color_space>LV_AIC_YUV_BT709_FULL) return LV_RESULT_INVALID;
    b->options=*options; b->configured=true; return LV_RESULT_OK;
}
#if defined(AIC_LVGL_USE_APNG) && AIC_LVGL_USE_APNG
lv_result_t lv_aic_player_configure_apng(lv_obj_t *obj,const lv_aic_apng_playback_options_t *o)
{
    LV_CHECK_OBJ(obj,&lv_aic_player_class,return LV_RESULT_INVALID);
    player_binding_t *b=((player_widget_t *)obj)->binding;
    if(backend_active(b) || b->closing || !o || !o->limits.file_bytes || o->limits.file_bytes>LONG_MAX ||
       !o->limits.frame_png_bytes || !o->limits.canvas_pixels || !o->limits.frames ||
       !o->stream_budget || !o->snapshot_budget || !o->cma_budget || o->packet_limit<256 ||
       o->packet_limit>INT_MAX-255U || o->snapshots<2 || o->snapshots>8 ||
       !o->minimum_delay_us || o->minimum_delay_us>1000000) return LV_RESULT_INVALID;
    b->apng_options=*o; b->apng_configured=true; return LV_RESULT_OK;
}
#endif
lv_result_t lv_aic_player_set_rate(lv_obj_t *obj,uint32_t numerator,uint32_t denominator)
{
    LV_CHECK_OBJ(obj,&lv_aic_player_class,return LV_RESULT_INVALID);
#if defined(AIC_LVGL_USE_APNG) && AIC_LVGL_USE_APNG
    player_binding_t *b=((player_widget_t *)obj)->binding;
    if(!b->closing && b->apng && lv_aic_apng_playback_rate(b->apng,numerator,denominator)) {
        b->rate_num=numerator; b->rate_den=denominator; return LV_RESULT_OK;
    }
#else
    (void)numerator; (void)denominator;
#endif
    return LV_RESULT_INVALID;
}
lv_result_t lv_aic_player_get_rate(lv_obj_t *obj,uint32_t *numerator,uint32_t *denominator)
{
    LV_CHECK_OBJ(obj,&lv_aic_player_class,return LV_RESULT_INVALID);
    player_binding_t *b=((player_widget_t *)obj)->binding;
    if(!numerator || !denominator || b->closing || !backend_active(b)) return LV_RESULT_INVALID;
    lv_aic_playback_status_t s=backend_status(b);
    if(s.seek_pending || (s.state!=LV_AIC_PLAYBACK_READY && s.state!=LV_AIC_PLAYBACK_PLAYING &&
       s.state!=LV_AIC_PLAYBACK_PAUSED && s.state!=LV_AIC_PLAYBACK_TERMINAL)) return LV_RESULT_INVALID;
    uint32_t n=1,d=1;
#if defined(AIC_LVGL_USE_APNG) && AIC_LVGL_USE_APNG
    if(b->apng) {
        lv_aic_apng_playback_status_t a=lv_aic_apng_playback_status(b->apng);
        n=a.rate_num; d=a.rate_den;
        if(!n || !d) return LV_RESULT_INVALID;
    }
#endif
    *numerator=n; *denominator=d; return LV_RESULT_OK;
}
lv_result_t lv_aic_player_set_src(lv_obj_t *obj,const char *uri)
{
    LV_CHECK_OBJ(obj,&lv_aic_player_class,return LV_RESULT_INVALID);
    player_binding_t *b=((player_widget_t *)obj)->binding;
    if(!uri || !uri[0] || strlen(uri)>=sizeof(b->uri) || !source_configured(b,uri)) return LV_RESULT_INVALID;
    memcpy(b->uri,uri,strlen(uri)+1); b->start_requested=false; group_reset(b->group);
    lv_timer_resume(b->timer);
    if(backend_active(b) || b->closing) {
        b->closing=b->reopen=true; b->state=LV_AIC_PLAYER_STOPPING;
        backend_close(b); return LV_RESULT_OK;
    }
    return prepare(b)?LV_RESULT_OK:LV_RESULT_INVALID;
}
lv_result_t lv_aic_player_start(lv_obj_t *obj)
{
    LV_CHECK_OBJ(obj,&lv_aic_player_class,return LV_RESULT_INVALID);
    player_binding_t *b=((player_widget_t *)obj)->binding;
    if(!b->uri[0] || !source_configured(b,b->uri) || b->state==LV_AIC_PLAYER_FAULT) return LV_RESULT_INVALID;
    b->start_requested=true; group_reset(b->group); lv_timer_resume(b->timer);
    if(b->closing || b->state==LV_AIC_PLAYER_TERMINAL) {
        b->closing=b->reopen=true; b->state=LV_AIC_PLAYER_STOPPING;
        backend_close(b); return LV_RESULT_OK;
    }
    if(!backend_active(b)) return prepare(b)?LV_RESULT_OK:LV_RESULT_INVALID;
    return backend_start(b)?LV_RESULT_OK:LV_RESULT_INVALID;
}
static lv_result_t request_close(lv_obj_t *obj,bool stopped)
{
    LV_CHECK_OBJ(obj,&lv_aic_player_class,return LV_RESULT_INVALID);
    player_binding_t *b=((player_widget_t *)obj)->binding;
    group_reset(b->group);
    b->reopen=b->start_requested=false; b->stopped=stopped; b->closing=true; b->state=LV_AIC_PLAYER_STOPPING;
    backend_close(b); lv_timer_resume(b->timer); return LV_RESULT_OK;
}
lv_result_t lv_aic_player_stop(lv_obj_t *obj) { return request_close(obj,true); }
lv_result_t lv_aic_player_close(lv_obj_t *obj) { return request_close(obj,false); }
static lv_result_t request_pause(lv_obj_t *obj,bool paused)
{
    LV_CHECK_OBJ(obj,&lv_aic_player_class,return LV_RESULT_INVALID);
    player_binding_t *b=((player_widget_t *)obj)->binding;
    return !b->closing && backend_pause(b,paused)?LV_RESULT_OK:LV_RESULT_INVALID;
}
lv_result_t lv_aic_player_pause(lv_obj_t *obj) { return request_pause(obj,true); }
lv_result_t lv_aic_player_resume(lv_obj_t *obj) { return request_pause(obj,false); }
lv_result_t lv_aic_player_seek(lv_obj_t *obj,uint64_t position_us)
{
    LV_CHECK_OBJ(obj,&lv_aic_player_class,return LV_RESULT_INVALID);
    player_binding_t *b=((player_widget_t *)obj)->binding;
    if((b->group && position_us) || b->closing || !backend_seek(b,position_us)) return LV_RESULT_INVALID;
    group_reset(b->group);
    b->repeat_frames=backend_status(b).frames_queued;
    b->state=LV_AIC_PLAYER_SEEKING; lv_timer_resume(b->timer); return LV_RESULT_OK;
}
lv_result_t lv_aic_player_set_auto_restart(lv_obj_t *obj,bool enabled)
{
    LV_CHECK_OBJ(obj,&lv_aic_player_class,return LV_RESULT_INVALID);
    player_binding_t *b=((player_widget_t *)obj)->binding;
    b->auto_restart=enabled; return LV_RESULT_OK;
}
bool lv_aic_player_get_auto_restart(lv_obj_t *obj)
{
    LV_CHECK_OBJ(obj,&lv_aic_player_class,return false);
    return ((player_widget_t *)obj)->binding->auto_restart;
}
uint64_t lv_aic_player_get_auto_restart_count(lv_obj_t *obj)
{
    LV_CHECK_OBJ(obj,&lv_aic_player_class,return 0);
    return ((player_widget_t *)obj)->binding->auto_restarts;
}
lv_result_t lv_aic_player_set_volume(lv_obj_t *obj,int volume)
{
    LV_CHECK_OBJ(obj,&lv_aic_player_class,return LV_RESULT_INVALID);
    player_binding_t *b=((player_widget_t *)obj)->binding;
#if defined(AIC_LVGL_USE_APNG) && AIC_LVGL_USE_APNG
    if(b->apng || (b->uri[0] && png_source(b->uri))) return LV_RESULT_INVALID;
#endif
    if(volume<0 || volume>100) return LV_RESULT_INVALID;
    if(b->playback && !b->closing && !lv_aic_player_playback_volume(b->playback,volume)) return LV_RESULT_INVALID;
    b->volume=volume; return LV_RESULT_OK;
}
lv_aic_player_state_t lv_aic_player_get_state(lv_obj_t *obj)
{
    LV_CHECK_OBJ(obj,&lv_aic_player_class,return LV_AIC_PLAYER_FAULT);
    return ((player_widget_t *)obj)->binding->state;
}
lv_aic_playback_status_t lv_aic_player_get_status(lv_obj_t *obj)
{
    LV_CHECK_OBJ(obj,&lv_aic_player_class,return lv_aic_player_playback_status(NULL));
    player_binding_t *b=((player_widget_t *)obj)->binding;
    return backend_active(b)?backend_status(b):b->status;
}
unsigned lv_aic_player_pending_cleanup(void) { return orphans; }

/* Native LVGL image state is shared by main and slave players; keep transform
 * behavior independent of media/APNG backend selection. */
void lv_aic_player_set_pivot(lv_obj_t *obj,int32_t x,int32_t y)
{ lv_image_set_pivot(obj,x,y); }
void lv_aic_player_get_pivot(lv_obj_t *obj,lv_point_t *pivot)
{ lv_image_get_pivot(obj,pivot); }
void lv_aic_player_set_rotation(lv_obj_t *obj,int32_t value)
{ lv_image_set_rotation(obj,value); }
int32_t lv_aic_player_get_rotation(lv_obj_t *obj)
{ return lv_image_get_rotation(obj); }
static void reset_requested_size(lv_obj_t *obj)
{
    if(lv_obj_check_type(obj,&lv_aic_player_class)) {
        player_binding_t *b=((player_widget_t *)obj)->binding;
        b->requested_width=b->requested_height=0;
    }
}
lv_result_t lv_aic_player_set_width(lv_obj_t *obj,uint32_t width)
{
    LV_CHECK_OBJ(obj,&lv_aic_player_class,return LV_RESULT_INVALID);
    if(!width || width>4096) return LV_RESULT_INVALID;
    player_binding_t *b=((player_widget_t *)obj)->binding;
    b->requested_width=width;apply_requested_size(b);return LV_RESULT_OK;
}
lv_result_t lv_aic_player_set_height(lv_obj_t *obj,uint32_t height)
{
    LV_CHECK_OBJ(obj,&lv_aic_player_class,return LV_RESULT_INVALID);
    if(!height || height>4096) return LV_RESULT_INVALID;
    player_binding_t *b=((player_widget_t *)obj)->binding;
    b->requested_height=height;apply_requested_size(b);return LV_RESULT_OK;
}
void lv_aic_player_set_scale(lv_obj_t *obj,uint32_t value)
{ reset_requested_size(obj);lv_image_set_scale(obj,value); }
int32_t lv_aic_player_get_scale(lv_obj_t *obj)
{ return lv_image_get_scale(obj); }
void lv_aic_player_set_scale_x(lv_obj_t *obj,uint32_t value)
{ reset_requested_size(obj);lv_image_set_scale_x(obj,value); }
int32_t lv_aic_player_get_scale_x(lv_obj_t *obj)
{ return lv_image_get_scale_x(obj); }
void lv_aic_player_set_scale_y(lv_obj_t *obj,uint32_t value)
{ reset_requested_size(obj);lv_image_set_scale_y(obj,value); }
int32_t lv_aic_player_get_scale_y(lv_obj_t *obj)
{ return lv_image_get_scale_y(obj); }
void lv_aic_player_set_offset_x(lv_obj_t *obj,int32_t value)
{ lv_image_set_offset_x(obj,value); }
int32_t lv_aic_player_get_offset_x(lv_obj_t *obj)
{ return lv_image_get_offset_x(obj); }
void lv_aic_player_set_offset_y(lv_obj_t *obj,int32_t value)
{ lv_image_set_offset_y(obj,value); }
int32_t lv_aic_player_get_offset_y(lv_obj_t *obj)
{ return lv_image_get_offset_y(obj); }
void lv_aic_player_set_inner_align(lv_obj_t *obj,lv_image_align_t value)
{ lv_image_set_inner_align(obj,value); }
lv_image_align_t lv_aic_player_get_inner_align(lv_obj_t *obj)
{ return lv_image_get_inner_align(obj); }
#endif
