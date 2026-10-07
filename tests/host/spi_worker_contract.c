/* SPDX-License-Identifier: Apache-2.0 */
#include "lv_aic_spi_worker.h"
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
static unsigned allocated,freed,entered,release_dma;
static void pause_ms(void) { struct timespec t={0,1000000};nanosleep(&t,NULL); }
aicos_sem_t aicos_sem_create(uint32_t count)
{ if(fail_sem) return NULL;unsigned *s=malloc(sizeof(*s));assert(s);*s=count;allocated++;return s; }
void aicos_sem_delete(aicos_sem_t s) { free(s);freed++; }
int aicos_sem_give(aicos_sem_t s)
{ if(fail_wake) return -1;__atomic_add_fetch((unsigned *)s,1,__ATOMIC_RELEASE);return 0; }
int aicos_sem_take(aicos_sem_t s,uint32_t ms)
{
    for(unsigned i=0;i<ms;i++) {
        unsigned n=__atomic_load_n((unsigned *)s,__ATOMIC_ACQUIRE);
        if(n && __atomic_compare_exchange_n((unsigned *)s,&n,n-1,false,__ATOMIC_ACQUIRE,__ATOMIC_RELAXED)) return 0;
        pause_ms();
    }
    return -1;
}
static void *entry(void *unused) { (void)unused;entry_fn(entry_context);return NULL; }
aicos_thread_t aicos_thread_create(const char *name,uint32_t stack,uint32_t priority,
    void (*fn)(void *),void *context)
{
    assert(name && stack==4096 && priority==20);
    if(fail_thread) return NULL;
    entry_fn=fn;entry_context=context;assert(!pthread_create(&thread,NULL,entry,NULL));return &thread;
}
lv_aic_spi_result_t lv_aic_spi_session_submit(lv_aic_spi_session_t *session,
    const lv_aic_spi_rgb565_frame_t *frame,unsigned degrees)
{
    assert(session && frame->data[0]==42 && degrees==0);
    __atomic_store_n(&entered,1,__ATOMIC_RELEASE);return LV_AIC_SPI_OK;
}
lv_aic_spi_result_t lv_aic_spi_session_drain(lv_aic_spi_session_t *session)
{
    assert(session);
    while(!__atomic_load_n(&release_dma,__ATOMIC_ACQUIRE)) pause_ms();
    return LV_AIC_SPI_FAULT;
}
int main(void)
{
    lv_init();lv_aic_spi_session_t *session=(void *)(uintptr_t)1;
    assert(!lv_aic_spi_worker_create(session,512,20));
    assert(!lv_aic_spi_worker_create(session,4096,32));
    assert(!lv_aic_spi_worker_create(session,4096,256));
    fail_sem=true;assert(!lv_aic_spi_worker_create(session,4096,20));fail_sem=false;
    fail_thread=true;assert(!lv_aic_spi_worker_create(session,4096,20));fail_thread=false;
    assert(allocated==freed);
    lv_aic_spi_worker_t *w=lv_aic_spi_worker_create(session,4096,20);assert(w);
    uint8_t data[2]={42,0};lv_aic_spi_rgb565_frame_t frame={data,2,2,1,1};
    fail_wake=true; /* Bounded wait must rescue accepted work. */
    assert(lv_aic_spi_worker_submit(w,&frame,0,data)==LV_AIC_SPI_OK);
    while(!__atomic_load_n(&entered,__ATOMIC_ACQUIRE)) pause_ms();
    assert(lv_aic_spi_worker_submit(w,&frame,0,data)==LV_AIC_SPI_BUSY);
    lv_aic_spi_worker_stop(w);assert(!lv_aic_spi_worker_close(w));
    assert(lv_aic_spi_worker_submit(w,&frame,0,data)==LV_AIC_SPI_INVALID);
    lv_aic_spi_result_t result;void *cookie;
    assert(!lv_aic_spi_worker_take(w,&result,&cookie));
    __atomic_store_n(&release_dma,1,__ATOMIC_RELEASE);
    assert(!pthread_join(thread,NULL));
    assert(!lv_aic_spi_worker_close(w)); /* Unconsumed source completion. */
    assert(lv_aic_spi_worker_take(w,&result,&cookie) && result==LV_AIC_SPI_FAULT && cookie==data);
    assert(!lv_aic_spi_worker_take(w,&result,&cookie));
    assert(lv_aic_spi_worker_close(w) && allocated==freed);return 0;
}
