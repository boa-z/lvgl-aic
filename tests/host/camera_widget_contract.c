/* SPDX-License-Identifier: Apache-2.0 */
#define AIC_LVGL_USE_PRIVATE_API 1
#include "lv_aic_camera.h"
#include "lv_aic_yuv_image_private.h"
#include "lvgl_aic_private.h"
#include <assert.h>
#include <stdlib.h>
#include <string.h>
struct lv_aic_camera_capture { lv_aic_capture_state_t state; lv_aic_camera_input_status_t input; unsigned readers; bool closing; };
static lv_aic_camera_capture_t *active;
static unsigned created, freed, retained, released, frames;
static bool allow_exit=true, fail_prepare;
static uint8_t luma;
typedef struct { lv_aic_camera_capture_t *capture; uint8_t y[16],uv[16]; } producer_t;
lv_aic_camera_capture_t *lv_aic_camera_capture_prepare(const char *device,uint32_t queue,
    lv_aic_yuv_format_t format,lv_aic_yuv_color_space_t space)
{
    assert(!strcmp(device,"camera") && queue==0);
    assert(format==LV_AIC_YUV_NV16 && space==LV_AIC_YUV_BT601_LIMITED);
    if(active || fail_prepare) return NULL;
    active=calloc(1,sizeof(*active)); assert(active); created++;
    active->state=LV_AIC_CAPTURE_OPENING; active->input=lv_aic_camera_capture_get_input(NULL); return active;
}
lv_aic_camera_input_status_t lv_aic_camera_capture_get_input(lv_aic_camera_capture_t *c)
{ return c ? c->input : (lv_aic_camera_input_status_t){.state=LV_AIC_INPUT_NONE,
    .requested=UINT32_MAX,.applied=UINT32_MAX}; }
