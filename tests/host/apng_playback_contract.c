/* SPDX-License-Identifier: Apache-2.0 */
#include "lv_aic_apng_playback.h"
#include "lv_aic_apng_stream.h"
#include "lv_aic_rgb_image_private.h"
#include <aic_osal.h>
#include <pthread.h>
#include <stdatomic.h>
#include <assert.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
typedef struct {
    pthread_t thread;
    aic_thread_entry_t entry;
    void *argument;
    bool used,independent;
    unsigned tag;
    atomic_int tokens,fail_close;
} worker_slot_t;
static worker_slot_t workers[LV_AIC_APNG_PLAYBACK_INSTANCES];
static _Thread_local worker_slot_t *current;
static bool multiple;
extern unsigned test_media_runtime_refs(void);
static worker_slot_t *slot_for(lv_aic_apng_playback_t *p)
{
    for(unsigned i=0;i<LV_AIC_APNG_PLAYBACK_INSTANCES;i++)
        if(workers[i].used && workers[i].argument==p) return &workers[i];
    assert(!"missing worker");return NULL;
}
static atomic_int codec_busy;
static atomic_int tokens,close_fail,decode_fail,opened,closed,ticks,thread_fail;
static const char *path="apng-playback-fixture.tmp";
static void worker_only(void) { assert(current); }
aicos_mutex_t aicos_mutex_create(void)
{ pthread_mutex_t *m=malloc(sizeof(*m));assert(m && !pthread_mutex_init(m,NULL));return m; }
void aicos_mutex_delete(aicos_mutex_t m) { assert(!pthread_mutex_destroy(m));free(m); }
int aicos_mutex_take(aicos_mutex_t m,uint32_t timeout) { (void)timeout;return pthread_mutex_lock(m); }
int aicos_mutex_give(aicos_mutex_t m) { return pthread_mutex_unlock(m); }
void aicos_msleep(uint32_t ms) { struct timespec t={ms/1000,(long)(ms%1000)*1000000};nanosleep(&t,NULL); }
uint64_t aic_get_time_us(void) { return 1000; }
static void *run(void *argument)
{ current=argument;current->entry(current->argument);return NULL; }
aicos_thread_t aicos_thread_create(const char *name,uint32_t stack,uint32_t priority,aic_thread_entry_t cb,void *arg)
{
    assert(!strcmp(name,"aic_apng") && stack>=8192 && priority==20);
    if(atomic_load(&thread_fail)) return NULL;
    for(unsigned i=0;i<LV_AIC_APNG_PLAYBACK_INSTANCES;i++) if(!workers[i].used) {
        worker_slot_t *s=&workers[i];s->used=true;s->entry=cb;s->argument=arg;
        s->independent=multiple;s->tag=multiple?32+i*16:0;atomic_store(&s->tokens,0);atomic_store(&s->fail_close,0);
        assert(!pthread_create(&s->thread,NULL,run,s));return &s->thread;
    }
    assert(!"worker limit");return NULL;
}
struct lv_aic_apng_stream { bool paused;unsigned index;uint64_t seq;uint8_t pixels[8]; };
lv_aic_apng_stream_t *lv_aic_apng_stream_open(const void *data,size_t bytes,const lv_aic_apng_stream_config_t *c)
{
    worker_only();assert(bytes==4 && !memcmp(data,"APNG",4) && c->packet_limit==256 && c->now_us(NULL)==1000);
    atomic_fetch_add(&opened,1);return calloc(1,sizeof(lv_aic_apng_stream_t));
}
bool lv_aic_apng_stream_pause(lv_aic_apng_stream_t *s,bool pause) { worker_only();s->paused=pause;return true; }
bool lv_aic_apng_stream_rate(lv_aic_apng_stream_t *s,uint32_t n,uint32_t d)
{ worker_only();assert(s && n && d);return true; }
bool lv_aic_apng_stream_restart(lv_aic_apng_stream_t *s) { worker_only();s->index=0;return true; }
bool lv_aic_apng_stream_close(lv_aic_apng_stream_t *s)
{
    worker_only();assert(atomic_fetch_add(&codec_busy,1)==0);
    bool ok=!atomic_load(&close_fail) && !atomic_load(&current->fail_close);
    if(ok && s) { free(s);atomic_fetch_add(&closed,1); }
    atomic_fetch_sub(&codec_busy,1);return ok;
}
bool lv_aic_apng_stream_tick(lv_aic_apng_stream_t *s,lv_aic_apng_stream_view_t *v)
{
    worker_only();assert(atomic_fetch_add(&codec_busy,1)==0);
    if(current->independent) aicos_msleep(1); /* Force overlap without runtime gate. */
    atomic_fetch_add(&ticks,1);
    if(atomic_load(&decode_fail)) { atomic_fetch_sub(&codec_busy,1);return false; }
    lv_aic_apng_action_t action=LV_AIC_APNG_WAIT;
    if(s->paused) action=LV_AIC_APNG_PAUSED;
    else if(s->index==3) action=LV_AIC_APNG_ENDED;
    else if(atomic_load(current->independent?&current->tokens:&tokens)>0) {
        atomic_fetch_sub(current->independent?&current->tokens:&tokens,1);
        action=LV_AIC_APNG_FRAME;s->index++;s->seq++;
        memset(s->pixels,(int)(current->tag+s->seq),sizeof(s->pixels));s->pixels[3]=s->pixels[7]=255;
    }
    *v=(lv_aic_apng_stream_view_t){.step={.action=action,.frame_index=s->index?s->index-1:0,
        .completed_plays=s->index==3?1:0,.wait_us=1000},.width=2,.height=1,.stride=8,.bytes=8,
        .rgba=s->seq?s->pixels:NULL,.sequence=s->seq};
    atomic_fetch_sub(&codec_busy,1);return true;
}
static void state(lv_aic_apng_playback_t *p,lv_aic_apng_playback_state_t target)
{
    for(unsigned i=0;i<2000;i++) { if(lv_aic_apng_playback_status(p).state==target) return;aicos_msleep(1); }
    assert(!"state timeout");
}
static lv_aic_rgb_image_t *take(lv_aic_apng_playback_t *p,uint64_t expected)
{
    for(unsigned i=0;i<2000;i++) {
        lv_aic_rgb_image_t *image=NULL;uint64_t seq=0;
        if(lv_aic_apng_playback_poll(p,&image,&seq)) { assert(seq==expected);return image; }
        aicos_msleep(1);
    }
    assert(!"image timeout");return NULL;
}
static void finish(lv_aic_apng_playback_t *p)
{
    lv_aic_apng_playback_close(p);
    for(unsigned i=0;i<2000 && !lv_aic_apng_playback_status(p).finished;i++) aicos_msleep(1);
    assert(lv_aic_apng_playback_status(p).finished);
    worker_slot_t *s=slot_for(p);assert(!pthread_join(s->thread,NULL));s->used=false;
}
int main(void)
{
    FILE *file=fopen(path,"wb");assert(file && fwrite("APNG",1,4,file)==4 && !fclose(file));
    lv_init();assert(lv_aic_rgb_image_decoder_init());
    lv_aic_apng_playback_options_t o={.limits={32,256,16,8},.stream_budget=4096,
        .snapshot_budget=4096,.cma_budget=128,.packet_limit=256,.snapshots=2,.minimum_delay_us=1000};
    atomic_store(&thread_fail,1);assert(!lv_aic_apng_playback_prepare(path,&o));
    assert(!test_media_runtime_refs());atomic_store(&thread_fail,0);
    lv_aic_apng_playback_t *p=lv_aic_apng_playback_prepare(path,&o);assert(p);
    state(p,LV_AIC_APNG_READY);assert(test_media_runtime_refs()==1);
    assert(lv_aic_apng_playback_status(p).file_bytes==4);
    assert(!lv_aic_apng_playback_destroy(p));assert(!lv_aic_apng_playback_rate(p,11,1));
    assert(lv_aic_apng_playback_rate(p,2,1));assert(lv_aic_apng_playback_pause(p,true));
    assert(lv_aic_apng_playback_start(p));state(p,LV_AIC_APNG_PLAYBACK_PAUSED);
    assert(!lv_aic_apng_playback_status(p).composed);assert(lv_aic_apng_playback_pause(p,false));
    atomic_store(&tokens,1);lv_aic_rgb_image_t *a=take(p,1);
    atomic_store(&tokens,1);lv_aic_rgb_image_t *b=take(p,2);
    atomic_store(&tokens,1);state(p,LV_AIC_APNG_TERMINAL);
    assert(lv_aic_apng_playback_status(p).composed==3 && lv_aic_apng_playback_status(p).published==2);
    lv_aic_rgb_image_destroy(a);a=take(p,3); /* Final frame retry after backpressure. */
    assert(lv_aic_apng_playback_pause(p,true));assert(lv_aic_apng_playback_restart(p));
    state(p,LV_AIC_APNG_PLAYBACK_PAUSED);
    assert(lv_aic_apng_playback_status(p).restarts==1 && lv_aic_apng_playback_status(p).rate_num==2);
    assert(lv_aic_apng_playback_pause(p,false));lv_aic_rgb_image_destroy(b);
    atomic_store(&tokens,1);b=take(p,4);
    const lv_aic_rgb_frame_t *frame;
    lv_aic_rgb_image_t *reader=lv_aic_rgb_image_acquire(lv_aic_rgb_image_source(b),&frame);assert(reader);
    assert(((const uint32_t *)frame->data)[0]==0xff040404);
    atomic_store(&close_fail,1);lv_aic_apng_playback_close(p);aicos_msleep(20);
    assert(!lv_aic_apng_playback_status(p).finished && !lv_aic_apng_playback_destroy(p));
    assert(!lv_aic_apng_playback_start(p));atomic_store(&close_fail,0);finish(p);
    lv_aic_rgb_image_destroy(a);lv_aic_rgb_image_destroy(b);
    assert(!lv_aic_apng_playback_destroy(p));lv_aic_rgb_image_release_lease(reader);
    assert(lv_aic_apng_playback_destroy(p));assert(atomic_load(&opened)==atomic_load(&closed));
    p=lv_aic_apng_playback_prepare(path,&o);assert(p);state(p,LV_AIC_APNG_READY);
    assert(lv_aic_apng_playback_preserve(p,true));
    assert(lv_aic_apng_playback_start(p));atomic_store(&tokens,3);
    for(unsigned i=0;i<2000 && lv_aic_apng_playback_status(p).published<1;i++) aicos_msleep(1);
    aicos_msleep(20);
    assert(lv_aic_apng_playback_status(p).composed==1 && lv_aic_apng_playback_status(p).published==1);
    a=take(p,1);
    for(unsigned i=0;i<2000 && lv_aic_apng_playback_status(p).published<2;i++) aicos_msleep(1);
    aicos_msleep(20);assert(lv_aic_apng_playback_status(p).composed==2);
    b=take(p,2); /* Both immutable snapshots remain held. */
    for(unsigned i=0;i<2000 && lv_aic_apng_playback_status(p).composed<3;i++) aicos_msleep(1);
    int before_ticks=atomic_load(&ticks);aicos_msleep(20);
    assert(atomic_load(&ticks)==before_ticks && lv_aic_apng_playback_status(p).published==2);
    assert(lv_aic_apng_playback_pause(p,true));state(p,LV_AIC_APNG_PLAYBACK_PAUSED);
    assert(lv_aic_apng_playback_rate(p,1,2));
    lv_aic_rgb_image_destroy(a);a=take(p,3); /* Retry intact final canvas. */
    assert(lv_aic_apng_playback_restart(p));
    for(unsigned i=0;i<2000 && lv_aic_apng_playback_status(p).restart_pending;i++) aicos_msleep(1);
    assert(lv_aic_apng_playback_status(p).restarts==1);
    assert(lv_aic_apng_playback_status(p).rate_den==2);
    lv_aic_rgb_image_destroy(a);lv_aic_rgb_image_destroy(b);finish(p);
    assert(lv_aic_apng_playback_destroy(p));
    /* Replay also discards an unconsumed READY frame without waiting for poll. */
    p=lv_aic_apng_playback_prepare(path,&o);assert(p);state(p,LV_AIC_APNG_READY);
    assert(lv_aic_apng_playback_preserve(p,true));assert(lv_aic_apng_playback_start(p));
    atomic_store(&tokens,1);
    for(unsigned i=0;i<2000 && !lv_aic_apng_playback_status(p).published;i++) aicos_msleep(1);
    assert(lv_aic_apng_playback_pause(p,true));state(p,LV_AIC_APNG_PLAYBACK_PAUSED);
    assert(lv_aic_apng_playback_restart(p));
    for(unsigned i=0;i<2000 && lv_aic_apng_playback_status(p).restart_pending;i++) aicos_msleep(1);
    a=NULL;uint64_t none=99;assert(!lv_aic_apng_playback_poll(p,&a,&none) && !a && none==99);
    finish(p);assert(lv_aic_apng_playback_destroy(p));
    p=lv_aic_apng_playback_prepare(path,&o);assert(p);state(p,LV_AIC_APNG_READY);
    atomic_store(&decode_fail,1);state(p,LV_AIC_APNG_FAULT);finish(p);
    assert(lv_aic_apng_playback_status(p).state==LV_AIC_APNG_FAULT && lv_aic_apng_playback_destroy(p));
    atomic_store(&decode_fail,0);
    /* Real workers, control mailboxes and snapshot pools coexist. One shared
     * SDK runtime reference stays alive until the last reader-owning instance. */
    multiple=true;
    lv_aic_apng_playback_t *players[LV_AIC_APNG_PLAYBACK_INSTANCES];
    lv_aic_rgb_image_t *images[LV_AIC_APNG_PLAYBACK_INSTANCES];
    for(unsigned i=0;i<LV_AIC_APNG_PLAYBACK_INSTANCES;i++) {
        players[i]=lv_aic_apng_playback_prepare(path,&o);assert(players[i]);
        assert(lv_aic_apng_playback_preserve(players[i],true));state(players[i],LV_AIC_APNG_READY);
    }
    assert(!lv_aic_apng_playback_prepare(path,&o) && test_media_runtime_refs()==1);
    assert(lv_aic_apng_playback_pause(players[0],true));
    for(unsigned i=0;i<LV_AIC_APNG_PLAYBACK_INSTANCES;i++) {
        assert(lv_aic_apng_playback_start(players[i]));atomic_store(&slot_for(players[i])->tokens,2);
    }
    state(players[0],LV_AIC_APNG_PLAYBACK_PAUSED);assert(!lv_aic_apng_playback_status(players[0]).composed);
    for(unsigned i=1;i<LV_AIC_APNG_PLAYBACK_INSTANCES;i++) images[i]=take(players[i],1);
    assert(lv_aic_apng_playback_pause(players[0],false));images[0]=take(players[0],1);
    for(unsigned i=0;i<LV_AIC_APNG_PLAYBACK_INSTANCES;i++) {
        const lv_aic_rgb_frame_t *v;
        lv_aic_rgb_image_t *r=lv_aic_rgb_image_acquire(lv_aic_rgb_image_source(images[i]),&v);assert(r);
        assert(v->data[0]==slot_for(players[i])->tag+1);lv_aic_rgb_image_release_lease(r);
    }
    assert(lv_aic_apng_playback_rate(players[1],3,2));
    assert(lv_aic_apng_playback_pause(players[1],true));state(players[1],LV_AIC_APNG_PLAYBACK_PAUSED);
    assert(lv_aic_apng_playback_restart(players[1]));
    for(unsigned i=0;i<2000 && lv_aic_apng_playback_status(players[1]).restart_pending;i++) aicos_msleep(1);
    assert(lv_aic_apng_playback_status(players[1]).restarts==1);
    assert(!lv_aic_apng_playback_status(players[0]).restarts && lv_aic_apng_playback_status(players[0]).rate_num==1);
    reader=lv_aic_rgb_image_acquire(lv_aic_rgb_image_source(images[0]),&frame);assert(reader);
    atomic_store(&slot_for(players[0])->fail_close,1);lv_aic_apng_playback_close(players[0]);aicos_msleep(20);
    assert(!lv_aic_apng_playback_status(players[0]).finished);
    a=take(players[2],2);lv_aic_rgb_image_destroy(a); /* Another worker progresses during cleanup retry. */
    atomic_store(&slot_for(players[0])->fail_close,0);
    finish(players[0]);lv_aic_rgb_image_destroy(images[0]);
    assert(!lv_aic_apng_playback_destroy(players[0]));
    assert(!lv_aic_apng_playback_prepare(path,&o)); /* Closing reader keeps reservation. */
    for(unsigned i=1;i<LV_AIC_APNG_PLAYBACK_INSTANCES;i++) {
        finish(players[i]);lv_aic_rgb_image_destroy(images[i]);assert(lv_aic_apng_playback_destroy(players[i]));
    }
    assert(test_media_runtime_refs()==1);
    /* Another decoder can run while the first instance only retains pixels. */
    p=lv_aic_apng_playback_prepare(path,&o);assert(p);state(p,LV_AIC_APNG_READY);
    assert(lv_aic_apng_playback_start(p));atomic_store(&slot_for(p)->tokens,1);
    a=take(p,1);lv_aic_rgb_image_destroy(a);finish(p);assert(lv_aic_apng_playback_destroy(p));
    assert(test_media_runtime_refs()==1 && frame->data[0]==33);
    lv_aic_rgb_image_release_lease(reader);assert(lv_aic_apng_playback_destroy(players[0]));
    assert(!test_media_runtime_refs());multiple=false;
    atomic_store(&decode_fail,0);o.limits.file_bytes=3;
    p=lv_aic_apng_playback_prepare(path,&o);assert(p);state(p,LV_AIC_APNG_FAULT);finish(p);
    assert(lv_aic_apng_playback_destroy(p));assert(!remove(path));o.limits.file_bytes=32;
    p=lv_aic_apng_playback_prepare(path,&o);assert(p);state(p,LV_AIC_APNG_FAULT);finish(p);
    assert(lv_aic_apng_playback_destroy(p));assert(lv_aic_rgb_image_decoder_deinit());lv_deinit();return 0;
}
