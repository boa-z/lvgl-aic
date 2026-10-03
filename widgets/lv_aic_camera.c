/* SPDX-License-Identifier: Apache-2.0 */
#include "lv_aic_camera.h"
#define AIC_LVGL_USE_PRIVATE_API 1
#include "lvgl_aic_private.h"
#if defined(AIC_LVGL_USE_CAMERA) && AIC_LVGL_USE_CAMERA
#include <string.h>
typedef struct {
    lv_obj_t *obj;
    lv_timer_t *timer;
    lv_aic_camera_capture_t *capture;
    lv_aic_yuv_image_t *image;
    char device[16];
    uint32_t queue, desired_input;
    lv_aic_camera_input_status_t input_reported;
    bool have_input;
    lv_aic_yuv_color_space_t space;
    lv_aic_camera_format format;
    lv_aic_camera_state_t state, reported;
    bool configured, closing, stopped;
} camera_binding_t;
typedef struct { lv_image_t image; camera_binding_t *binding; } camera_widget_t;
static unsigned orphan_count;
/* Do not retire a source that a queued (not yet opened) draw task can reference.
 * Timers normally run between refreshes. Conservatively defer even for another
 * display's pending work; never wait/spin on a quarantined GE task from UI. */
static bool draws_idle(void)
{
    for(lv_display_t *d=lv_display_get_next(NULL);d;d=lv_display_get_next(d))
        for(lv_layer_t *l=d->layer_head;l;l=l->next)
            if(l->draw_task_head) return false;
    return true;
}
static void retire_image(camera_binding_t *b)
{
    if(b->image) { lv_aic_yuv_image_destroy(b->image); b->image=NULL; }
}
static void tick(lv_timer_t *timer)
{
    camera_binding_t *b=lv_timer_get_user_data(timer);
    if(!draws_idle()) return;
    if(!b->obj || b->closing) {
        if(b->obj && b->image) lv_image_set_src(b->obj,NULL);
        retire_image(b);
        if(!lv_aic_camera_capture_destroy(b->capture)) return;
        b->capture=NULL; b->closing=false;
        if(!b->obj) {
            orphan_count--; lv_timer_delete(timer); lv_free(b); return;
        }
        b->state=b->stopped ? LV_AIC_CAMERA_STOPPED : LV_AIC_CAMERA_CLOSED;
        lv_timer_pause(timer);
    }
    else if(b->capture) {
        lv_aic_capture_state_t state=lv_aic_camera_capture_state(b->capture);
        switch(state) {
        case LV_AIC_CAPTURE_OPENING: b->state=LV_AIC_CAMERA_OPENING; break;
        case LV_AIC_CAPTURE_READY: b->state=LV_AIC_CAMERA_READY; break;
        case LV_AIC_CAPTURE_RUNNING: b->state=LV_AIC_CAMERA_RUNNING; break;
        case LV_AIC_CAPTURE_PAUSED: b->state=LV_AIC_CAMERA_PAUSED; break;
        case LV_AIC_CAPTURE_FAULT: b->state=LV_AIC_CAMERA_FAULT; break;
        default: break;
        }
        if(state==LV_AIC_CAPTURE_FAULT) {
            /* Fault already requested worker shutdown; preserve FAULT for the
             * caller until explicit close/stop. Release its last displayed frame. */
            if(b->image) lv_image_set_src(b->obj,NULL);
            retire_image(b);
        }
        else if(state==LV_AIC_CAPTURE_RUNNING) {
            lv_aic_yuv_image_t *next=lv_aic_camera_capture_poll(b->capture);
            if(next) {
                lv_aic_yuv_image_t *old=b->image;
                b->image=next;
                lv_image_set_src(b->obj,lv_aic_yuv_image_source(next));
                if(old) lv_aic_yuv_image_destroy(old);
            }
        }
    }
    /* This must be the final action: event handlers may delete the object or
     * request a different transport state. The binding survives until next tick. */
    lv_aic_camera_input_status_t input=lv_aic_camera_capture_get_input(b->capture);
    bool input_changed=input.state!=b->input_reported.state ||
        input.sequence!=b->input_reported.sequence || input.applied!=b->input_reported.applied;
    if(b->obj && (b->state!=b->reported || input_changed)) {
        b->input_reported=input;
        b->reported=b->state;
        lv_obj_send_event(b->obj,LV_EVENT_VALUE_CHANGED,NULL);
    }
}
static void destructor(const lv_obj_class_t *class_p,lv_obj_t *obj)
{
    (void)class_p;
    camera_binding_t *b=((camera_widget_t *)obj)->binding;
    if(!b) return;
    b->obj=NULL; orphan_count++;
    lv_aic_camera_capture_close(b->capture);
    lv_timer_resume(b->timer);
    /* Image superclass borrows variable sources. Keep the owner alive until
     * pending tasks have opened/completed, then retire it from tick(). */
}
const lv_obj_class_t lv_aic_camera_class={
    .base_class=&lv_image_class, .instance_size=sizeof(camera_widget_t),
    .destructor_cb=destructor, .width_def=LV_SIZE_CONTENT,
    .height_def=LV_SIZE_CONTENT, .name="aic_camera",
};
lv_obj_t *lv_aic_camera_create(lv_obj_t *parent)
{
    lv_obj_t *obj=lv_obj_class_create_obj(&lv_aic_camera_class,parent);
    if(!obj) return NULL;
    lv_obj_class_init_obj(obj);
    camera_binding_t *b=lv_malloc_zeroed(sizeof(*b));
    if(!b) { lv_obj_delete(obj); return NULL; }
    b->timer=lv_timer_create(tick,20,b);
    if(!b->timer) { lv_free(b); lv_obj_delete(obj); return NULL; }
    b->obj=obj; memcpy(b->device,"camera",7);
    b->input_reported=lv_aic_camera_capture_get_input(NULL);
    ((camera_widget_t *)obj)->binding=b;
    lv_timer_pause(b->timer); return obj;
}
lv_result_t lv_aic_camera_configure(lv_obj_t *obj,const char *device,
    uint32_t queue,lv_aic_yuv_color_space_t space)
{
    LV_CHECK_OBJ(obj,&lv_aic_camera_class,return LV_RESULT_INVALID);
    camera_binding_t *b=((camera_widget_t *)obj)->binding;
    if(b->capture || b->closing || !device || !device[0] || strlen(device)>=sizeof(b->device) ||
       space<LV_AIC_YUV_BT601_LIMITED || space>LV_AIC_YUV_BT709_FULL) return LV_RESULT_INVALID;
    memcpy(b->device,device,strlen(device)+1); b->queue=queue; b->space=space;
    b->configured=true; b->have_input=false; return LV_RESULT_OK;
}
lv_result_t lv_aic_camera_set_format(lv_obj_t *obj,lv_aic_camera_format format)
{
    LV_CHECK_OBJ(obj,&lv_aic_camera_class,return LV_RESULT_INVALID);
    camera_binding_t *b=((camera_widget_t *)obj)->binding;
    if(b->capture || b->closing || format<0 || format>=_LV_AIC_CAMERA_FORMAT_LAST) return LV_RESULT_INVALID;
    b->format=format; return LV_RESULT_OK;
}
lv_result_t lv_aic_camera_open(lv_obj_t *obj)
{
    LV_CHECK_OBJ(obj,&lv_aic_camera_class,return LV_RESULT_INVALID);
    camera_binding_t *b=((camera_widget_t *)obj)->binding;
    if(!b->configured || b->capture || b->closing) return LV_RESULT_INVALID;
    lv_aic_yuv_format_t format=b->format==LV_AIC_CAMERA_FORMAT_NV16 ? LV_AIC_YUV_NV16 :
        b->format==LV_AIC_CAMERA_FORMAT_NV12 ? LV_COLOR_FORMAT_NV12 : LV_COLOR_FORMAT_I400;
    b->capture=lv_aic_camera_capture_prepare(b->device,b->queue,format,b->space);
    if(!b->capture) return LV_RESULT_INVALID;
    if(b->have_input && !lv_aic_camera_capture_select_input(b->capture,b->desired_input)) {
        lv_aic_camera_capture_close(b->capture);
        b->closing=true; b->stopped=false; b->state=LV_AIC_CAMERA_STOPPING;
        lv_timer_resume(b->timer); return LV_RESULT_INVALID;
    }
    b->state=LV_AIC_CAMERA_OPENING; b->stopped=false;
    lv_timer_resume(b->timer); return LV_RESULT_OK;
}
lv_result_t lv_aic_camera_start(lv_obj_t *obj)
{
    LV_CHECK_OBJ(obj,&lv_aic_camera_class,return LV_RESULT_INVALID);
    camera_binding_t *b=((camera_widget_t *)obj)->binding;
    if(b->state==LV_AIC_CAMERA_STOPPED && lv_aic_camera_open(obj)!=LV_RESULT_OK) return LV_RESULT_INVALID;
    return !b->closing && lv_aic_camera_capture_start(b->capture) ? LV_RESULT_OK : LV_RESULT_INVALID;
}
int lv_aic_camera_set_channel(lv_obj_t *obj,uint32_t input)
{
    LV_CHECK_OBJ(obj,&lv_aic_camera_class,return -1);
    camera_binding_t *b=((camera_widget_t *)obj)->binding;
    if(b->closing || !lv_aic_camera_capture_select_input(b->capture,input)) return -1;
    b->desired_input=input; b->have_input=true; return 0;
}
lv_aic_camera_input_status_t lv_aic_camera_get_channel_status(lv_obj_t *obj)
{
    LV_CHECK_OBJ(obj,&lv_aic_camera_class,return lv_aic_camera_capture_get_input(NULL));
    return lv_aic_camera_capture_get_input(((camera_widget_t *)obj)->binding->capture);
}
uint32_t lv_aic_camera_get_channel(lv_obj_t *obj)
{ return lv_aic_camera_get_channel_status(obj).applied; }
static lv_result_t close_request(lv_obj_t *obj,bool stopped)
{
    LV_CHECK_OBJ(obj,&lv_aic_camera_class,return LV_RESULT_INVALID);
    camera_binding_t *b=((camera_widget_t *)obj)->binding;
    b->stopped=stopped; b->closing=true; b->state=LV_AIC_CAMERA_STOPPING;
    lv_aic_camera_capture_close(b->capture);
    lv_timer_resume(b->timer); return LV_RESULT_OK;
}
lv_result_t lv_aic_camera_stop(lv_obj_t *obj) { return close_request(obj,true); }
lv_result_t lv_aic_camera_close(lv_obj_t *obj) { return close_request(obj,false); }
static lv_result_t pause_request(lv_obj_t *obj,bool pause)
{
    LV_CHECK_OBJ(obj,&lv_aic_camera_class,return LV_RESULT_INVALID);
    camera_binding_t *b=((camera_widget_t *)obj)->binding;
    if(!b->capture || b->closing || b->state==LV_AIC_CAMERA_FAULT) return LV_RESULT_INVALID;
    lv_aic_camera_capture_pause(b->capture,pause); return LV_RESULT_OK;
}
lv_result_t lv_aic_camera_pause(lv_obj_t *obj) { return pause_request(obj,true); }
lv_result_t lv_aic_camera_resume(lv_obj_t *obj) { return pause_request(obj,false); }
lv_aic_camera_state_t lv_aic_camera_get_state(lv_obj_t *obj)
{
    LV_CHECK_OBJ(obj,&lv_aic_camera_class,return LV_AIC_CAMERA_FAULT);
    return ((camera_widget_t *)obj)->binding->state;
}
unsigned lv_aic_camera_pending_cleanup(void) { return orphan_count; }
#endif
