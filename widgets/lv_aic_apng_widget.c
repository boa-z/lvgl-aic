/* SPDX-License-Identifier: Apache-2.0 */
#include "lv_aic_apng_widget.h"
#define AIC_LVGL_USE_PRIVATE_API 1
#include "lvgl_aic_private.h"
#if defined(AIC_LVGL_USE_APNG_WIDGET) && AIC_LVGL_USE_APNG_WIDGET
#if !LV_USE_IMAGE
#error "APNG widget requires LV_USE_IMAGE"
#endif
#include <string.h>
#include <limits.h>
typedef struct binding binding_t;
typedef struct slave_binding slave_binding_t;
typedef struct { lv_aic_rgb_image_t *image;size_t owners; } frame_t;
struct binding {
    lv_obj_t *obj;
    lv_timer_t *timer;
    lv_aic_apng_playback_t *playback;
    frame_t *frame;
    slave_binding_t *slaves;
    lv_aic_apng_playback_options_t options;
    lv_aic_apng_playback_status_t status,reported;
    char path[128];
    uint32_t num,den;
    bool configured,closing,replace,start,paused;
};
typedef struct { lv_image_t image;binding_t *binding; } widget_t;
struct slave_binding {
    lv_obj_t *obj;
    lv_timer_t *timer;
    binding_t *master;
    slave_binding_t *next;
    frame_t *frame;
};
typedef struct { lv_image_t image;slave_binding_t *binding; } slave_widget_t;
static unsigned orphans;
static bool draws_idle(void)
{
    for(lv_display_t *d=lv_display_get_next(NULL);d;d=lv_display_get_next(d))
        for(lv_layer_t *l=d->layer_head;l;l=l->next) if(l->draw_task_head) return false;
    return true;
}
static void release_frame(frame_t *f)
{ if(f && --f->owners==0) { lv_aic_rgb_image_destroy(f->image);lv_free(f); } }
static void slave_frame(slave_binding_t *s,frame_t *f)
{
    if(s->frame==f) return;
    if(f) f->owners++;
    if(s->obj) lv_image_set_src(s->obj,f?lv_aic_rgb_image_source(f->image):NULL);
    release_frame(s->frame);s->frame=f;
}
static void publish_slaves(binding_t *b,frame_t *f)
{
    for(slave_binding_t *s=b->slaves;s;s=s->next) slave_frame(s,f);
}
static void retire(binding_t *b)
{
    publish_slaves(b,NULL);
    if(b->frame) {
        if(b->obj) lv_image_set_src(b->obj,NULL);
        release_frame(b->frame);b->frame=NULL;
    }
}
static void unlink_slave(slave_binding_t *s)
{
    if(s->master) {
        slave_binding_t **entry=&s->master->slaves;
        while(*entry && *entry!=s) entry=&(*entry)->next;
        if(*entry) *entry=s->next;
    }
    s->master=NULL;s->next=NULL;
}
static void slave_tick(lv_timer_t *timer)
{
    slave_binding_t *s=lv_timer_get_user_data(timer);
    if(!draws_idle()) return;
    slave_frame(s,s->obj && s->master?s->master->frame:NULL);
    if(!s->obj) { orphans--;lv_timer_delete(timer);lv_free(s); }
    else if(!s->master) lv_timer_pause(timer);
}
static void slave_destructor(const lv_obj_class_t *class_p,lv_obj_t *obj)
{
    (void)class_p;slave_binding_t *s=((slave_widget_t *)obj)->binding;
    if(!s) return;
    unlink_slave(s);s->obj=NULL;orphans++;lv_timer_resume(s->timer);
}
const lv_obj_class_t lv_aic_apng_slave_class={.base_class=&lv_image_class,.instance_size=sizeof(slave_widget_t),
    .destructor_cb=slave_destructor,.width_def=LV_SIZE_CONTENT,.height_def=LV_SIZE_CONTENT,.name="aic_apng_slave"};