bool lv_aic_camera_capture_select_input(lv_aic_camera_capture_t *c,uint32_t input)
{
    if(!c || c->closing || input>3 || c->input.state==LV_AIC_INPUT_PENDING) return false;
    c->input.state=LV_AIC_INPUT_PENDING; c->input.requested=input; c->input.sequence++; return true;
}
bool lv_aic_camera_capture_start(lv_aic_camera_capture_t *c)
{ if(!c || c->closing) return false; c->state=LV_AIC_CAPTURE_RUNNING; return true; }
void lv_aic_camera_capture_pause(lv_aic_camera_capture_t *c,bool pause)
{ assert(c); c->state=pause ? LV_AIC_CAPTURE_PAUSED : LV_AIC_CAPTURE_RUNNING; }
lv_aic_capture_state_t lv_aic_camera_capture_state(lv_aic_camera_capture_t *c)
{ return c ? c->state : LV_AIC_CAPTURE_CLOSED; }
void lv_aic_camera_capture_close(lv_aic_camera_capture_t *c)
{ if(c) { c->closing=true; c->state=LV_AIC_CAPTURE_CLOSING; } }
bool lv_aic_camera_capture_destroy(lv_aic_camera_capture_t *c)
{
    if(!c) return true;
    if(!c->closing || c->readers || !allow_exit) return false;
    assert(c==active); active=NULL; free(c); freed++; return true;
}
static bool retain(void *p)
{ producer_t *owner=p; owner->capture->readers++; retained++; return true; }
static void release(void *p)
{ producer_t *owner=p; assert(owner->capture->readers); owner->capture->readers--; released++; free(owner); }
lv_aic_yuv_image_t *lv_aic_camera_capture_poll(lv_aic_camera_capture_t *c)
{
    if(!frames || c->input.state==LV_AIC_INPUT_PENDING) return NULL;
    frames--;
    producer_t *p=calloc(1,sizeof(*p)); assert(p); p->capture=c;
    memset(p->y,luma,sizeof(p->y)); memset(p->uv,128,sizeof(p->uv));
    lv_aic_yuv_frame_t f={.width=4,.height=4,.format=LV_AIC_YUV_NV16,
        .color_space=LV_AIC_YUV_BT601_LIMITED,
        .planes={{p->y,4,16},{p->uv,4,16}}};
    lv_aic_yuv_image_t *image=lv_aic_yuv_image_create(&f,retain,release,p);
    if(!image) free(p);
    return image;
}
static void tick(void) { lv_tick_inc(25); lv_timer_handler(); }
static void flush(lv_display_t *d,const lv_area_t *a,uint8_t *p)
{ (void)a; (void)p; lv_display_flush_ready(d); }
static void delete_on_state(lv_event_t *e) { lv_obj_delete(lv_event_get_target_obj(e)); }
static lv_obj_t *make(lv_obj_t *screen)
{
    lv_obj_t *obj=lv_aic_camera_create(screen); assert(obj);
    assert(lv_aic_camera_open(obj)==LV_RESULT_INVALID); /* Colorimetry is required. */
    assert(lv_aic_camera_configure(obj,"camera",0,LV_AIC_YUV_BT601_LIMITED)==LV_RESULT_OK);
    return obj;
}
int main(void)
{
    lv_init(); assert(lv_aic_yuv_image_decoder_init());
    static uint8_t pixels[16*16*3];
    lv_display_t *d=lv_display_create(16,16);
    lv_display_set_color_format(d,LV_COLOR_FORMAT_RGB888);
    lv_display_set_buffers(d,pixels,NULL,sizeof(pixels),LV_DISPLAY_RENDER_MODE_DIRECT);
    lv_display_set_flush_cb(d,flush);
    lv_timer_pause(lv_display_get_refr_timer(d));
    lv_obj_t *screen=lv_screen_active();
    lv_obj_t *obj=make(screen);
    fail_prepare=true; assert(lv_aic_camera_open(obj)==LV_RESULT_INVALID); fail_prepare=false;
    assert(lv_aic_camera_open(obj)==LV_RESULT_OK);
    assert(lv_aic_camera_open(obj)==LV_RESULT_INVALID);
    assert(lv_aic_camera_set_format(obj,LV_AIC_CAMERA_FORMAT_NV12)==LV_RESULT_INVALID);
    active->state=LV_AIC_CAPTURE_READY; tick();
    assert(lv_aic_camera_get_state(obj)==LV_AIC_CAMERA_READY);
    assert(lv_aic_camera_get_channel(obj)==UINT32_MAX);
    assert(lv_aic_camera_set_channel(obj,4)==-1);
    assert(lv_aic_camera_set_channel(obj,LV_AIC_CAMERA_CH_VIN2)==0);
    assert(lv_aic_camera_set_channel(obj,LV_AIC_CAMERA_CH_VIN1)==-1);
    assert(active->input.requested==2); tick();
    assert(lv_aic_camera_get_channel_status(obj).state==LV_AIC_INPUT_PENDING);
    active->input.state=LV_AIC_INPUT_APPLIED; active->input.applied=2; tick();
    assert(lv_aic_camera_get_channel(obj)==2);
    assert(lv_aic_camera_start(obj)==LV_RESULT_OK);
    lv_obj_set_pos(obj,0,0);
    for(unsigned i=0;i<20;i++) {
        
        frames=1; luma=i&1 ? 235 : 16; tick(); lv_refr_now(d);
        assert(lv_aic_camera_get_state(obj)==LV_AIC_CAMERA_RUNNING);
        for(unsigned y=0;y<4;y++) for(unsigned x=0;x<12;x++)
            assert(pixels[y*48+x]==(i&1 ? 255 : 0));
        assert(retained-released==1);
    }
    assert(lv_aic_camera_pause(obj)==LV_RESULT_OK); tick();
    assert(lv_aic_camera_get_state(obj)==LV_AIC_CAMERA_PAUSED);
    assert(lv_aic_camera_resume(obj)==LV_RESULT_OK); tick();
    const lv_aic_yuv_frame_t *view;
    lv_aic_yuv_image_t *reader=lv_aic_yuv_image_acquire(lv_image_get_src(obj),&view); assert(reader);
    assert(lv_aic_camera_stop(obj)==LV_RESULT_OK); tick();
    assert(active && lv_aic_camera_get_state(obj)==LV_AIC_CAMERA_STOPPING);
    assert(lv_aic_camera_start(obj)==LV_RESULT_INVALID);
    assert(view->planes[0].data[0]==235); /* Stop cannot free a reader's storage. */
    lv_aic_yuv_image_release_lease(reader); tick();
    assert(!active && lv_aic_camera_get_state(obj)==LV_AIC_CAMERA_STOPPED);
    assert(lv_aic_camera_start(obj)==LV_RESULT_OK); tick(); assert(active);
    assert(active->input.state==LV_AIC_INPUT_PENDING && active->input.requested==2);
    active->input.state=LV_AIC_INPUT_APPLIED; active->input.applied=2; tick();
    frames=1; tick();
    /* Simulate a queued, not-yet-opened draw reference: deletion must retain
     * source until a later idle timer pass, without waiting on UI. */
    lv_draw_task_t pending={0}; d->layer_head->draw_task_head=&pending;
    lv_obj_delete(obj); assert(lv_aic_camera_pending_cleanup()==1);
    lv_timer_pause(lv_display_get_refr_timer(d));
    tick(); assert(active && retained-released==1);
    d->layer_head->draw_task_head=NULL; allow_exit=false;
    tick(); assert(active && retained==released && lv_aic_camera_pending_cleanup()==1);
    allow_exit=true; tick(); assert(!active && !lv_aic_camera_pending_cleanup());
    /* Deletion from notification must not access a freed widget afterwards. */
    obj=make(screen); assert(lv_aic_camera_open(obj)==LV_RESULT_OK);
    lv_obj_add_event_cb(obj,delete_on_state,LV_EVENT_VALUE_CHANGED,NULL);
    active->state=LV_AIC_CAPTURE_READY; tick();
    assert(lv_aic_camera_pending_cleanup()==1); tick();
    assert(!active && !lv_aic_camera_pending_cleanup());
    /* An input-only status change also notifies and supports callback deletion. */
    obj=make(screen); assert(lv_aic_camera_open(obj)==LV_RESULT_OK);
    assert(lv_aic_camera_start(obj)==LV_RESULT_OK); tick();
    lv_obj_add_event_cb(obj,delete_on_state,LV_EVENT_VALUE_CHANGED,NULL);
    assert(lv_aic_camera_set_channel(obj,1)==0); tick();
    assert(lv_aic_camera_pending_cleanup()==1); tick();
    assert(!active && !lv_aic_camera_pending_cleanup());
    /* Device fault detaches pixels but remains observable until explicit close. */
    obj=make(screen); assert(lv_aic_camera_open(obj)==LV_RESULT_OK);
    assert(lv_aic_camera_start(obj)==LV_RESULT_OK); frames=1; tick();
    active->state=LV_AIC_CAPTURE_FAULT; active->closing=true; tick();
    assert(lv_aic_camera_get_state(obj)==LV_AIC_CAMERA_FAULT);
    assert(!lv_image_get_src(obj) && retained==released);
    assert(lv_aic_camera_pause(obj)==LV_RESULT_INVALID);
    assert(lv_aic_camera_close(obj)==LV_RESULT_OK); tick(); lv_obj_delete(obj); tick();
    /* Closing before start, and deleting an unopened widget need no worker wait. */
    obj=make(screen); assert(lv_aic_camera_open(obj)==LV_RESULT_OK);
    assert(lv_aic_camera_close(obj)==LV_RESULT_OK); tick();
    assert(lv_aic_camera_get_state(obj)==LV_AIC_CAMERA_CLOSED);
    lv_obj_delete(obj); tick();
    obj=make(screen); lv_obj_delete(obj); tick();
    assert(created==freed && retained==released && !lv_aic_camera_pending_cleanup());
    lv_display_delete(d); assert(lv_aic_yuv_image_decoder_deinit()); lv_deinit();
    return 0;
}
