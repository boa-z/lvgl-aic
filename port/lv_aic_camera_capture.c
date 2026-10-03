/* SPDX-License-Identifier: Apache-2.0 */
#if defined(AIC_LVGL_USE_VIN) && AIC_LVGL_USE_VIN
#include "lv_aic_camera_capture.h"
#include "lv_aic_vin_frame.h"
#include <aic_osal.h>
#include <drv_camera.h>
#include <string.h>

typedef enum { EMPTY, READY, PUBLISHING, PUBLISHED, RETURN } slot_state_t;
typedef struct {
    struct lv_aic_camera_capture *owner;
    lv_aic_yuv_frame_t frame;
    slot_state_t state;
} frame_slot_t;
struct lv_aic_camera_capture {
    aicos_mutex_t mutex;
    lv_aic_vin_session_t vin;
    frame_slot_t slots[VIN_MAX_BUF_NUM];
    char camera[16];
    uint32_t channel;
    enum mpp_pixel_format format;
    lv_aic_yuv_color_space_t space;
    lv_aic_capture_state_t state;
    lv_aic_camera_input_status_t input;
    bool input_inflight;
    bool closing, pause_requested, start_requested, finished;
};
/* UI-owned singleton; prevents competing worker threads entering SDK globals. */
static lv_aic_camera_capture_t *active;
static void lock(lv_aic_camera_capture_t *c) { aicos_mutex_take(c->mutex,AICOS_WAIT_FOREVER); }
static void unlock(lv_aic_camera_capture_t *c) { aicos_mutex_give(c->mutex); }
static void fault(lv_aic_camera_capture_t *c)
{
    lock(c); c->closing=true; c->state=LV_AIC_CAPTURE_FAULT;
    if(c->input.state==LV_AIC_INPUT_PENDING && !c->input_inflight)
        c->input.state=LV_AIC_INPUT_CANCELLED;
    unlock(c);
}
static bool retain_slot(void *context)
{
    frame_slot_t *s=context;
    lock(s->owner);
    bool valid=s->state==PUBLISHING;
    if(valid) s->state=PUBLISHED;
    unlock(s->owner); return valid;
}
static void release_slot(void *context)
{
    frame_slot_t *s=context;
    lock(s->owner); s->state=RETURN; unlock(s->owner);
}
static bool invalidate_frame(const lv_aic_yuv_frame_t *f)
{
    lv_aic_yuv_layout_t layout;
    if(!lv_aic_yuv_layout(f->format,f->width,f->height,&layout)) return false;
    for(unsigned p=0;p<layout.planes;p++) {
        uintptr_t start=(uintptr_t)f->planes[p].data;
        uint64_t end=(uint64_t)start+(uint64_t)f->planes[p].stride*layout.rows[p];
        if(end>UINT32_MAX-31U) return false;
        /* VIN owns aligned CMA allocation including a cache-line tail. All
         * shared planes are immutable for CPU readers; never clean stale data. */
        uintptr_t aligned=start&~(uintptr_t)31;
        aicos_dcache_invalid_range((unsigned long *)aligned,(unsigned long)(((end+31)&~UINT64_C(31))-aligned));
    }
    return true;
}
static void worker(void *argument)
{
    lv_aic_camera_capture_t *c=argument;
    if(!lv_aic_vin_open(&c->vin,c->camera,c->channel,c->format,3)) fault(c);
    for(;;) {
        lock(c);
        bool closing=c->closing, paused=c->pause_requested, start=c->start_requested;
        if(closing) for(unsigned i=0;i<VIN_MAX_BUF_NUM;i++)
            if(c->slots[i].state==READY) c->slots[i].state=RETURN;
        unlock(c);
        /* Only the worker ever calls Q_BUF, including publication failures. */
        for(unsigned i=0;i<VIN_MAX_BUF_NUM;i++) {
            lock(c); bool give_back=c->slots[i].state==RETURN; unlock(c);
            if(give_back) {
                if(!lv_aic_vin_release(&c->vin,i)) { fault(c); closing=true; }
                else { lock(c); c->slots[i].state=EMPTY; unlock(c); }
            }
        }
        if(closing) {
            if(lv_aic_vin_close(&c->vin)) {
                lock(c);
                if(c->state!=LV_AIC_CAPTURE_FAULT) c->state=LV_AIC_CAPTURE_CLOSED;
                c->finished=true;
                unlock(c);
                /* No access to c after unlock: UI may now destroy it. */
                return;
            }
            aicos_msleep(5); continue;
        }
        /* Sensor control runs only on the VIN owner, serialized with dequeue
         * and teardown. Do not hold the mailbox mutex across a driver call. */
        lock(c);
        bool select=!c->closing && c->input.state==LV_AIC_INPUT_PENDING;
        uint32_t input=c->input.requested;
        if(select) c->input_inflight=true;
        unlock(c);
        if(select) {
            int result=camera_set_channel(c->vin.device.camera_dev,input);
            lock(c);
            c->input_inflight=false;
            c->input.state=result<0 ? LV_AIC_INPUT_FAILED : LV_AIC_INPUT_APPLIED;
            c->input.applied=result<0 ? UINT32_MAX : input;
            /* Publish failure and close atomically: a UI request must not
             * overwrite FAILED in the gap before shutdown becomes visible. */
            if(result<0) { c->closing=true; c->state=LV_AIC_CAPTURE_FAULT; }
            closing=c->closing;
            unlock(c);
            /* A failed driver call may have partially changed sensor state.
             * Stop publishing; retain/return frames through normal shutdown. */
            if(closing) continue;
        }
        if(!c->vin.streaming) {
            if(!start) {
                lock(c);
                if(!c->closing) c->state=LV_AIC_CAPTURE_READY;
                unlock(c);
                aicos_msleep(5); continue;
            }
            if(!lv_aic_vin_start(&c->vin)) { fault(c); continue; }
        }
        if(paused!=c->vin.paused) {
            if(!(paused ? lv_aic_vin_pause(&c->vin) : lv_aic_vin_resume(&c->vin))) { fault(c); continue; }
        }
        lock(c);
        if(!c->closing) c->state=paused ? LV_AIC_CAPTURE_PAUSED : LV_AIC_CAPTURE_RUNNING;
        unlock(c);
        unsigned held=0;
        for(unsigned i=0;i<VIN_MAX_BUF_NUM;i++) held+=(c->vin.held>>i)&1U;
        /* Leave a buffer with VIN and service releases without blocking on an
         * empty queue while UI still owns all other frames. */
        if(paused || held+1>=c->vin.buffers.num_buffers) { aicos_msleep(5); continue; }
        uint32_t index;
        if(!lv_aic_vin_acquire(&c->vin,&index)) {
            if(c->vin.faulted) fault(c);
            aicos_msleep(5); continue;
        }
        frame_slot_t *s=&c->slots[index];
        if(!lv_aic_vin_frame_view(&c->vin,index,c->space,&s->frame) || !invalidate_frame(&s->frame)) {
            lock(c); s->state=RETURN; unlock(c); fault(c); continue;
        }
        lock(c);
        /* Latest unpublished frame wins; published readers retain their slots. */
        for(unsigned i=0;i<VIN_MAX_BUF_NUM;i++) if(c->slots[i].state==READY) c->slots[i].state=RETURN;
        s->state=c->closing || c->input.state==LV_AIC_INPUT_PENDING ? RETURN : READY;
        unlock(c);
    }
}
lv_aic_camera_capture_t *lv_aic_camera_capture_prepare(const char *camera,uint32_t channel,
    lv_aic_yuv_format_t format,lv_aic_yuv_color_space_t space)
{
    if(active || !camera || !camera[0] || strlen(camera)>=16 || channel>=VIN_MAX_CHANNELS ||
       space>LV_AIC_YUV_BT709_FULL || space<LV_AIC_YUV_BT601_LIMITED) return NULL;
    enum mpp_pixel_format mpp;
    if(format==LV_COLOR_FORMAT_NV12) mpp=MPP_FMT_NV12;
    else if(format==LV_AIC_YUV_NV16) mpp=MPP_FMT_NV16;
    else if(format==LV_COLOR_FORMAT_I400) mpp=MPP_FMT_YUV400;
    else return NULL;
    lv_aic_camera_capture_t *c=lv_malloc_zeroed(sizeof(*c));
    if(!c) return NULL;
    c->mutex=aicos_mutex_create();
    if(!c->mutex) { lv_free(c); return NULL; }
    memcpy(c->camera,camera,strlen(camera)+1); c->channel=channel; c->format=mpp; c->space=space;
    c->state=LV_AIC_CAPTURE_OPENING;
    c->input=lv_aic_camera_capture_get_input(NULL);
    for(unsigned i=0;i<VIN_MAX_BUF_NUM;i++) c->slots[i].owner=c;
    active=c;
    if(!aicos_thread_create("aic_capture",8192,20,worker,c)) {
        active=NULL; aicos_mutex_delete(c->mutex); lv_free(c); return NULL;
    }
    return c;
}
bool lv_aic_camera_capture_select_input(lv_aic_camera_capture_t *c,uint32_t input)
{
    if(!c || input>3) return false;
    lock(c);
    bool accepted=!c->closing && !c->finished && c->input.state!=LV_AIC_INPUT_PENDING;
    if(accepted) {
        c->input.sequence++; c->input.requested=input; c->input.state=LV_AIC_INPUT_PENDING;
        for(unsigned i=0;i<VIN_MAX_BUF_NUM;i++)
            if(c->slots[i].state==READY) c->slots[i].state=RETURN;
    }
    unlock(c); return accepted;
}
lv_aic_camera_input_status_t lv_aic_camera_capture_get_input(lv_aic_camera_capture_t *c)
{
    if(!c) return (lv_aic_camera_input_status_t){.state=LV_AIC_INPUT_NONE,
        .requested=UINT32_MAX,.applied=UINT32_MAX};
    lock(c); lv_aic_camera_input_status_t status=c->input; unlock(c); return status;
}
bool lv_aic_camera_capture_start(lv_aic_camera_capture_t *c)
{
    if(!c) return false;
    lock(c);
    bool accepted=!c->closing && !c->finished;
    if(accepted) c->start_requested=true;
    unlock(c); return accepted;
}
lv_aic_camera_capture_t *lv_aic_camera_capture_open(const char *camera,uint32_t channel,
    lv_aic_yuv_format_t format,lv_aic_yuv_color_space_t space)
{
    lv_aic_camera_capture_t *c=lv_aic_camera_capture_prepare(camera,channel,format,space);
    if(c) (void)lv_aic_camera_capture_start(c);
    return c;
}
lv_aic_yuv_image_t *lv_aic_camera_capture_poll(lv_aic_camera_capture_t *c)
{
    if(!c) return NULL;
    frame_slot_t *s=NULL;
    lock(c);
    if(!c->closing && c->input.state!=LV_AIC_INPUT_PENDING) for(unsigned i=0;i<VIN_MAX_BUF_NUM;i++) if(c->slots[i].state==READY) {
        s=&c->slots[i]; s->state=PUBLISHING; break;
    }
    unlock(c);
    if(!s) return NULL;
    lv_aic_yuv_image_t *image=lv_aic_yuv_image_create(&s->frame,retain_slot,release_slot,s);
    if(!image) { lock(c); s->state=RETURN; unlock(c); }
    return image;
}
void lv_aic_camera_capture_pause(lv_aic_camera_capture_t *c,bool paused)
{ if(c) { lock(c); c->pause_requested=paused; unlock(c); } }
lv_aic_capture_state_t lv_aic_camera_capture_state(lv_aic_camera_capture_t *c)
{
    if(!c) return LV_AIC_CAPTURE_CLOSED;
    lock(c); lv_aic_capture_state_t state=c->state; unlock(c); return state;
}
void lv_aic_camera_capture_close(lv_aic_camera_capture_t *c)
{
    if(c) { lock(c); c->closing=true;
        if(c->input.state==LV_AIC_INPUT_PENDING && !c->input_inflight)
            c->input.state=LV_AIC_INPUT_CANCELLED;
        if(!c->finished && c->state!=LV_AIC_CAPTURE_FAULT) c->state=LV_AIC_CAPTURE_CLOSING;
        unlock(c); }
}
bool lv_aic_camera_capture_destroy(lv_aic_camera_capture_t *c)
{
    if(!c) return true;
    lock(c); bool finished=c->finished; unlock(c);
    if(!finished) return false;
    aicos_mutex_delete(c->mutex); active=NULL; lv_free(c); return true;
}
#endif
