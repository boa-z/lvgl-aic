/* SPDX-License-Identifier: Apache-2.0 */
#include "lv_aic_player_frames.h"
#include "lv_aic_yuv_image_private.h"
#include "lv_aic_rgb_image_private.h"
#include <aic_osal.h>
#include <assert.h>
#include <pthread.h>
#include <stdlib.h>
#include <string.h>
static pthread_t worker;
static pthread_mutex_t gate=PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t wake=PTHREAD_COND_INITIALIZER;
static int command;
static bool result, finished, cache_hold, cache_wait;
static lv_aic_player_session_t session;
static lv_aic_player_allocator_t *allocator;
static lv_aic_player_frames_t *bridge;
static struct frame_allocator *sdk_allocator;
static struct mpp_frame pool[4];
static unsigned held, next_frame;
static enum mpp_pixel_format media_format=MPP_FMT_YUV400;
static int puts, frees, allocations, invalidates, fail_put;
static uint64_t submitted;
static uintptr_t next_address=0x42000000;
struct aic_player { bool live; };
static struct aic_player player;
static void worker_only(void) { assert(pthread_equal(pthread_self(),worker)); }
aicos_mutex_t aicos_mutex_create(void)
{ pthread_mutex_t *m=malloc(sizeof(*m)); assert(m && !pthread_mutex_init(m,NULL)); return m; }
void aicos_mutex_delete(aicos_mutex_t m) { assert(!pthread_mutex_destroy(m)); free(m); }
int aicos_mutex_take(aicos_mutex_t m,uint32_t timeout) { (void)timeout; return pthread_mutex_lock(m); }
int aicos_mutex_give(aicos_mutex_t m) { return pthread_mutex_unlock(m); }
void *aicos_malloc_align(unsigned int type,size_t size,size_t align)
{ worker_only(); assert(type==MEM_CMA && size==512 && align==32); allocations++; next_address+=0x1000; return (void *)next_address; }
void aicos_free_align(unsigned int type,void *ptr) { worker_only(); assert(type==MEM_CMA && ptr); frees++; }
void aicos_dcache_clean_invalid_range(unsigned long *p,unsigned long size)
{ worker_only(); assert(p && size==512); }
void aicos_dcache_invalid_range(unsigned long *p,unsigned long size)
{
    worker_only(); assert(p && size==512); invalidates++;
    pthread_mutex_lock(&gate);
    while(cache_hold) { cache_wait=true; pthread_cond_broadcast(&wake); pthread_cond_wait(&wake,&gate); }
    cache_wait=false; pthread_mutex_unlock(&gate);
}
struct aic_player *aic_player_create(char *uri)
{ worker_only(); assert(!uri && !player.live); player.live=true; return &player; }
s32 aic_player_set_uri(struct aic_player *p,char *uri) { worker_only(); assert(p->live && uri); return 0; }
s32 aic_player_prepare_sync(struct aic_player *p) { worker_only(); assert(p->live); return 0; }
s32 aic_player_get_media_info(struct aic_player *p,struct av_media_info *info)
{ worker_only(); assert(p->live); *info=(struct av_media_info){.has_video=1,.video_stream={32,16}}; return 0; }
s32 aic_player_control(struct aic_player *p,enum aic_player_command cmd,void *data)
{ worker_only(); assert(p->live); if(cmd==AIC_PLAYER_CMD_SET_VDEC_EXT_FRAME_ALLOCATOR) sdk_allocator=data;
  else { assert(cmd==AIC_PLAYER_CMD_SET_VDEC_EXT_FRAME_NUM && *(s32 *)data==3); }
  return 0; }
