/* SPDX-License-Identifier: Apache-2.0 */
#include "lvgl_aic_feature_config.h"
#if defined(AIC_LVGL_USE_SPI_SDK) && AIC_LVGL_USE_SPI_SDK
#include "lv_aic_spi_worker.h"
#include "lvgl.h"
#include <aic_osal.h>
struct lv_aic_spi_worker {
    lv_aic_spi_handoff_t *handoff;
    aicos_sem_t wake;
    unsigned stopping,exited;
};
static void worker_entry(void *context)
{
    lv_aic_spi_worker_t *w=context;
    for(;;) {
        /* Bounded wait also guarantees progress if a wake notification fails. */
        aicos_sem_take(w->wake,10);
        lv_aic_spi_handoff_run(w->handoff);
        if(__atomic_load_n(&w->stopping,__ATOMIC_ACQUIRE)) {
            /* stop may arrive just after run saw an empty slot. Admission was
             * closed before publishing stopping; consume that last queued job. */
            lv_aic_spi_handoff_run(w->handoff);
            __atomic_store_n(&w->exited,1,__ATOMIC_RELEASE);
            return; /* No context/semaphore access after the release above. */
        }
    }
}
lv_aic_spi_worker_t *lv_aic_spi_worker_create(lv_aic_spi_session_t *session,
    uint32_t stack_bytes,uint32_t priority)
{
    if(!session || stack_bytes<1024 || priority>255) return NULL;
#ifdef RT_THREAD_PRIORITY_MAX
    if(priority>=RT_THREAD_PRIORITY_MAX) return NULL;
#endif
    lv_aic_spi_worker_t *w=lv_malloc_zeroed(sizeof(*w));
    if(!w) return NULL;
    w->handoff=lv_aic_spi_handoff_create(session);
    if(!w->handoff) { lv_free(w);return NULL; }
    w->wake=aicos_sem_create(0);
    if(!w->wake || !aicos_thread_create("aic_spi",stack_bytes,priority,worker_entry,w)) {
        if(w->wake) aicos_sem_delete(w->wake);
        lv_aic_spi_handoff_stop(w->handoff);lv_aic_spi_handoff_close(w->handoff);
        lv_free(w);return NULL;
    }
    return w;
}
lv_aic_spi_result_t lv_aic_spi_worker_submit(lv_aic_spi_worker_t *w,
    const lv_aic_spi_rgb565_frame_t *frame,unsigned degrees,void *cookie)
{
    if(!w) return LV_AIC_SPI_INVALID;
    lv_aic_spi_result_t result=lv_aic_spi_handoff_put(w->handoff,frame,degrees,cookie);
    if(result==LV_AIC_SPI_OK) aicos_sem_give(w->wake);
    return result;
}
bool lv_aic_spi_worker_take(lv_aic_spi_worker_t *w,lv_aic_spi_result_t *result,void **cookie)
{ return w && lv_aic_spi_handoff_take(w->handoff,result,cookie); }
bool lv_aic_spi_worker_take_timed(lv_aic_spi_worker_t *w,lv_aic_spi_result_t *result,
    void **cookie,lv_aic_spi_timing_t *timing)
{ return w && lv_aic_spi_handoff_take_timed(w->handoff,result,cookie,timing); }
void lv_aic_spi_worker_stop(lv_aic_spi_worker_t *w)
{
    if(!w) return;
    lv_aic_spi_handoff_stop(w->handoff);
    __atomic_store_n(&w->stopping,1,__ATOMIC_RELEASE);aicos_sem_give(w->wake);
}
bool lv_aic_spi_worker_close(lv_aic_spi_worker_t *w)
{
    if(!w || !__atomic_load_n(&w->exited,__ATOMIC_ACQUIRE)) return false;
    if(!lv_aic_spi_handoff_close(w->handoff)) return false;
    aicos_sem_delete(w->wake);lv_free(w);return true;
}
#endif
