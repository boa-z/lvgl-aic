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
struct slot { pthread_t thread; aic_thread_entry_t entry; void *arg; bool used;
    atomic_int tokens,done,starts,end_event,hold_stop,stopping; unsigned id; };
static struct slot slots[4];
static _Thread_local struct slot *current;
static _Thread_local event_handler notify;
static _Thread_local void *notify_context;
static _Thread_local struct frame_allocator *sdk_allocator;
static _Thread_local struct mpp_frame pool[4];
static _Thread_local unsigned pool_count,held,next_frame;
static _Thread_local bool audio_only;
static atomic_int allocated,freed,gets,puts,starts,stops,pauses,resumes,seeks;
static atomic_uintptr_t next_address=0x42000000;
struct aic_player { bool live; };
static _Thread_local struct aic_player player;
static void worker_only(void) { assert(current && pthread_equal(pthread_self(),current->thread)); }
aicos_mutex_t aicos_mutex_create(void)
{ pthread_mutex_t *m=malloc(sizeof(*m)); assert(m && !pthread_mutex_init(m,NULL)); return m; }
void aicos_mutex_delete(aicos_mutex_t m) { assert(!pthread_mutex_destroy(m)); free(m); }
int aicos_mutex_take(aicos_mutex_t m,uint32_t timeout) { (void)timeout; return pthread_mutex_lock(m); }
int aicos_mutex_give(aicos_mutex_t m) { return pthread_mutex_unlock(m); }
void aicos_msleep(uint32_t ms) { struct timespec t={ms/1000,(long)(ms%1000)*1000000}; nanosleep(&t,NULL); }
static void *run(void *arg) { current=arg; current->entry(current->arg); atomic_store(&current->done,1); return NULL; }
aicos_thread_t aicos_thread_create(const char *name,uint32_t stack,uint32_t priority,aic_thread_entry_t cb,void *arg)
{
    assert(!strcmp(name,"aic_playback") && stack>=8192 && priority==20);
    for(unsigned i=0;i<4;i++) if(!slots[i].used) {
        struct slot *s=&slots[i];s->used=true;s->entry=cb;s->arg=arg;s->id=i;
        atomic_store(&s->end_event,0);atomic_store(&s->hold_stop,0);atomic_store(&s->stopping,0);atomic_store(&s->done,0);atomic_store(&s->tokens,0);atomic_store(&s->starts,0);
        assert(!pthread_create(&s->thread,NULL,run,s));return s;
    }
    assert(!"worker limit");return NULL;
}
uint64_t aic_get_time_us(void) { return 100000; }
void *aicos_malloc_align(unsigned int type,size_t size,size_t align)
{ worker_only(); assert(type==MEM_CMA && size==512 && align==32); atomic_fetch_add(&allocated,1); return (void *)(atomic_fetch_add(&next_address,0x1000)+0x1000); }
void aicos_free_align(unsigned int type,void *ptr)
{ worker_only(); assert(type==MEM_CMA && ptr); atomic_fetch_add(&freed,1); }
void aicos_dcache_clean_invalid_range(unsigned long *ptr,unsigned long size)
{ worker_only(); assert(ptr && size==512); }
void aicos_dcache_invalid_range(unsigned long *ptr,unsigned long size)
{ worker_only(); assert(ptr && size==512); }
struct aic_player *aic_player_create(char *uri)
{ worker_only(); assert(!uri && !player.live); player.live=true; return &player; }
s32 aic_player_set_uri(struct aic_player *p,char *uri)
{ worker_only(); assert(p->live);audio_only=!strcmp(uri,"/audio.mp4");return 0; }
s32 aic_player_prepare_sync(struct aic_player *p) { worker_only(); assert(p->live); return 0; }
s32 aic_player_get_media_info(struct aic_player *p,struct av_media_info *info)
{ worker_only(); assert(p->live); *info=(struct av_media_info){.has_video=!audio_only,.has_audio=audio_only,
  .file_size=6000000000LL,.audio_stream={2,16,48000},.duration=1000000,.seek_able=true,.video_stream={audio_only?0:8,audio_only?0:16}}; return 0; }
