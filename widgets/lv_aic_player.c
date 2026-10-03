/* SPDX-License-Identifier: Apache-2.0 */
#include "lv_aic_player.h"
#define AIC_LVGL_USE_PRIVATE_API 1
#include "lvgl_aic_private.h"
#if defined(AIC_LVGL_USE_PLAYER) && AIC_LVGL_USE_PLAYER
#include <string.h>
#if !LV_USE_IMAGE
#error "The AIC player widget requires LV_USE_IMAGE"
#endif
typedef struct {
    lv_obj_t *obj;
    lv_timer_t *timer;
    lv_aic_player_playback_t *playback;
    lv_aic_player_image_t image;
    lv_aic_playback_options_t options;
    lv_aic_playback_status_t status;
    lv_aic_player_state_t state,reported;
    char uri[128];
    int volume,reported_volume;
    bool configured,closing,stopped,reopen,start_requested;
} player_binding_t;
typedef struct { lv_image_t image; player_binding_t *binding; } player_widget_t;
static unsigned orphans;
static bool draws_idle(void)
{
    for(lv_display_t *d=lv_display_get_next(NULL);d;d=lv_display_get_next(d))
        for(lv_layer_t *l=d->layer_head;l;l=l->next) if(l->draw_task_head) return false;
    return true;
}
static void retire(player_binding_t *b)
{
    if(b->image.rgb || b->image.yuv) {
        if(b->obj) lv_image_set_src(b->obj,NULL);
        lv_aic_player_image_destroy(&b->image);
    }
}
static bool prepare(player_binding_t *b)
{
    b->playback=lv_aic_player_playback_prepare(b->uri,&b->options);
    b->status=lv_aic_player_playback_status(NULL);
    if(!b->playback) { b->state=LV_AIC_PLAYER_FAULT; return false; }
    b->state=LV_AIC_PLAYER_OPENING; b->stopped=false;
    if((b->volume>=0 && !lv_aic_player_playback_volume(b->playback,b->volume)) ||
       (b->start_requested && !lv_aic_player_playback_start(b->playback))) {
        lv_aic_player_playback_close(b->playback); b->state=LV_AIC_PLAYER_FAULT; return false;
    }
    return true;
}
static void tick(lv_timer_t *timer)
{
    player_binding_t *b=lv_timer_get_user_data(timer);
    if(!draws_idle()) return;
    if(!b->obj || b->closing) {
        retire(b);
        if(b->playback) b->status=lv_aic_player_playback_status(b->playback);
        if(!lv_aic_player_playback_destroy(b->playback)) return;
        b->playback=NULL; b->closing=false;
        if(!b->obj) { orphans--; lv_timer_delete(timer); lv_free(b); return; }
        if(b->reopen) { b->reopen=false; (void)prepare(b); }
        else { b->state=b->stopped?LV_AIC_PLAYER_STOPPED:LV_AIC_PLAYER_CLOSED; lv_timer_pause(timer); }
    }
    else if(b->playback) {
        b->status=lv_aic_player_playback_status(b->playback);
        switch(b->status.state) {
        case LV_AIC_PLAYBACK_OPENING: b->state=LV_AIC_PLAYER_OPENING; break;
        case LV_AIC_PLAYBACK_READY: b->state=LV_AIC_PLAYER_READY; break;
        case LV_AIC_PLAYBACK_PLAYING: b->state=LV_AIC_PLAYER_PLAYING; break;
        case LV_AIC_PLAYBACK_PAUSED: b->state=LV_AIC_PLAYER_PAUSED; break;
        case LV_AIC_PLAYBACK_TERMINAL: b->state=LV_AIC_PLAYER_TERMINAL; break;
        case LV_AIC_PLAYBACK_FAULT: b->state=LV_AIC_PLAYER_FAULT; break;
        default: break;
        }
        if(b->state==LV_AIC_PLAYER_FAULT) retire(b);
        else if(b->state==LV_AIC_PLAYER_PLAYING || b->state==LV_AIC_PLAYER_TERMINAL) {
            lv_aic_player_image_t next={0};
            if(lv_aic_player_playback_poll(b->playback,&next)) {
                lv_aic_player_image_t old=b->image; b->image=next;
                lv_image_set_src(b->obj,lv_aic_player_image_source(&b->image));
                lv_aic_player_image_destroy(&old);
            }
        }
    }
    /* Final action only: callback can delete the widget or replace its source. */
    if(b->obj && (b->state!=b->reported || b->status.volume!=b->reported_volume)) {
        b->reported=b->state; b->reported_volume=b->status.volume;
        lv_obj_send_event(b->obj,LV_EVENT_VALUE_CHANGED,NULL);
    }
}
static void destructor(const lv_obj_class_t *class_p,lv_obj_t *obj)
{
    (void)class_p; player_binding_t *b=((player_widget_t *)obj)->binding;
    if(!b) return;
    b->obj=NULL; b->reopen=false; orphans++;
    lv_aic_player_playback_close(b->playback); lv_timer_resume(b->timer);
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
    b->obj=obj; b->volume=b->reported_volume=-1; b->status=lv_aic_player_playback_status(NULL);
    ((player_widget_t *)obj)->binding=b; lv_timer_pause(b->timer); return obj;
}
lv_result_t lv_aic_player_configure(lv_obj_t *obj,const lv_aic_playback_options_t *options)
{
    LV_CHECK_OBJ(obj,&lv_aic_player_class,return LV_RESULT_INVALID);
    player_binding_t *b=((player_widget_t *)obj)->binding;
    if(b->playback || b->closing || !options || !options->cma_budget || options->extra_frames<2 || options->extra_frames>8 ||
       options->color_space<LV_AIC_YUV_BT601_LIMITED || options->color_space>LV_AIC_YUV_BT709_FULL) return LV_RESULT_INVALID;
    b->options=*options; b->configured=true; return LV_RESULT_OK;
}
lv_result_t lv_aic_player_set_src(lv_obj_t *obj,const char *uri)
{
    LV_CHECK_OBJ(obj,&lv_aic_player_class,return LV_RESULT_INVALID);
    player_binding_t *b=((player_widget_t *)obj)->binding;
    if(!b->configured || !uri || !uri[0] || strlen(uri)>=sizeof(b->uri)) return LV_RESULT_INVALID;
    memcpy(b->uri,uri,strlen(uri)+1); b->start_requested=false;
    lv_timer_resume(b->timer);
    if(b->playback || b->closing) {
        b->closing=b->reopen=true; b->state=LV_AIC_PLAYER_STOPPING;
        lv_aic_player_playback_close(b->playback); return LV_RESULT_OK;
    }
    return prepare(b)?LV_RESULT_OK:LV_RESULT_INVALID;
}
lv_result_t lv_aic_player_start(lv_obj_t *obj)
{
    LV_CHECK_OBJ(obj,&lv_aic_player_class,return LV_RESULT_INVALID);
    player_binding_t *b=((player_widget_t *)obj)->binding;
    if(!b->configured || !b->uri[0] || b->state==LV_AIC_PLAYER_FAULT) return LV_RESULT_INVALID;
    b->start_requested=true; lv_timer_resume(b->timer);
    if(b->closing || b->state==LV_AIC_PLAYER_TERMINAL) {
        b->closing=b->reopen=true; b->state=LV_AIC_PLAYER_STOPPING;
        lv_aic_player_playback_close(b->playback); return LV_RESULT_OK;
    }
    if(!b->playback) return prepare(b)?LV_RESULT_OK:LV_RESULT_INVALID;
    return lv_aic_player_playback_start(b->playback)?LV_RESULT_OK:LV_RESULT_INVALID;
}
static lv_result_t request_close(lv_obj_t *obj,bool stopped)
{
    LV_CHECK_OBJ(obj,&lv_aic_player_class,return LV_RESULT_INVALID);
    player_binding_t *b=((player_widget_t *)obj)->binding;
    b->reopen=b->start_requested=false; b->stopped=stopped; b->closing=true; b->state=LV_AIC_PLAYER_STOPPING;
    lv_aic_player_playback_close(b->playback); lv_timer_resume(b->timer); return LV_RESULT_OK;
}
lv_result_t lv_aic_player_stop(lv_obj_t *obj) { return request_close(obj,true); }
lv_result_t lv_aic_player_close(lv_obj_t *obj) { return request_close(obj,false); }
static lv_result_t request_pause(lv_obj_t *obj,bool paused)
{
    LV_CHECK_OBJ(obj,&lv_aic_player_class,return LV_RESULT_INVALID);
    player_binding_t *b=((player_widget_t *)obj)->binding;
    return !b->closing && lv_aic_player_playback_pause(b->playback,paused)?LV_RESULT_OK:LV_RESULT_INVALID;
}
lv_result_t lv_aic_player_pause(lv_obj_t *obj) { return request_pause(obj,true); }
lv_result_t lv_aic_player_resume(lv_obj_t *obj) { return request_pause(obj,false); }
lv_result_t lv_aic_player_set_volume(lv_obj_t *obj,int volume)
{
    LV_CHECK_OBJ(obj,&lv_aic_player_class,return LV_RESULT_INVALID);
    player_binding_t *b=((player_widget_t *)obj)->binding;
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
    return b->playback?lv_aic_player_playback_status(b->playback):b->status;
}
unsigned lv_aic_player_pending_cleanup(void) { return orphans; }
#endif