s32 aic_player_start(struct aic_player *p)
{
    worker_only(); assert(p->live && sdk_allocator);
    for(unsigned i=0;i<4;i++) {
        pool[i]=(struct mpp_frame){.id=i,.pts=(int64_t)i*40000,.buf={.size={8,16}}};
        assert(!sdk_allocator->ops->alloc_frame_buffer(sdk_allocator,&pool[i],32,16,media_format));
    }
    held=0; next_frame=0; return 0;
}
s32 aic_player_stop(struct aic_player *p)
{
    worker_only(); assert(p->live && !held);
    if(sdk_allocator) {
        for(unsigned i=0;i<4;i++) assert(!sdk_allocator->ops->free_frame_buffer(sdk_allocator,&pool[i]));
        assert(!sdk_allocator->ops->close_allocator(sdk_allocator)); sdk_allocator=NULL;
    }
    return 0;
}
s32 aic_player_destroy(struct aic_player *p) { worker_only(); assert(p->live); p->live=false; return 0; }
s32 aic_player_get_frame(struct aic_player *p,struct mpp_frame *f)
{ worker_only(); assert(p->live); unsigned i=next_frame++%4; if(held&(1U<<i)) return -1;
  held|=1U<<i; *f=pool[i]; return 0; }
s32 aic_player_put_frame(struct aic_player *p,struct mpp_frame *f)
{ worker_only(); assert(p->live && (held&(1U<<f->id))); puts++; if(fail_put) return -1;
  held&=~(1U<<f->id); return 0; }
