/* SPDX-License-Identifier: Apache-2.0 */
#include "lv_aic_spi_handoff.h"
#include "lvgl.h"
#include <pthread.h>
#include <assert.h>
static unsigned submits,drains;
static lv_aic_spi_result_t submit_result=LV_AIC_SPI_OK,drain_result=LV_AIC_SPI_OK;
static lv_aic_spi_handoff_t *handoff;
static unsigned finish_worker;
lv_aic_spi_result_t lv_aic_spi_session_submit(lv_aic_spi_session_t *s,
    const lv_aic_spi_rgb565_frame_t *frame,unsigned degrees)
{
    assert(s==(void *)(uintptr_t)1 && frame->width==1 && frame->height==1 && degrees==90);
    assert(frame->data[0]==(uint8_t)submits && frame->data[1]==(uint8_t)(submits>>8));
    assert(!lv_aic_spi_handoff_run(handoff)); /* Cannot process a running job twice. */
    lv_tick_inc(3);submits++;return submit_result;
}
lv_aic_spi_result_t lv_aic_spi_session_drain(lv_aic_spi_session_t *s)
{ assert(s);lv_tick_inc(5);drains++;return drain_result; }
static void *worker(void *unused)
{
    (void)unused;
    while(!__atomic_load_n(&finish_worker,__ATOMIC_ACQUIRE)) lv_aic_spi_handoff_run(handoff);
    return NULL;
}
int main(void)
{
    lv_init();assert(!lv_aic_spi_handoff_create(NULL));
    handoff=lv_aic_spi_handoff_create((void *)(uintptr_t)1);assert(handoff);
    uint8_t data[2];lv_aic_spi_rgb565_frame_t frame={data,2,2,1,1};
    lv_aic_spi_result_t result;void *cookie;lv_aic_spi_timing_t timing;
    assert(!lv_aic_spi_handoff_take(handoff,&result,&cookie));
    assert(!lv_aic_spi_handoff_close(handoff));
    pthread_t thread;assert(!pthread_create(&thread,NULL,worker,NULL));
    for(unsigned i=0;i<1000;i++) {
        data[0]=(uint8_t)i;data[1]=(uint8_t)(i>>8);
        assert(lv_aic_spi_handoff_put(handoff,&frame,90,&data)==LV_AIC_SPI_OK);
        assert(lv_aic_spi_handoff_put(handoff,&frame,90,NULL)==LV_AIC_SPI_BUSY);
        while(!lv_aic_spi_handoff_take_timed(handoff,&result,&cookie,&timing)) {}
        assert(timing.submit_ms==3 && timing.drain_ms==5);
        assert(result==LV_AIC_SPI_OK && cookie==&data);
        assert(!lv_aic_spi_handoff_take(handoff,&result,&cookie));
    }
    __atomic_store_n(&finish_worker,1,__ATOMIC_RELEASE);assert(!pthread_join(thread,NULL));
    assert(submits==1000 && drains==1000);
    data[0]=(uint8_t)submits;data[1]=(uint8_t)(submits>>8);
    assert(lv_aic_spi_handoff_put(handoff,&frame,90,data)==LV_AIC_SPI_OK);
    lv_aic_spi_handoff_stop(handoff);
    assert(!lv_aic_spi_handoff_close(handoff));
    assert(lv_aic_spi_handoff_put(handoff,&frame,90,data)==LV_AIC_SPI_INVALID);
    drain_result=LV_AIC_SPI_FAULT;
    assert(lv_aic_spi_handoff_run(handoff));assert(!lv_aic_spi_handoff_close(handoff));
    assert(lv_aic_spi_handoff_take(handoff,&result,&cookie) && result==LV_AIC_SPI_FAULT);
    assert(lv_aic_spi_handoff_put(handoff,&frame,90,data)==LV_AIC_SPI_FAULT);
    assert(lv_aic_spi_handoff_close(handoff));
    handoff=lv_aic_spi_handoff_create((void *)(uintptr_t)1);assert(handoff);
    data[0]=(uint8_t)submits;data[1]=(uint8_t)(submits>>8);
    submit_result=LV_AIC_SPI_INVALID;unsigned before=drains;
    assert(lv_aic_spi_handoff_put(handoff,&frame,90,data)==LV_AIC_SPI_OK);
    assert(lv_aic_spi_handoff_run(handoff));
    assert(lv_aic_spi_handoff_take_timed(handoff,&result,&cookie,&timing) && result==LV_AIC_SPI_INVALID);
    assert(timing.submit_ms==3 && timing.drain_ms==0);
    assert(drains==before);lv_aic_spi_handoff_stop(handoff);
    assert(lv_aic_spi_handoff_close(handoff));return 0;
}