lv_obj_t *lv_aic_apng_slave_create(lv_obj_t *parent)
{
    lv_obj_t *obj=lv_obj_class_create_obj(&lv_aic_apng_slave_class,parent);if(!obj) return NULL;
    lv_obj_class_init_obj(obj);slave_binding_t *s=lv_malloc_zeroed(sizeof(*s));
    if(!s) { lv_obj_delete(obj);return NULL; }
    s->timer=lv_timer_create(slave_tick,10,s);
    if(!s->timer) { lv_free(s);lv_obj_delete(obj);return NULL; }
    s->obj=obj;((slave_widget_t *)obj)->binding=s;lv_timer_pause(s->timer);return obj;
}
lv_result_t lv_aic_apng_slave_set_master(lv_obj_t *obj,lv_obj_t *master)
{
    LV_CHECK_OBJ(obj,&lv_aic_apng_slave_class,return LV_RESULT_INVALID);
    if(master) { LV_CHECK_OBJ(master,&lv_aic_apng_class,return LV_RESULT_INVALID); }
    slave_binding_t *s=((slave_widget_t *)obj)->binding;unlink_slave(s);
    if(master) {
        s->master=((widget_t *)master)->binding;s->next=s->master->slaves;s->master->slaves=s;
    }
    lv_timer_resume(s->timer);return LV_RESULT_OK;
}
lv_obj_t *lv_aic_apng_slave_get_master(lv_obj_t *obj)
{
    LV_CHECK_OBJ(obj,&lv_aic_apng_slave_class,return NULL);
    slave_binding_t *s=((slave_widget_t *)obj)->binding;return s->master?s->master->obj:NULL;
}
static void tick(lv_timer_t *timer)
{
    binding_t *b=lv_timer_get_user_data(timer);
    if(!draws_idle()) return;
    if(!b->obj || b->closing) {
        retire(b);
        if(!lv_aic_apng_playback_destroy(b->playback)) return;
        b->playback=NULL;b->closing=false;
        if(!b->obj) { orphans--;lv_timer_delete(timer);lv_free(b);return; }
        b->status=lv_aic_apng_playback_status(NULL);
        if(!b->replace) lv_timer_pause(timer);
    }
    if(b->replace && !b->playback) {
        b->replace=false;
        b->playback=lv_aic_apng_playback_prepare(b->path,&b->options);
        if(!b->playback) { b->status.state=LV_AIC_APNG_FAULT;lv_timer_pause(timer); }
        else if(!lv_aic_apng_playback_rate(b->playback,b->num,b->den) ||
                !lv_aic_apng_playback_pause(b->playback,b->paused) ||
                (b->start && !lv_aic_apng_playback_start(b->playback))) {
            lv_aic_apng_playback_close(b->playback);
            b->status.state=LV_AIC_APNG_FAULT;
        }
    }
    if(b->playback && b->status.state!=LV_AIC_APNG_FAULT) {
        b->status=lv_aic_apng_playback_status(b->playback);
        if(b->status.state==LV_AIC_APNG_FAULT) retire(b);
        else {
            lv_aic_rgb_image_t *next=NULL;uint64_t sequence;
            if(lv_aic_apng_playback_poll(b->playback,&next,&sequence)) {
                frame_t *frame=lv_malloc(sizeof(*frame));
                if(frame) {
                    *frame=(frame_t){.image=next,.owners=1};frame_t *old=b->frame;b->frame=frame;
                    lv_image_set_src(b->obj,lv_aic_rgb_image_source(next));
                    publish_slaves(b,frame);release_frame(old);
                } else lv_aic_rgb_image_destroy(next);
            }
        }
    }
    bool changed=b->status.state!=b->reported.state || b->status.rate_num!=b->reported.rate_num ||
        b->status.rate_den!=b->reported.rate_den || b->status.restarts!=b->reported.restarts;
    if(changed) {
        b->reported=b->status;
        /* Final action: event may delete object or queue a replacement. */
        lv_obj_send_event(b->obj,LV_EVENT_VALUE_CHANGED,NULL);
    }
}
static void destructor(const lv_obj_class_t *class_p,lv_obj_t *obj)
{
    (void)class_p;binding_t *b=((widget_t *)obj)->binding;if(!b) return;
    while(b->slaves) {
        slave_binding_t *s=b->slaves;unlink_slave(s);lv_timer_resume(s->timer);
    }
    b->obj=NULL;orphans++;lv_aic_apng_playback_close(b->playback);lv_timer_resume(b->timer);
}
const lv_obj_class_t lv_aic_apng_class={.base_class=&lv_image_class,.instance_size=sizeof(widget_t),
    .destructor_cb=destructor,.width_def=LV_SIZE_CONTENT,.height_def=LV_SIZE_CONTENT,.name="aic_apng"};