s32 aic_player_pause(struct aic_player *p) { (void)p; worker_only(); return 0; }
s32 aic_player_play(struct aic_player *p) { (void)p; worker_only(); return 0; }
s32 aic_player_seek(struct aic_player *p,u64 us) { (void)p; (void)us; worker_only(); return 0; }
s32 aic_player_set_volum(struct aic_player *p,s32 v) { (void)p; (void)v; worker_only(); return 0; }
s32 aic_player_get_volum(struct aic_player *p,s32 *v) { (void)p; worker_only(); *v=50; return 0; }
s64 aic_player_get_play_time(struct aic_player *p) { (void)p; worker_only(); return 0; }
enum { OPEN=1,SUBMIT,DRAIN,DUPLICATE,CLOSE,BAD_FRAME,EXIT };
static void *run(void *arg)
{
    (void)arg;
    for(;;) {
        pthread_mutex_lock(&gate);
        while(!command) pthread_cond_wait(&wake,&gate);
        int op=command; pthread_mutex_unlock(&gate);
        if(op==EXIT) return NULL;
        bool ok=false;
        if(op==OPEN) ok=lv_aic_player_session_open(&session,"/test.mp4") &&
            lv_aic_player_session_allocator(&session,lv_aic_player_allocator_sdk(allocator),3) &&
            lv_aic_player_session_start(&session);
        if(op==SUBMIT || op==BAD_FRAME) {
            const struct mpp_frame *f;
            ok=lv_aic_player_session_acquire(&session,&submitted,&f);
            assert(ok);
            if(op==BAD_FRAME) session.frames[f-session.frames].flags|=FRAME_FLAG_ERROR;
            ok=lv_aic_player_frames_submit(bridge,submitted);
            if(!ok) assert(lv_aic_player_session_release(&session,submitted));
        }
        if(op==DUPLICATE) ok=lv_aic_player_frames_submit(bridge,submitted);
        if(op==DRAIN) ok=lv_aic_player_frames_drain(bridge);
        if(op==CLOSE) ok=lv_aic_player_session_close(&session);
        pthread_mutex_lock(&gate); result=ok; command=0; finished=true;
        pthread_cond_broadcast(&wake); pthread_mutex_unlock(&gate);
    }
}
static bool execute(int op)
{
    pthread_mutex_lock(&gate); finished=false; command=op; pthread_cond_broadcast(&wake);
    while(!finished) pthread_cond_wait(&wake,&gate);
    bool ok=result; pthread_mutex_unlock(&gate); return ok;
}
static void shutdown(void)
{
    assert(execute(DRAIN)); assert(lv_aic_player_frames_idle(bridge));
    assert(execute(CLOSE));
    pthread_mutex_lock(&gate); command=EXIT; pthread_cond_broadcast(&wake); pthread_mutex_unlock(&gate);
    pthread_join(worker,NULL); command=0;
    assert(lv_aic_player_frames_destroy(bridge));
    assert(lv_aic_player_allocator_destroy(allocator));
}
static void startup(void)
{
    allocator=lv_aic_player_allocator_create(4096); assert(allocator);
    bridge=lv_aic_player_frames_create(&session,allocator,LV_AIC_YUV_BT601_LIMITED); assert(bridge);
    assert(!pthread_create(&worker,NULL,run,NULL)); assert(execute(OPEN));
}
int main(void)
{
    lv_init(); startup();
    int64_t pts=-1;
    assert(!lv_aic_player_frames_poll(bridge,&pts) && pts==-1);
    assert(execute(SUBMIT)); assert(execute(DUPLICATE));
    /* Missing YUV decoder publication fails, but only worker may return frame. */
    int before=puts;
    assert(!lv_aic_player_frames_poll(bridge,&pts) && puts==before && pts==-1);
    assert(execute(DRAIN) && puts==before+1);
    assert(lv_aic_yuv_image_decoder_init());
    assert(!execute(BAD_FRAME));
    assert(execute(SUBMIT)); assert(execute(SUBMIT)); /* latest unpublished wins */
    before=puts; assert(execute(DRAIN) && puts==before+1);
    lv_aic_yuv_image_t *image=lv_aic_player_frames_poll(bridge,&pts);
    assert(image && pts==120000);
    const lv_aic_yuv_frame_t *view;
    lv_aic_yuv_image_t *reader=lv_aic_yuv_image_acquire(lv_aic_yuv_image_source(image),&view);
    assert(reader && view->width==8 && view->planes[0].capacity==512);
    assert(!lv_aic_player_frames_destroy(bridge));
    lv_aic_player_frames_close(bridge);
    assert(!execute(SUBMIT));
    assert(!lv_aic_player_frames_poll(bridge,&pts));
    before=puts; lv_aic_yuv_image_destroy(image);
    assert(execute(DRAIN) && puts==before && !lv_aic_player_frames_idle(bridge));
    assert(!execute(CLOSE)); /* session still owns the published SDK frame */
    lv_aic_yuv_image_release_lease(reader);
    fail_put=1; assert(!execute(DRAIN) && puts==before+1);
    assert(!lv_aic_player_frames_idle(bridge));
    fail_put=0; assert(execute(DRAIN) && puts==before+2);
    shutdown();
    /* Unpublished frame on close is returned without ever publishing it. */
    startup(); assert(execute(SUBMIT)); lv_aic_player_frames_close(bridge); shutdown();
    /* Close concurrently with worker pin/cache handoff. A successful submit
     * still owns the lease, but must never become publishable after close. */
    startup();
    pthread_mutex_lock(&gate); cache_hold=true; finished=false; command=SUBMIT;
    pthread_cond_broadcast(&wake);
    while(!cache_wait) pthread_cond_wait(&wake,&gate);
    pthread_mutex_unlock(&gate);
    lv_aic_player_frames_close(bridge);
    assert(!lv_aic_player_frames_idle(bridge) && !lv_aic_player_frames_poll(bridge,&pts));
    pthread_mutex_lock(&gate); cache_hold=false; pthread_cond_broadcast(&wake);
    while(!finished) pthread_cond_wait(&wake,&gate);
    assert(result); pthread_mutex_unlock(&gate);
    assert(!lv_aic_player_frames_poll(bridge,&pts)); shutdown();
    assert(lv_aic_rgb_image_decoder_init()); media_format=MPP_FMT_ARGB_8888;
    startup(); assert(execute(SUBMIT));
    assert(!lv_aic_player_frames_poll(bridge,&pts)); /* Legacy poll must not discard RGB. */
    lv_aic_player_image_t published={0};
    assert(lv_aic_player_frames_poll_image(bridge,&published) && published.rgb && !published.yuv);
    const lv_aic_rgb_frame_t *rgb;
    lv_aic_rgb_image_t *rgb_reader=lv_aic_rgb_image_acquire(lv_aic_player_image_source(&published),&rgb);
    assert(rgb_reader && rgb->width==8 && rgb->stride==32 && rgb->capacity==512);
    lv_aic_player_frames_close(bridge); before=puts;
    lv_aic_player_image_destroy(&published);
    assert(!published.rgb && !lv_aic_player_image_source(&published));
    assert(execute(DRAIN) && puts==before && !lv_aic_player_frames_idle(bridge));
    lv_aic_rgb_image_release_lease(rgb_reader); shutdown();
    assert(lv_aic_rgb_image_decoder_deinit());
    assert(allocations==frees && invalidates>0);
    assert(lv_aic_yuv_image_decoder_deinit()); lv_deinit();
    return 0;
}
