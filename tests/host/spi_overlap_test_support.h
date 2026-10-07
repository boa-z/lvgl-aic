/* SPDX-License-Identifier: Apache-2.0 */
#include "lv_aic_spi_pipeline.h"
#include <string.h>
#include <stdio.h>
#include "lvgl.h"
#include <aic_osal.h>
#include <pthread.h>
#include <assert.h>
#include <stdlib.h>
#include <time.h>
static pthread_t thread;
static void (*entry_fn)(void *);
static void *entry_context;
static bool fail_sem,fail_thread,fail_wake;
static unsigned allocated,freed,worker_allowed;
static void pause_ms(void) { struct timespec t={0,1000000};nanosleep(&t,NULL); }
static uint64_t now_ms(void)
{ struct timespec t;assert(!clock_gettime(CLOCK_MONOTONIC,&t));return (uint64_t)t.tv_sec*1000+t.tv_nsec/1000000; }
aicos_sem_t aicos_sem_create(uint32_t count)
{ if(fail_sem) return NULL;unsigned *s=malloc(sizeof(*s));assert(s);*s=count;allocated++;return s; }
void aicos_sem_delete(aicos_sem_t s) { free(s);freed++; }
int aicos_sem_give(aicos_sem_t s)
{ if(fail_wake) return -1;__atomic_add_fetch((unsigned *)s,1,__ATOMIC_RELEASE);return 0; }
int aicos_sem_take(aicos_sem_t s,uint32_t ms)
{
    uint64_t start=now_ms();
    do {
        unsigned n=__atomic_load_n((unsigned *)s,__ATOMIC_ACQUIRE);
        if(n && __atomic_compare_exchange_n((unsigned *)s,&n,n-1,false,__ATOMIC_ACQUIRE,__ATOMIC_RELAXED)) return 0;
        pause_ms();
    } while(now_ms()-start<ms);
    return -1;
}
static void *entry(void *unused) { (void)unused;while(!__atomic_load_n(&worker_allowed,__ATOMIC_ACQUIRE)) pause_ms();entry_fn(entry_context);return NULL; }
aicos_thread_t aicos_thread_create(const char *name,uint32_t stack,uint32_t priority,
    void (*fn)(void *),void *context)
{
    assert(name && stack==4096 && priority==20);
    if(fail_thread) return NULL;
    entry_fn=fn;entry_context=context;assert(!pthread_create(&thread,NULL,entry,NULL));return &thread;
}

static lv_aic_spi_transfer_t *transfer;
static uint8_t front[12],back[12],saved[12];
static const uint8_t *active;
static unsigned starts,waits,concurrent,transforms;
static unsigned allow_wait,wait_entered;
static unsigned fail_start_at,fail_wait_at;
static bool force_busy,render_pixels;
static bool start_dma(void *context,const uint8_t *pixels,size_t bytes)
{
    assert(context==front && !active && bytes==12);
    active=pixels;memcpy(saved,pixels,12);starts++;
    return starts!=fail_start_at;
}
static bool wait_dma(void *context)
{
    assert(context==front && active && !memcmp(active,saved,12));waits++;
    __atomic_store_n(&wait_entered,1,__ATOMIC_RELEASE);
    while(!__atomic_load_n(&allow_wait,__ATOMIC_ACQUIRE)) pause_ms();
    assert(!memcmp(active,saved,12));
    if(waits==fail_wait_at) return false;
    active=NULL;return true;
}
static lv_aic_spi_result_t convert(void *context,const lv_aic_spi_rgb565_frame_t *frame,
    uint8_t *out,size_t capacity,uint32_t width,uint32_t height,unsigned degrees,bool swap)
{
    assert(context==front);transforms++;
    if(active) { concurrent++;assert(out!=active && !memcmp(active,saved,12)); }
    if(!render_pixels && frame->data[0]==0) return LV_AIC_SPI_INVALID;
    if(!render_pixels && frame->data[0]==254) return LV_AIC_SPI_FAULT;
    return lv_aic_spi_pack_rgb565(frame,out,capacity,width,height,degrees,swap)?
        LV_AIC_SPI_OK:LV_AIC_SPI_INVALID;
}
lv_aic_spi_result_t lv_aic_spi_session_submit_ex(lv_aic_spi_session_t *session,
    const lv_aic_spi_rgb565_frame_t *frame,unsigned degrees,bool *receipt)
{
    assert(session==(void *)transfer);
    if(force_busy) { *receipt=false;return LV_AIC_SPI_BUSY; }
    return lv_aic_spi_transfer_submit_ex(transfer,frame,degrees,receipt);
}
lv_aic_spi_result_t lv_aic_spi_session_drain(lv_aic_spi_session_t *session)
{ assert(session==(void *)transfer);return lv_aic_spi_transfer_drain(transfer); }
static void setup_transfer(void)
{
    active=NULL;starts=waits=concurrent=transforms=0;
    worker_allowed=allow_wait=wait_entered=0;
    fail_start_at=fail_wait_at=0;force_busy=false;render_pixels=false;
    lv_aic_spi_transfer_ops_t ops={start_dma,wait_dma,front};
    transfer=lv_aic_spi_transfer_create(front,12,3,2,false,&ops);assert(transfer);
    assert(lv_aic_spi_transfer_set_back_buffer(transfer,back,12));
    assert(lv_aic_spi_transfer_set_transform(transfer,convert,front));

}