s32 aic_player_control(struct aic_player *p,enum aic_player_command cmd,void *data)
{
    worker_only(); assert(p->live);
    if(cmd==AIC_PLAYER_CMD_SET_VDEC_EXT_FRAME_ALLOCATOR) sdk_allocator=data;
    else { assert(cmd==AIC_PLAYER_CMD_SET_VDEC_EXT_FRAME_NUM && *(s32 *)data==3); }
    return 0;
}
s32 aic_player_set_event_callback(struct aic_player *p,void *ctx,event_handler cb)
{ worker_only(); assert(p->live); notify=cb; notify_context=ctx; return 0; }
s32 aic_player_start(struct aic_player *p)
{
    worker_only(); assert(p->live && sdk_allocator); atomic_fetch_add(&starts,1);
    atomic_fetch_add(&current->starts,1);
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
    atomic_store(&current->stopping,1);
    while(atomic_load(&current->hold_stop)) aicos_msleep(1);
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
    if(atomic_exchange(&current->end_event,0)) assert(!notify(notify_context,AIC_PLAYER_EVENT_PLAY_END,0,0));
    if(!atomic_load(&current->tokens)) { aicos_msleep(1);return -1; }
    atomic_fetch_sub(&current->tokens,1);
    unsigned i=next_frame++%4; assert(!(held&(1U<<i))); held|=1U<<i;
    *f=pool[i]; f->pts=(int64_t)(current->id+1)*100000+next_frame*1000; f->flags=0;
    if(f->flags&FRAME_FLAG_EOS) assert(!notify(notify_context,AIC_PLAYER_EVENT_PLAY_END,0,0));
    return 0;
}
s32 aic_player_put_frame(struct aic_player *p,struct mpp_frame *f)
{
    worker_only(); assert(p->live && (held&(1U<<f->id))); atomic_fetch_add(&puts,1);
    held&=~(1U<<f->id); return 0;
}
s32 aic_player_pause(struct aic_player *p) { worker_only(); assert(p->live); atomic_fetch_add(&pauses,1); return 0; }
s32 aic_player_play(struct aic_player *p) { worker_only(); assert(p->live); atomic_fetch_add(&resumes,1); return 0; }
s32 aic_player_seek(struct aic_player *p,u64 pts)
{
    worker_only(); assert(p->live && !held && !pool_count);
    (void)pts; atomic_fetch_add(&seeks,1);
    return 0;
}
s32 aic_player_set_volum(struct aic_player *p,s32 volume) { worker_only(); assert(p->live && volume==42); return 0; }
s32 aic_player_get_volum(struct aic_player *p,s32 *volume) { (void)p; (void)volume; assert(!"unexpected getter"); return -1; }
s64 aic_player_get_play_time(struct aic_player *p) { (void)p; assert(!"unsynchronized SDK time getter"); return -1; }
static void wait_state(lv_aic_player_playback_t *p,lv_aic_playback_state_t state)
{
    for(unsigned i=0;i<3000;i++) { if(lv_aic_player_playback_status(p).state==state) return;aicos_msleep(1); }
    assert(!"state timeout");
}
static struct slot *slot_for(lv_aic_player_playback_t *p)
{ for(unsigned i=0;i<4;i++) if(slots[i].used && slots[i].arg==p) return &slots[i];assert(0);return NULL; }
static void finish(lv_aic_player_playback_t *p)
{
    struct slot *s=slot_for(p);
    for(unsigned i=0;i<3000 && !atomic_load(&s->done);i++) aicos_msleep(1);
    assert(atomic_load(&s->done));assert(!pthread_join(s->thread,NULL));
    assert(lv_aic_player_playback_destroy(p));s->used=false;
}
static lv_aic_playback_options_t options={4096,3,LV_AIC_YUV_BT601_LIMITED};
static lv_aic_player_playback_t *prepare(const char *uri)
{ lv_aic_player_playback_t *p=lv_aic_player_playback_prepare(uri,&options);assert(p);return p; }
static lv_aic_player_image_t frame(lv_aic_player_playback_t *p,unsigned n)
{
    struct slot *s=slot_for(p);atomic_fetch_add(&s->tokens,1);
    lv_aic_player_image_t image={0};
    for(unsigned i=0;i<3000 && !image.rgb;i++) { lv_aic_player_playback_poll(p,&image);if(!image.rgb) aicos_msleep(1); }
    assert(image.rgb && image.pts==(int64_t)(s->id+1)*100000+n*1000);return image;
}
int main(void)
{
    lv_init();assert(lv_aic_rgb_image_decoder_init() && lv_aic_yuv_image_decoder_init());
    lv_aic_player_playback_t *p[4];lv_aic_player_image_t images[4];
    for(unsigned i=0;i<4;i++) { p[i]=prepare("/test.mp4");wait_state(p[i],LV_AIC_PLAYBACK_READY);assert(lv_aic_player_playback_start(p[i])); }
    assert(!lv_aic_player_playback_prepare("/test.mp4",&options));
    for(unsigned i=0;i<4;i++) images[i]=frame(p[i],1);
    const lv_aic_rgb_frame_t *view;
    lv_aic_rgb_image_t *reader=lv_aic_rgb_image_acquire(lv_aic_player_image_source(&images[0]),&view);assert(reader);
    for(unsigned i=0;i<4;i++) lv_aic_player_image_destroy(&images[i]);
    lv_aic_player_playback_close(p[0]);aicos_msleep(20);
    assert(!lv_aic_player_playback_destroy(p[0]));
    assert(!lv_aic_player_playback_prepare("/test.mp4",&options));
    assert(lv_aic_player_playback_pause(p[1],true));wait_state(p[1],LV_AIC_PLAYBACK_PAUSED);
    assert(lv_aic_player_playback_seek(p[1],100));
    for(unsigned i=0;i<3000 && !lv_aic_player_playback_status(p[1]).seeks_completed;i++) aicos_msleep(1);
    assert(lv_aic_player_playback_status(p[1]).seeks_completed==1);
    images[2]=frame(p[2],2);lv_aic_player_image_destroy(&images[2]);
    assert(!lv_aic_player_playback_status(p[2]).seeks_completed);
    atomic_store(&slot_for(p[3])->end_event,1);wait_state(p[3],LV_AIC_PLAYBACK_TERMINAL);
    assert(!lv_aic_player_playback_status(p[2]).sdk_terminal);
    images[2]=frame(p[2],3);lv_aic_player_image_destroy(&images[2]);
    lv_aic_rgb_image_release_lease(reader);finish(p[0]);
    for(unsigned i=1;i<4;i++) { lv_aic_player_playback_close(p[i]);finish(p[i]); }
    /* A prepared audio source owns the lease across session recreation. */
    p[0]=prepare("/audio.mp4");wait_state(p[0],LV_AIC_PLAYBACK_READY);
    p[1]=prepare("/audio.mp4");wait_state(p[1],LV_AIC_PLAYBACK_FAULT);
    assert(!atomic_load(&slot_for(p[1])->starts));finish(p[1]);
    assert(lv_aic_player_playback_seek(p[0],100));
    for(unsigned i=0;i<3000 && !lv_aic_player_playback_status(p[0]).seeks_completed;i++) aicos_msleep(1);
    assert(lv_aic_player_playback_status(p[0]).seeks_completed==1);
    p[1]=prepare("/audio.mp4");wait_state(p[1],LV_AIC_PLAYBACK_FAULT);finish(p[1]);
    p[2]=prepare("/test.mp4");assert(lv_aic_player_playback_start(p[2]));
    images[2]=frame(p[2],1);lv_aic_player_image_destroy(&images[2]);
    struct slot *owner=slot_for(p[0]);atomic_store(&owner->stopping,0);atomic_store(&owner->hold_stop,1);
    lv_aic_player_playback_close(p[0]);
    for(unsigned i=0;i<3000 && !atomic_load(&owner->stopping);i++) aicos_msleep(1);
    assert(atomic_load(&owner->stopping));
    p[1]=prepare("/audio.mp4");wait_state(p[1],LV_AIC_PLAYBACK_FAULT);finish(p[1]);
    atomic_store(&owner->hold_stop,0);finish(p[0]);
    p[1]=prepare("/audio.mp4");assert(lv_aic_player_playback_start(p[1]));wait_state(p[1],LV_AIC_PLAYBACK_PLAYING);
    lv_aic_player_playback_close(p[1]);finish(p[1]);lv_aic_player_playback_close(p[2]);finish(p[2]);
    assert(atomic_load(&allocated)==atomic_load(&freed));
    assert(lv_aic_rgb_image_decoder_deinit() && lv_aic_yuv_image_decoder_deinit());lv_deinit();return 0;
}
