/* SPDX-License-Identifier: Apache-2.0 */
#include "lv_aic_player_playback.h"
#include "lv_aic_rgb_image_private.h"
#include <aic_player.h>
#include <frame_allocator.h>
#include <aic_osal.h>
#include <assert.h>
#include <pthread.h>
#include <stdatomic.h>
#include <stdlib.h>
#include <time.h>
static pthread_t worker_thread;
static pthread_mutex_t gate=PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t wake=PTHREAD_COND_INITIALIZER;
static aic_thread_entry_t entry;
static void *entry_arg;
static unsigned tokens;
static bool timeout_requested;
static atomic_int done,blocked,gets,puts,starts,pauses,resumes,stops,freed,allocated,fail_put,next_flags;
static int fail_thread,fail_start,fail_callback;
static bool audio_only,unseekable;
static atomic_int seeks,fail_seek;
static atomic_ullong seek_target;
static event_handler notify;
static void *notify_context;
static struct frame_allocator *sdk_allocator;
static struct mpp_frame pool[4];
static unsigned pool_count,held,next_frame;
static uintptr_t next_address=0x42000000;
struct aic_player { bool live; };
static struct aic_player player;
static void worker_only(void) { assert(pthread_equal(pthread_self(),worker_thread)); }
aicos_mutex_t aicos_mutex_create(void)
{ pthread_mutex_t *m=malloc(sizeof(*m)); assert(m && !pthread_mutex_init(m,NULL)); return m; }
void aicos_mutex_delete(aicos_mutex_t m) { assert(!pthread_mutex_destroy(m)); free(m); }
int aicos_mutex_take(aicos_mutex_t m,uint32_t timeout) { (void)timeout; return pthread_mutex_lock(m); }
int aicos_mutex_give(aicos_mutex_t m) { return pthread_mutex_unlock(m); }
void aicos_msleep(uint32_t ms) { struct timespec t={ms/1000,(long)(ms%1000)*1000000}; nanosleep(&t,NULL); }
static void *run(void *arg) { (void)arg; entry(entry_arg); atomic_store(&done,1); return NULL; }
aicos_thread_t aicos_thread_create(const char *name,uint32_t stack,uint32_t priority,aic_thread_entry_t cb,void *arg)
{
    assert(!strcmp(name,"aic_playback") && stack>=8192 && priority==20);
    if(fail_thread) return NULL;
    entry=cb; entry_arg=arg; atomic_store(&done,0);
    assert(!pthread_create(&worker_thread,NULL,run,NULL)); return &worker_thread;
}
uint64_t aic_get_time_us(void) { return 100000; }
void *aicos_malloc_align(unsigned int type,size_t size,size_t align)
{ worker_only(); assert(type==MEM_CMA && size==512 && align==32); atomic_fetch_add(&allocated,1); next_address+=0x1000; return (void *)next_address; }
void aicos_free_align(unsigned int type,void *ptr)
{ worker_only(); assert(type==MEM_CMA && ptr); atomic_fetch_add(&freed,1); }
void aicos_dcache_clean_invalid_range(unsigned long *ptr,unsigned long size)
{ worker_only(); assert(ptr && size==512); }
void aicos_dcache_invalid_range(unsigned long *ptr,unsigned long size)
{ worker_only(); assert(ptr && size==512); }
struct aic_player *aic_player_create(char *uri)
{ worker_only(); assert(!uri && !player.live); player.live=true; return &player; }
s32 aic_player_set_uri(struct aic_player *p,char *uri)
{ worker_only(); assert(p->live && !strcmp(uri,"/test.mp4")); return 0; }
s32 aic_player_prepare_sync(struct aic_player *p) { worker_only(); assert(p->live); return 0; }
s32 aic_player_get_media_info(struct aic_player *p,struct av_media_info *info)
{ worker_only(); assert(p->live); *info=(struct av_media_info){.has_video=!audio_only,.has_audio=audio_only,
  .file_size=6000000000LL,.audio_stream={2,16,48000},.duration=1000000,.seek_able=!unseekable,.video_stream={audio_only?0:8,audio_only?0:16}}; return 0; }
