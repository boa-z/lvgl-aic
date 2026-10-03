/* SPDX-License-Identifier: Apache-2.0 */
#include "lv_aic_camera_capture.h"
#include "lv_aic_vin_session.h"
#include "lv_aic_yuv_image_private.h"
#include <aic_osal.h>
#include <assert.h>
#include <pthread.h>
#include <stdatomic.h>
#include <stdlib.h>
#include <time.h>

static pthread_t thread;
static pthread_mutex_t gate=PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t wake=PTHREAD_COND_INITIALIZER;
static unsigned tokens, timeout_requested, queued;
static aic_thread_entry_t entry;
static void *entry_arg;
static atomic_int done, blocked, caches, returned, freed, off_calls, fail_off, fail_return, on_calls;
static int fail_thread, fail_init;
static void worker_only(void) { assert(pthread_equal(pthread_self(),thread)); }
aicos_mutex_t aicos_mutex_create(void)
{ pthread_mutex_t *m=malloc(sizeof(*m)); assert(m); assert(!pthread_mutex_init(m,NULL)); return m; }
void aicos_mutex_delete(aicos_mutex_t m) { assert(!pthread_mutex_destroy(m)); free(m); }
int aicos_mutex_take(aicos_mutex_t m,uint32_t timeout) { (void)timeout; return pthread_mutex_lock(m); }
int aicos_mutex_give(aicos_mutex_t m) { return pthread_mutex_unlock(m); }
void aicos_msleep(uint32_t ms)
{ struct timespec delay={ms/1000,(long)(ms%1000)*1000000}; nanosleep(&delay,NULL); }
static void *run(void *arg)
{ (void)arg; entry(entry_arg); atomic_store(&done,1); return NULL; }
aicos_thread_t aicos_thread_create(const char *name,uint32_t stack,uint32_t priority,aic_thread_entry_t cb,void *arg)
{
    (void)name; (void)stack; (void)priority;
    if(fail_thread) return NULL;
    entry=cb; entry_arg=arg; atomic_store(&done,0);
    assert(!pthread_create(&thread,NULL,run,NULL)); return &thread;
}
void aicos_dcache_invalid_range(unsigned long *addr,unsigned long size)
{ worker_only(); assert((uintptr_t)addr>=0x42000000 && !((uintptr_t)addr&31) && size); atomic_fetch_add(&caches,1); }
int mpp_vin2_init(struct vin_dev_ctx *c)
{ worker_only(); if(fail_init) return -1; c->state=VIN_STATE_READY; return 0; }
void mpp_vin2_deinit(struct vin_dev_ctx *c)
{ worker_only(); assert(queued==7 || queued==0); c->state=VIN_STATE_INIT; atomic_fetch_add(&freed,1); }
int mpp_vin2_vb_init(u32 ch,struct vin_dev_ctx *c) { worker_only(); (void)ch; (void)c; return 0; }
void mpp_vin2_vb_deinit(u32 ch,struct vin_dev_ctx *c) { worker_only(); (void)ch; (void)c; }
int mpp_vin2_ioctl(int cmd,void *arg,u32 ch,struct vin_dev_ctx *c)
{
    worker_only(); assert(ch==0); (void)c;
    switch(cmd) {
    case VIN_IN_G_FMT: {
        struct mpp_video_fmt *f=arg; f->width=32; f->height=16; break;
    }
    case VIN_OUT_S_FMT: {
        struct vin_video_fmt *f=arg;
        f->plane_fmt[0].bytesperline=f->plane_fmt[1].bytesperline=32;
        f->plane_fmt[0].sizeimage=f->plane_fmt[1].sizeimage=512;
        break;
    }
    case VIN_REQ_BUF: {
        struct vin_video_buf *b=arg; b->num_planes=2; b->num_buffers=3;
        for(unsigned i=0;i<6;i++) { b->planes[i].buf=0x42000000+i*512; b->planes[i].len=512; }
        queued=0; break;
    }
    case VIN_Q_BUF: {
        unsigned i=(uintptr_t)arg; assert(i<3 && !(queued&(1U<<i)));
        if(atomic_exchange(&fail_return,0)) return -1;
        queued|=1U<<i; atomic_fetch_add(&returned,1); break;
    }
    case VIN_DQ_BUF: {
        pthread_mutex_lock(&gate);
        while(!tokens && !timeout_requested) {
            atomic_store(&blocked,1); pthread_cond_wait(&wake,&gate);
        }
        atomic_store(&blocked,0);
        if(timeout_requested) { timeout_requested=0; pthread_mutex_unlock(&gate); return -1; }
        tokens--; pthread_mutex_unlock(&gate);
        unsigned i=0; while(i<3 && !(queued&(1U<<i))) i++;
        assert(i<3); queued&=~(1U<<i); *(uint32_t *)arg=i; break;
    }
    case VIN_STREAM_ON: atomic_fetch_add(&on_calls,1); break;
    case VIN_STREAM_OFF:
        atomic_fetch_add(&off_calls,1); if(atomic_load(&fail_off)) return -1; break;
    default: break;
    }
    return 0;
}
static void wait_count(atomic_int *value,int minimum)
{
    for(unsigned i=0;i<3000;i++) { if(atomic_load(value)>=minimum) return; aicos_msleep(1); }
    assert(!"worker timed out");
}
static void wake_worker(unsigned frames,bool timeout)
{
    pthread_mutex_lock(&gate); tokens+=frames; timeout_requested=timeout;
    pthread_cond_signal(&wake); pthread_mutex_unlock(&gate);
}
static void wait_state(lv_aic_camera_capture_t *capture,lv_aic_capture_state_t state)
{
    for(unsigned i=0;i<3000;i++) {
        if(lv_aic_camera_capture_state(capture)==state) return;
        aicos_msleep(1);
    }
    assert(!"capture state timed out");
}
int main(void)
{
    lv_init(); assert(lv_aic_yuv_image_decoder_init());
    fail_thread=1;
    assert(!lv_aic_camera_capture_open("camera",0,LV_AIC_YUV_NV16,LV_AIC_YUV_BT601_LIMITED));
    fail_thread=0; fail_init=1;
    lv_aic_camera_capture_t *capture=lv_aic_camera_capture_open("camera",0,LV_AIC_YUV_NV16,LV_AIC_YUV_BT601_LIMITED);
    assert(capture); wait_count(&done,1); pthread_join(thread,NULL);
    assert(lv_aic_camera_capture_state(capture)==LV_AIC_CAPTURE_FAULT);
    assert(lv_aic_camera_capture_destroy(capture)); fail_init=0;
    /* Preparing must not queue, stream or dequeue. Close before start is safe. */
    capture=lv_aic_camera_capture_prepare("camera",0,LV_AIC_YUV_NV16,LV_AIC_YUV_BT601_LIMITED);
    assert(capture); wait_state(capture,LV_AIC_CAPTURE_READY);
    assert(!atomic_load(&on_calls) && !atomic_load(&returned) && !atomic_load(&blocked));
    assert(!lv_aic_camera_capture_poll(capture));
    lv_aic_camera_capture_close(capture);
    assert(!lv_aic_camera_capture_start(capture));
    wait_count(&done,1); pthread_join(thread,NULL);
    assert(lv_aic_camera_capture_destroy(capture)); atomic_store(&freed,0);
    /* Early/idempotent start and pause requests are serviced by worker. */
    capture=lv_aic_camera_capture_prepare("camera",0,LV_AIC_YUV_NV16,LV_AIC_YUV_BT601_LIMITED);
    assert(capture); lv_aic_camera_capture_pause(capture,true);
    assert(lv_aic_camera_capture_start(capture));
    assert(lv_aic_camera_capture_start(capture));
    wait_state(capture,LV_AIC_CAPTURE_PAUSED);
    assert(atomic_load(&on_calls)==1 && !atomic_load(&blocked));
    lv_aic_camera_capture_close(capture);
    wait_count(&done,1); pthread_join(thread,NULL);
    assert(lv_aic_camera_capture_destroy(capture));
    atomic_store(&freed,0); atomic_store(&returned,0); atomic_store(&off_calls,0);
    capture=lv_aic_camera_capture_open("camera",0,LV_AIC_YUV_NV16,LV_AIC_YUV_BT601_LIMITED);
    assert(capture);
    assert(!lv_aic_camera_capture_open("camera",0,LV_AIC_YUV_NV16,LV_AIC_YUV_BT601_LIMITED));
    wait_count(&blocked,1);
    lv_aic_camera_capture_pause(capture,true); wake_worker(0,true);
    wait_state(capture,LV_AIC_CAPTURE_PAUSED);
    assert(!lv_aic_camera_capture_poll(capture));
    lv_aic_camera_capture_pause(capture,false);
    wait_state(capture,LV_AIC_CAPTURE_RUNNING); wait_count(&blocked,1);
    /* Worker replaces the first unpublished frame and queues it back itself. */
    wake_worker(2,false); wait_count(&caches,4); wait_count(&returned,4); wait_count(&blocked,1);
    lv_aic_yuv_image_t *image=lv_aic_camera_capture_poll(capture); assert(image);
    const lv_aic_yuv_frame_t *view;
    lv_aic_yuv_image_t *reader=lv_aic_yuv_image_acquire(lv_aic_yuv_image_source(image),&view);
    assert(reader && view->format==LV_AIC_YUV_NV16 && (uintptr_t)view->planes[0].data==0x42000400);
    lv_aic_yuv_image_destroy(image);
    lv_aic_camera_capture_close(capture);
    assert(!lv_aic_camera_capture_destroy(capture) && !atomic_load(&freed));
    /* Close returns while DQ is still blocked. It must not kill the thread,
     * call VIN from UI, or free the decoder reader's capture storage. */
    wake_worker(0,true);
    aicos_msleep(10);
    assert(!lv_aic_camera_capture_destroy(capture) && !atomic_load(&freed));
    atomic_store(&fail_return,1); atomic_store(&fail_off,1);
    lv_aic_yuv_image_release_lease(reader);
    wait_count(&off_calls,1);
    assert(!lv_aic_camera_capture_destroy(capture) && !atomic_load(&freed));
    atomic_store(&fail_off,0); wait_count(&done,1); pthread_join(thread,NULL);
    assert(atomic_load(&freed)==1);
    assert(lv_aic_camera_capture_state(capture)==LV_AIC_CAPTURE_FAULT);
    assert(lv_aic_camera_capture_destroy(capture));
    /* A failed publication must still return the dequeued frame on worker. */
    assert(lv_aic_yuv_image_decoder_deinit());
    int before=atomic_load(&caches);
    capture=lv_aic_camera_capture_open("camera",0,LV_AIC_YUV_NV16,LV_AIC_YUV_BT601_LIMITED);
    assert(capture); wait_count(&blocked,1); wake_worker(1,false);
    wait_count(&caches,before+2); wait_count(&blocked,1);
    assert(!lv_aic_camera_capture_poll(capture));
    lv_aic_camera_capture_close(capture); wake_worker(0,true);
    wait_count(&done,1); pthread_join(thread,NULL);
    assert(lv_aic_camera_capture_state(capture)==LV_AIC_CAPTURE_CLOSED);
    assert(lv_aic_camera_capture_destroy(capture));
    assert(lv_aic_yuv_image_decoder_init());
    assert(lv_aic_yuv_image_decoder_deinit()); lv_deinit(); return 0;
}
