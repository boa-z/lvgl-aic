/* SPDX-License-Identifier: Apache-2.0 */
#include "lvgl_aic_feature_config.h"
#if defined(AIC_LVGL_USE_SPI_SDK) && AIC_LVGL_USE_SPI_SDK
#include "lv_aic_spi_handoff.h"
#include "lvgl.h"
enum { EMPTY, QUEUED, RUNNING, DONE };
struct lv_aic_spi_handoff {
    unsigned state;
    bool stopped,fault;
    lv_aic_spi_session_t *session;
    lv_aic_spi_rgb565_frame_t frame;
    unsigned degrees;
    void *cookie;
    lv_aic_spi_result_t result;
};
lv_aic_spi_handoff_t *lv_aic_spi_handoff_create(lv_aic_spi_session_t *session)
{
    if(!session) return NULL;
    lv_aic_spi_handoff_t *h=lv_malloc_zeroed(sizeof(*h));
    if(h) h->session=session;
    return h;
}
lv_aic_spi_result_t lv_aic_spi_handoff_put(lv_aic_spi_handoff_t *h,
    const lv_aic_spi_rgb565_frame_t *frame,unsigned degrees,void *cookie)
{
    if(!h || !frame) return LV_AIC_SPI_INVALID;
    if(h->fault) return LV_AIC_SPI_FAULT;
    if(h->stopped) return LV_AIC_SPI_INVALID;
    if(__atomic_load_n(&h->state,__ATOMIC_ACQUIRE)!=EMPTY) return LV_AIC_SPI_BUSY;
    h->frame=*frame;h->degrees=degrees;h->cookie=cookie;
    __atomic_store_n(&h->state,QUEUED,__ATOMIC_RELEASE);return LV_AIC_SPI_OK;
}
bool lv_aic_spi_handoff_run(lv_aic_spi_handoff_t *h)
{
    if(!h) return false;
    unsigned expected=QUEUED;
    if(!__atomic_compare_exchange_n(&h->state,&expected,RUNNING,false,
                                   __ATOMIC_ACQUIRE,__ATOMIC_RELAXED)) return false;
    h->result=lv_aic_spi_session_submit(h->session,&h->frame,h->degrees);
    if(h->result==LV_AIC_SPI_OK) h->result=lv_aic_spi_session_drain(h->session);
    __atomic_store_n(&h->state,DONE,__ATOMIC_RELEASE);return true;
}
bool lv_aic_spi_handoff_take(lv_aic_spi_handoff_t *h,lv_aic_spi_result_t *result,void **cookie)
{
    if(!h || !result || !cookie || __atomic_load_n(&h->state,__ATOMIC_ACQUIRE)!=DONE) return false;
    *result=h->result;*cookie=h->cookie;
    if(h->result==LV_AIC_SPI_FAULT || h->result==LV_AIC_SPI_BUSY) h->fault=true;
    __atomic_store_n(&h->state,EMPTY,__ATOMIC_RELEASE);return true;
}
void lv_aic_spi_handoff_stop(lv_aic_spi_handoff_t *h) { if(h) h->stopped=true; }
bool lv_aic_spi_handoff_close(lv_aic_spi_handoff_t *h)
{
    if(!h || !h->stopped || __atomic_load_n(&h->state,__ATOMIC_ACQUIRE)!=EMPTY) return false;
    lv_free(h);return true;
}
#endif