lv_obj_t *lv_aic_apng_create(lv_obj_t *parent)
{
    lv_obj_t *obj=lv_obj_class_create_obj(&lv_aic_apng_class,parent);if(!obj) return NULL;
    lv_obj_class_init_obj(obj);binding_t *b=lv_malloc_zeroed(sizeof(*b));
    if(!b) { lv_obj_delete(obj);return NULL; }
    b->timer=lv_timer_create(tick,10,b);
    if(!b->timer) { lv_free(b);lv_obj_delete(obj);return NULL; }
    b->obj=obj;b->num=b->den=1;b->status=b->reported=lv_aic_apng_playback_status(NULL);
    ((widget_t *)obj)->binding=b;lv_timer_pause(b->timer);return obj;
}
lv_result_t lv_aic_apng_configure(lv_obj_t *obj,const lv_aic_apng_playback_options_t *o)
{
    LV_CHECK_OBJ(obj,&lv_aic_apng_class,return LV_RESULT_INVALID);
    binding_t *b=((widget_t *)obj)->binding;
    if(b->playback || b->replace || b->closing || !o || !o->limits.file_bytes || o->limits.file_bytes>LONG_MAX ||
       !o->limits.frame_png_bytes || !o->limits.canvas_pixels || !o->limits.frames ||
       !o->stream_budget || !o->snapshot_budget || !o->cma_budget || o->packet_limit<256 ||
       o->packet_limit>INT_MAX-255U || o->snapshots<2 || o->snapshots>8 ||
       !o->minimum_delay_us || o->minimum_delay_us>1000000) return LV_RESULT_INVALID;
    b->options=*o;b->configured=true;return LV_RESULT_OK;
}
lv_result_t lv_aic_apng_set_src(lv_obj_t *obj,const char *path)
{
    LV_CHECK_OBJ(obj,&lv_aic_apng_class,return LV_RESULT_INVALID);
    binding_t *b=((widget_t *)obj)->binding;
    if(!b->configured || !path || !path[0] || strlen(path)>=sizeof(b->path)) return LV_RESULT_INVALID;
    memcpy(b->path,path,strlen(path)+1);b->replace=true;b->start=false;b->paused=false;
    b->closing=true;b->status.state=LV_AIC_APNG_CLOSING;
    lv_aic_apng_playback_close(b->playback);lv_timer_resume(b->timer);return LV_RESULT_OK;
}
lv_result_t lv_aic_apng_start(lv_obj_t *obj)
{
    LV_CHECK_OBJ(obj,&lv_aic_apng_class,return LV_RESULT_INVALID);
    binding_t *b=((widget_t *)obj)->binding;
    if(!b->configured || !b->path[0] || b->status.state==LV_AIC_APNG_FAULT) return LV_RESULT_INVALID;
    if(!b->playback || b->closing) { b->replace=true;b->start=true;lv_timer_resume(b->timer);return LV_RESULT_OK; }
    if(lv_aic_apng_playback_status(b->playback).state==LV_AIC_APNG_TERMINAL &&
       !lv_aic_apng_playback_restart(b->playback)) return LV_RESULT_INVALID;
    if(!lv_aic_apng_playback_start(b->playback)) return LV_RESULT_INVALID;
    b->start=true;return LV_RESULT_OK;
}
lv_result_t lv_aic_apng_pause(lv_obj_t *obj,bool paused)
{
    LV_CHECK_OBJ(obj,&lv_aic_apng_class,return LV_RESULT_INVALID);
    binding_t *b=((widget_t *)obj)->binding;
    if(b->status.state==LV_AIC_APNG_FAULT || (!b->playback && !b->replace)) return LV_RESULT_INVALID;
    if(b->playback && !b->closing && !lv_aic_apng_playback_pause(b->playback,paused)) return LV_RESULT_INVALID;
    b->paused=paused;return LV_RESULT_OK;
}
lv_result_t lv_aic_apng_set_rate(lv_obj_t *obj,uint32_t n,uint32_t d)
{
    LV_CHECK_OBJ(obj,&lv_aic_apng_class,return LV_RESULT_INVALID);
    binding_t *b=((widget_t *)obj)->binding;
    if(!n || !d || n>1000000 || d>1000000 || (uint64_t)n*10<d || (uint64_t)d*10<n) return LV_RESULT_INVALID;
    if(b->playback && !b->closing && !lv_aic_apng_playback_rate(b->playback,n,d)) return LV_RESULT_INVALID;
    b->num=n;b->den=d;return LV_RESULT_OK;
}
lv_result_t lv_aic_apng_restart(lv_obj_t *obj)
{
    LV_CHECK_OBJ(obj,&lv_aic_apng_class,return LV_RESULT_INVALID);
    binding_t *b=((widget_t *)obj)->binding;
    return !b->closing && lv_aic_apng_playback_restart(b->playback)?LV_RESULT_OK:LV_RESULT_INVALID;
}
lv_result_t lv_aic_apng_close(lv_obj_t *obj)
{
    LV_CHECK_OBJ(obj,&lv_aic_apng_class,return LV_RESULT_INVALID);
    binding_t *b=((widget_t *)obj)->binding;
    b->replace=b->start=false;b->closing=true;b->status.state=LV_AIC_APNG_CLOSING;
    lv_aic_apng_playback_close(b->playback);lv_timer_resume(b->timer);return LV_RESULT_OK;
}
lv_aic_apng_playback_status_t lv_aic_apng_get_status(lv_obj_t *obj)
{
    LV_CHECK_OBJ(obj,&lv_aic_apng_class,return lv_aic_apng_playback_status(NULL));
    return ((widget_t *)obj)->binding->status;
}
unsigned lv_aic_apng_pending_cleanup(void) { return orphans; }
#endif