s32 aic_player_control(struct aic_player *p,enum aic_player_command cmd,void *data)
{
    worker_only(); assert(p->live);
    if(cmd==AIC_PLAYER_CMD_SET_VDEC_EXT_FRAME_ALLOCATOR) sdk_allocator=data;
    else { assert(cmd==AIC_PLAYER_CMD_SET_VDEC_EXT_FRAME_NUM && *(s32 *)data==3); }
    return 0;
}
s32 aic_player_set_event_callback(struct aic_player *p,void *ctx,event_handler cb)
{ worker_only(); assert(p->live); notify=cb; notify_context=ctx; return fail_callback?-1:0; }
s32 aic_player_start(struct aic_player *p)
{
    worker_only(); assert(p->live && sdk_allocator); atomic_fetch_add(&starts,1);
    if(fail_start) return -1;
    pool_count=held=next_frame=0;
    if(audio_only) return 0;
    for(unsigned i=0;i<4;i++) {
        pool[i]=(struct mpp_frame){.id=i,.buf={.size={8,16}}};
        if(sdk_allocator->ops->alloc_frame_buffer(sdk_allocator,&pool[i],32,16,MPP_FMT_ARGB_8888)) return -1;
        pool_count++;
    }
    return 0;
}
s32 aic_player_stop(struct aic_player *p)
{
    worker_only(); assert(p->live && !held); atomic_fetch_add(&stops,1);
    if(sdk_allocator) {
        for(unsigned i=0;i<pool_count;i++) assert(!sdk_allocator->ops->free_frame_buffer(sdk_allocator,&pool[i]));
        assert(!sdk_allocator->ops->close_allocator(sdk_allocator));
    }
    pool_count=0; return 0;
}
s32 aic_player_destroy(struct aic_player *p)
{ worker_only(); assert(p->live); p->live=false; sdk_allocator=NULL; notify=NULL; notify_context=NULL; return 0; }
s32 aic_player_get_frame(struct aic_player *p,struct mpp_frame *f)
{
    worker_only(); assert(p->live); atomic_fetch_add(&gets,1);
    pthread_mutex_lock(&gate);
    while(!tokens && !timeout_requested) { atomic_store(&blocked,1); pthread_cond_wait(&wake,&gate); }
    atomic_store(&blocked,0);
    if(timeout_requested) { timeout_requested=false; pthread_mutex_unlock(&gate); return -1; }
    tokens--; pthread_mutex_unlock(&gate);
    unsigned i=next_frame++%4; assert(!(held&(1U<<i))); held|=1U<<i;
    *f=pool[i]; f->pts=(int64_t)next_frame*40000; f->flags=atomic_exchange(&next_flags,0);
    if(f->flags&FRAME_FLAG_EOS) assert(!notify(notify_context,AIC_PLAYER_EVENT_PLAY_END,0,0));
    return 0;
}
s32 aic_player_put_frame(struct aic_player *p,struct mpp_frame *f)
{
    worker_only(); assert(p->live && (held&(1U<<f->id))); atomic_fetch_add(&puts,1);
    if(atomic_load(&fail_put)) return -1;
    held&=~(1U<<f->id); return 0;
}
s32 aic_player_pause(struct aic_player *p) { worker_only(); assert(p->live); atomic_fetch_add(&pauses,1); return 0; }
s32 aic_player_play(struct aic_player *p) { worker_only(); assert(p->live); atomic_fetch_add(&resumes,1); return 0; }
s32 aic_player_seek(struct aic_player *p,u64 pts)
{
    worker_only(); assert(p->live && !held && !pool_count);
    atomic_store(&seek_target,pts); atomic_fetch_add(&seeks,1);
    return atomic_load(&fail_seek)?-1:0;
}
s32 aic_player_set_volum(struct aic_player *p,s32 volume) { worker_only(); assert(p->live && volume==42); return 0; }
s32 aic_player_get_volum(struct aic_player *p,s32 *volume) { (void)p; (void)volume; assert(!"unexpected getter"); return -1; }
s64 aic_player_get_play_time(struct aic_player *p) { (void)p; assert(!"unsynchronized SDK time getter"); return -1; }
static void wake_worker(unsigned frames,bool timeout)
{
    pthread_mutex_lock(&gate); tokens+=frames; timeout_requested=timeout;
    pthread_cond_broadcast(&wake); pthread_mutex_unlock(&gate);
}
static void wait_count(atomic_int *counter,int minimum)
{
    for(unsigned i=0;i<3000;i++) { if(atomic_load(counter)>=minimum) return; aicos_msleep(1); }
    assert(!"worker timeout");
}
static void wait_state(lv_aic_player_playback_t *p,lv_aic_playback_state_t state)
{
    for(unsigned i=0;i<3000;i++) { if(lv_aic_player_playback_status(p).state==state) return; aicos_msleep(1); }
    assert(!"state timeout");
}
static void finish(lv_aic_player_playback_t *p)
{
    wait_count(&done,1); pthread_join(worker_thread,NULL);
    assert(lv_aic_player_playback_status(p).finished);
    assert(lv_aic_player_playback_destroy(p));
    assert(atomic_load(&allocated)==atomic_load(&freed));
}
static lv_aic_playback_options_t options={4096,3,LV_AIC_YUV_BT601_LIMITED};
static lv_aic_player_playback_t *prepare(void)
{
    lv_aic_player_playback_t *p=lv_aic_player_playback_prepare("/test.mp4",&options); assert(p); return p;
}
int main(void)
{
    lv_init(); assert(lv_aic_rgb_image_decoder_init()); assert(lv_aic_yuv_image_decoder_init());
    assert(!lv_aic_player_playback_prepare(NULL,&options));
    fail_thread=1; assert(!lv_aic_player_playback_prepare("/test.mp4",&options)); fail_thread=0;
    lv_aic_player_playback_t *p=prepare(); wait_state(p,LV_AIC_PLAYBACK_READY);
    assert(!atomic_load(&starts) && !atomic_load(&gets));
    lv_aic_player_playback_close(p); finish(p);
    p=prepare(); assert(lv_aic_player_playback_pause(p,true)); assert(lv_aic_player_playback_volume(p,42));
    assert(lv_aic_player_playback_start(p)); wait_state(p,LV_AIC_PLAYBACK_PAUSED);
    assert(lv_aic_player_playback_status(p).volume==42 && !atomic_load(&gets));
    int paused=atomic_load(&pauses);
    assert(lv_aic_player_playback_pause(p,true)); aicos_msleep(15); assert(atomic_load(&pauses)==paused);
    assert(lv_aic_player_playback_pause(p,false)); wait_count(&blocked,1);
    wake_worker(1,false);
    lv_aic_player_image_t image={0};
    for(unsigned i=0;i<3000 && !lv_aic_player_playback_poll(p,&image);i++) aicos_msleep(1);
    assert(image.rgb && image.pts==40000);
    const lv_aic_rgb_frame_t *view;
    lv_aic_rgb_image_t *reader=lv_aic_rgb_image_acquire(lv_aic_player_image_source(&image),&view); assert(reader);
    wait_count(&blocked,1); lv_aic_player_playback_close(p);
    assert(!lv_aic_player_playback_destroy(p));
    assert(!lv_aic_player_playback_start(p) && !lv_aic_player_playback_pause(p,false));
    wake_worker(0,true); aicos_msleep(15);
    assert(!lv_aic_player_playback_status(p).finished);
    lv_aic_player_image_destroy(&image); aicos_msleep(10);
    assert(!lv_aic_player_playback_status(p).finished);
    atomic_store(&fail_put,1); lv_aic_rgb_image_release_lease(reader);
    wait_state(p,LV_AIC_PLAYBACK_FAULT); assert(!lv_aic_player_playback_status(p).finished);
    atomic_store(&fail_put,0); finish(p);
    p=prepare(); assert(lv_aic_player_playback_start(p)); wait_count(&blocked,1);
    assert(lv_aic_player_playback_pause(p,true)); /* Pause races the final blocked get. */
    atomic_store(&next_flags,FRAME_FLAG_EOS); wake_worker(1,false);
    wait_state(p,LV_AIC_PLAYBACK_TERMINAL);
    assert(lv_aic_player_playback_status(p).video_eos && lv_aic_player_playback_status(p).sdk_terminal);
    assert(lv_aic_player_playback_poll(p,&image) && image.rgb);
    assert(!lv_aic_player_playback_start(p)); lv_aic_player_playback_close(p);
    assert(!lv_aic_player_playback_destroy(p)); lv_aic_player_image_destroy(&image); finish(p);
    p=prepare(); assert(lv_aic_player_playback_start(p)); wait_count(&blocked,1);
    assert(!notify(notify_context,AIC_PLAYER_EVENT_PLAY_END,0,0)); wake_worker(0,true);
    wait_state(p,LV_AIC_PLAYBACK_TERMINAL); assert(!lv_aic_player_playback_status(p).video_eos);
    lv_aic_player_playback_close(p); finish(p);
    p=prepare(); assert(lv_aic_player_playback_start(p)); wait_count(&blocked,1);
    atomic_store(&next_flags,FRAME_FLAG_ERROR); wake_worker(1,false); wait_state(p,LV_AIC_PLAYBACK_FAULT); finish(p);
    fail_callback=1; p=prepare(); wait_state(p,LV_AIC_PLAYBACK_FAULT); finish(p); fail_callback=0;
    fail_start=1; p=prepare(); assert(lv_aic_player_playback_start(p)); wait_state(p,LV_AIC_PLAYBACK_FAULT); finish(p); fail_start=0;
    options.cma_budget=256; p=prepare(); assert(lv_aic_player_playback_start(p)); wait_state(p,LV_AIC_PLAYBACK_FAULT); finish(p); options.cma_budget=4096;
    /* Seek closes the old epoch only after native readers and blocked get exit. */
    p=prepare(); assert(lv_aic_player_playback_start(p)); wait_count(&blocked,1);
    wake_worker(1,false);
    for(unsigned i=0;i<3000 && !lv_aic_player_playback_poll(p,&image);i++) aicos_msleep(1);
    assert(image.rgb); reader=lv_aic_rgb_image_acquire(lv_aic_player_image_source(&image),&view); assert(reader);
    wait_count(&blocked,1);
    assert(!lv_aic_player_playback_seek(p,1000001));
    assert(lv_aic_player_playback_seek(p,500000));
    assert(!lv_aic_player_playback_seek(p,400000));
    assert(!lv_aic_player_playback_poll(p,&(lv_aic_player_image_t){0}));
    assert(lv_aic_player_playback_pause(p,true));
    wake_worker(0,true); lv_aic_player_image_destroy(&image); aicos_msleep(15);
    assert(!atomic_load(&seeks) && lv_aic_player_playback_status(p).seek_pending);
    /* Old terminal and audio notifications must die with the old mailbox. */
    assert(!notify(notify_context,AIC_PLAYER_EVENT_PLAY_END,0,0));
    lv_aic_rgb_image_release_lease(reader); wait_count(&seeks,1); wait_state(p,LV_AIC_PLAYBACK_PAUSED);
    assert(atomic_load(&seek_target)==500000 && lv_aic_player_playback_status(p).seeks_completed==1);
    assert(!lv_aic_player_playback_status(p).sdk_terminal && !lv_aic_player_playback_status(p).position_valid);
    /* A second paused seek doesn't wait for first-frame completion in old SDK. */
    assert(lv_aic_player_playback_seek(p,0)); wait_count(&seeks,2); wait_state(p,LV_AIC_PLAYBACK_PAUSED);
    assert(lv_aic_player_playback_pause(p,false)); wait_count(&blocked,1);
    atomic_store(&next_flags,FRAME_FLAG_EOS); wake_worker(1,false); wait_state(p,LV_AIC_PLAYBACK_TERMINAL);
    assert(lv_aic_player_playback_seek(p,200000)); wait_count(&seeks,3); wait_count(&blocked,1);
    lv_aic_player_playback_close(p); wake_worker(0,true); finish(p);
    /* Prepared seek preserves the no-auto-start contract. */
    p=prepare(); wait_state(p,LV_AIC_PLAYBACK_READY);
    lv_aic_playback_status_t metadata=lv_aic_player_playback_status(p);
    assert(metadata.media_info_valid && metadata.media_info.file_size==6000000000LL &&
           metadata.media_info.audio_stream.sample_rate==48000 && metadata.media_info.video_stream.width==8);
    int before_starts=atomic_load(&starts);
    assert(lv_aic_player_playback_seek(p,123)); wait_count(&seeks,4); wait_state(p,LV_AIC_PLAYBACK_READY);
    assert(atomic_load(&starts)==before_starts); lv_aic_player_playback_close(p); finish(p);
    atomic_store(&fail_seek,1); p=prepare(); wait_state(p,LV_AIC_PLAYBACK_READY);
    assert(lv_aic_player_playback_seek(p,123)); wait_state(p,LV_AIC_PLAYBACK_FAULT); finish(p); atomic_store(&fail_seek,0);
    /* Close wins while a seek waits for a blocked old get. */
    p=prepare(); assert(lv_aic_player_playback_start(p)); wait_count(&blocked,1);
    int before_seek=atomic_load(&seeks);
    assert(lv_aic_player_playback_seek(p,456)); lv_aic_player_playback_close(p);
    wake_worker(0,true); finish(p); assert(atomic_load(&seeks)==before_seek);
    unseekable=true; p=prepare(); wait_state(p,LV_AIC_PLAYBACK_READY);
    assert(!lv_aic_player_playback_seek(p,0)); lv_aic_player_playback_close(p); finish(p); unseekable=false;
    audio_only=true; p=prepare(); assert(lv_aic_player_playback_start(p)); wait_state(p,LV_AIC_PLAYBACK_PLAYING);
    assert(!notify(notify_context,AIC_PLAYER_EVENT_PLAY_TIME,-1,-5000));
    for(unsigned i=0;i<3000 && !lv_aic_player_playback_status(p).position_valid;i++) aicos_msleep(1);
    assert(lv_aic_player_playback_status(p).position_us==-5000);
    before_seek=atomic_load(&seeks); assert(lv_aic_player_playback_seek(p,700000));
    wait_count(&seeks,before_seek+1); wait_state(p,LV_AIC_PLAYBACK_PLAYING);
    assert(!lv_aic_player_playback_status(p).position_valid);
    assert(!notify(notify_context,AIC_PLAYER_EVENT_PLAY_TIME,0,700000));
    for(unsigned i=0;i<3000 && !lv_aic_player_playback_status(p).position_valid;i++) aicos_msleep(1);
    assert(lv_aic_player_playback_status(p).position_us==700000);
    assert(!notify(notify_context,AIC_PLAYER_EVENT_PLAY_END,0,0)); wait_state(p,LV_AIC_PLAYBACK_TERMINAL);
    lv_aic_player_playback_close(p); finish(p);
    /* Backpressure prevents latest-frame replacement and another blocking
     * get_frame while READY is unconsumed; controls still reach the worker. */
    audio_only=false;p=prepare();wait_state(p,LV_AIC_PLAYBACK_READY);
    assert(lv_aic_player_playback_preserve(p,true));
    assert(lv_aic_player_playback_start(p));wake_worker(3,false);
    for(unsigned i=0;i<3000 && lv_aic_player_playback_status(p).frames_queued<1;i++) aicos_msleep(1);
    aicos_msleep(20);assert(lv_aic_player_playback_status(p).frames_received==1);
    assert(lv_aic_player_playback_pause(p,true));wait_state(p,LV_AIC_PLAYBACK_PAUSED);
    assert(lv_aic_player_playback_volume(p,42));
    assert(lv_aic_player_playback_pause(p,false));wait_state(p,LV_AIC_PLAYBACK_PLAYING);
    for(unsigned n=1;n<=3;n++) {
        lv_aic_player_image_t kept={0};
        for(unsigned i=0;i<3000 && !kept.rgb;i++) {
            (void)lv_aic_player_playback_poll(p,&kept);if(!kept.rgb) aicos_msleep(1);
        }
        assert(kept.rgb && kept.pts==(int64_t)n*40000);
        lv_aic_player_image_destroy(&kept);
        if(n<3) {
            for(unsigned i=0;i<3000 && lv_aic_player_playback_status(p).frames_queued<n+1;i++) aicos_msleep(1);
            aicos_msleep(20);assert(lv_aic_player_playback_status(p).frames_received==n+1);
        }
    }
    lv_aic_player_playback_close(p);wake_worker(0,true);finish(p);
    /* Enabling preservation while the next SDK get is already blocked must
     * retain both the older READY image and the arriving EOS lease. */
    pthread_mutex_lock(&gate);tokens=0;timeout_requested=false;pthread_mutex_unlock(&gate);
    p=prepare();assert(lv_aic_player_playback_start(p));wake_worker(1,false);
    for(unsigned i=0;i<3000 && lv_aic_player_playback_status(p).frames_queued<1;i++) aicos_msleep(1);
    wait_count(&blocked,1);
    assert(lv_aic_player_playback_preserve(p,true));atomic_store(&next_flags,FRAME_FLAG_EOS);
    wake_worker(1,false);
    for(unsigned i=0;i<3000 && lv_aic_player_playback_status(p).frames_received<2;i++) aicos_msleep(1);
    aicos_msleep(20);
    assert(lv_aic_player_playback_status(p).frames_queued==1);
    assert(lv_aic_player_playback_status(p).state!=LV_AIC_PLAYBACK_FAULT);
    for(unsigned n=1;n<=2;n++) {
        lv_aic_player_image_t kept={0};
        for(unsigned i=0;i<3000 && !kept.rgb;i++) {
            (void)lv_aic_player_playback_poll(p,&kept);if(!kept.rgb) aicos_msleep(1);
        }
        assert(kept.rgb && kept.pts==(int64_t)n*40000);lv_aic_player_image_destroy(&kept);
    }
    wait_state(p,LV_AIC_PLAYBACK_TERMINAL);lv_aic_player_playback_close(p);finish(p);
    assert(lv_aic_rgb_image_decoder_deinit() && lv_aic_yuv_image_decoder_deinit()); lv_deinit(); return 0;
}
