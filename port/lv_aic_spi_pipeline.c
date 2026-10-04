/* SPDX-License-Identifier: Apache-2.0 */
#include "lvgl_aic_feature_config.h"
#if defined(AIC_LVGL_USE_SPI_SDK) && AIC_LVGL_USE_SPI_SDK
#include "lv_aic_spi_pipeline.h"
#include "lvgl.h"
#include <aic_osal.h>
enum { EMPTY, QUEUED, RUNNING, SUBMITTED, DONE };
typedef struct {
    unsigned state;
    bool released; /* Producer only. */
    lv_aic_spi_rgb565_frame_t frame;
    unsigned degrees;
    void *cookie;
    lv_aic_spi_result_t submitted,completed;
    lv_aic_spi_timing_t timing;
} slot_t;
struct lv_aic_spi_pipeline {
    lv_aic_spi_session_t *session;
    aicos_sem_t wake;
    slot_t slots[2];
    unsigned put,release,take; /* Producer only. */
    unsigned run;             /* Worker only. */
    int active;              /* Worker only; outstanding DMA slot or -1. */
    unsigned stopping,exited,fault;
};
static void complete(lv_aic_spi_pipeline_t *p,unsigned index,lv_aic_spi_result_t result)
{
    slot_t *s=&p->slots[index];s->completed=result;
    __atomic_store_n(&s->state,DONE,__ATOMIC_RELEASE);
}
static bool run_next(lv_aic_spi_pipeline_t *p)
{
    slot_t *s=&p->slots[p->run];
    if(__atomic_load_n(&s->state,__ATOMIC_ACQUIRE)!=QUEUED) return false;
    unsigned index=p->run;p->run^=1;
    __atomic_store_n(&s->state,RUNNING,__ATOMIC_RELAXED);
    bool previous_completed=false;
    uint32_t start=lv_tick_get();
    s->submitted=__atomic_load_n(&p->fault,__ATOMIC_RELAXED) ? LV_AIC_SPI_FAULT :
        lv_aic_spi_session_submit_ex(p->session,&s->frame,s->degrees,&previous_completed);
    s->timing.submit_ms=lv_tick_elaps(start);s->timing.drain_ms=0;
    if(p->active>=0 && previous_completed) {
        unsigned previous=(unsigned)p->active;p->active=-1;
        complete(p,previous,LV_AIC_SPI_OK);
    }
    if(s->submitted==LV_AIC_SPI_FAULT || s->submitted==LV_AIC_SPI_BUSY ||
       (s->submitted==LV_AIC_SPI_OK && p->active>=0)) {
        /* BUSY or a missing receipt after OK violates exclusive ownership.
         * Quarantine rather than attributing an unverified completion. */
        s->submitted=LV_AIC_SPI_FAULT;
        __atomic_store_n(&p->fault,1,__ATOMIC_RELEASE);
        if(p->active>=0) {
            unsigned previous=(unsigned)p->active;p->active=-1;
            complete(p,previous,LV_AIC_SPI_FAULT);
        }
    }
    if(s->submitted==LV_AIC_SPI_OK) {
        p->active=(int)index;
        __atomic_store_n(&s->state,SUBMITTED,__ATOMIC_RELEASE);
    } else complete(p,index,s->submitted);
    return true;
}
static void drain(lv_aic_spi_pipeline_t *p)
{
    if(p->active<0) return;
    unsigned index=(unsigned)p->active;p->active=-1;
    uint32_t start=lv_tick_get();
    lv_aic_spi_result_t result=lv_aic_spi_session_drain(p->session);
    p->slots[index].timing.drain_ms=lv_tick_elaps(start);
    if(result!=LV_AIC_SPI_OK) {
        result=LV_AIC_SPI_FAULT;
        __atomic_store_n(&p->fault,1,__ATOMIC_RELEASE);
    }
    complete(p,index,result);
}
static void worker_entry(void *context)
{
    lv_aic_spi_pipeline_t *p=context;
    for(;;) {
        /* Stale wake tokens are harmless: only an idle timeout triggers drain.
         * This allows next conversion while DMA owns the previous tx frame. */
        int woke=aicos_sem_take(p->wake,10);
        bool ran=false;
        for(unsigned i=0;i<2 && run_next(p);i++) ran=true;
        if(__atomic_load_n(&p->stopping,__ATOMIC_ACQUIRE)) {
            /* Admission closed before stopping publication; pick up the last
             * jobs even if the earlier acquire observed no queued work. */
            for(unsigned i=0;i<2 && run_next(p);i++) {}
            drain(p);
            __atomic_store_n(&p->exited,1,__ATOMIC_RELEASE);
            return; /* No resource access after exit publication. */
        }
        if(!ran && woke!=0) drain(p);
    }
}
lv_aic_spi_pipeline_t *lv_aic_spi_pipeline_create(lv_aic_spi_session_t *session,
    uint32_t stack_bytes,uint32_t priority)
{
    if(!session || stack_bytes<1024 || priority>255) return NULL;
#ifdef RT_THREAD_PRIORITY_MAX
    if(priority>=RT_THREAD_PRIORITY_MAX) return NULL;
#endif
    lv_aic_spi_pipeline_t *p=lv_malloc_zeroed(sizeof(*p));
    if(!p) return NULL;
    p->session=session;p->active=-1;p->wake=aicos_sem_create(0);
    if(!p->wake || !aicos_thread_create("aic_spi_pipe",stack_bytes,priority,worker_entry,p)) {
        if(p->wake) aicos_sem_delete(p->wake);
        lv_free(p);return NULL;
    }
    return p;
}
lv_aic_spi_result_t lv_aic_spi_pipeline_submit(lv_aic_spi_pipeline_t *p,
    const lv_aic_spi_rgb565_frame_t *frame,unsigned degrees,void *cookie)
{
    if(!p || !frame || __atomic_load_n(&p->stopping,__ATOMIC_RELAXED)) return LV_AIC_SPI_INVALID;
    if(__atomic_load_n(&p->fault,__ATOMIC_ACQUIRE)) return LV_AIC_SPI_FAULT;
    slot_t *s=&p->slots[p->put];
    if(__atomic_load_n(&s->state,__ATOMIC_ACQUIRE)!=EMPTY) return LV_AIC_SPI_BUSY;
    s->frame=*frame;s->degrees=degrees;s->cookie=cookie;s->released=false;
    p->put^=1;
    __atomic_store_n(&s->state,QUEUED,__ATOMIC_RELEASE);
    aicos_sem_give(p->wake);return LV_AIC_SPI_OK;
}
bool lv_aic_spi_pipeline_take(lv_aic_spi_pipeline_t *p,lv_aic_spi_event_t *event)
{
    if(!p || !event) return false;
    slot_t *s=&p->slots[p->release];
    unsigned state=__atomic_load_n(&s->state,__ATOMIC_ACQUIRE);
    if(state>=SUBMITTED && !s->released) {
        event->kind=LV_AIC_SPI_SOURCE_RELEASED;event->cookie=s->cookie;
        event->result=s->submitted;event->timing.submit_ms=s->timing.submit_ms;
        event->timing.drain_ms=0; /* May still be written by the worker. */
        s->released=true;p->release^=1;return true;
    }
    s=&p->slots[p->take];
    if(__atomic_load_n(&s->state,__ATOMIC_ACQUIRE)!=DONE || !s->released) return false;
    event->kind=LV_AIC_SPI_COMPLETED;event->cookie=s->cookie;
    event->result=s->completed;event->timing=s->timing;
    p->take^=1;
    __atomic_store_n(&s->state,EMPTY,__ATOMIC_RELEASE);return true;
}
void lv_aic_spi_pipeline_stop(lv_aic_spi_pipeline_t *p)
{
    if(!p) return;
    __atomic_store_n(&p->stopping,1,__ATOMIC_RELEASE);aicos_sem_give(p->wake);
}
bool lv_aic_spi_pipeline_close(lv_aic_spi_pipeline_t *p)
{
    if(!p || !__atomic_load_n(&p->exited,__ATOMIC_ACQUIRE)) return false;
    for(unsigned i=0;i<2;i++)
        if(__atomic_load_n(&p->slots[i].state,__ATOMIC_ACQUIRE)!=EMPTY) return false;
    aicos_sem_delete(p->wake);lv_free(p);return true;
}
#endif
