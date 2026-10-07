/* SPDX-License-Identifier: Apache-2.0 */
#include "lv_aic_spi_display.h"
#include <assert.h>
#include <string.h>
struct lv_aic_spi_worker {
    bool pending,ready,stopped,closed;
    unsigned submitted,consumed;
    uint8_t pixel;
    const uint8_t *source;
    void *cookie;
    lv_aic_spi_result_t result;
};
static struct lv_aic_spi_worker workers[2];
lv_aic_spi_worker_t *lv_aic_spi_worker_create(lv_aic_spi_session_t *s,uint32_t stack,uint32_t priority)
{
    unsigned id=(unsigned)(uintptr_t)s-1;assert(id<2 && stack==4096 && priority==20);
    workers[id].pixel=id?0:255;return &workers[id];
}
lv_aic_spi_result_t lv_aic_spi_worker_submit(lv_aic_spi_worker_t *w,
    const lv_aic_spi_rgb565_frame_t *f,unsigned degrees,void *cookie)
{
    assert(!w->stopped && !w->closed && !w->pending && degrees==0);
    assert(f->width==8 && f->height==4 && f->data[0]==w->pixel && f->data[1]==w->pixel);
    w->source=f->data;w->cookie=cookie;w->pending=true;w->ready=false;w->submitted++;return LV_AIC_SPI_OK;
}
bool lv_aic_spi_worker_take_timed(lv_aic_spi_worker_t *w,lv_aic_spi_result_t *result,
    void **cookie,lv_aic_spi_timing_t *timing)
{
    if(!w->pending || !w->ready) return false;
    assert(w->source[0]==w->pixel && w->source[1]==w->pixel);
    *result=w->result;*cookie=w->cookie;*timing=(lv_aic_spi_timing_t){1,2};
    w->pending=false;w->consumed++;return true;
}
void lv_aic_spi_worker_stop(lv_aic_spi_worker_t *w) { w->stopped=true; }
bool lv_aic_spi_worker_close(lv_aic_spi_worker_t *w)
{ if(!w->stopped || w->pending) return false;w->closed=true;return true; }
void aicos_msleep(unsigned int ms)
{ assert(ms==1);for(unsigned i=0;i<2;i++) if(workers[i].pending) workers[i].ready=true; }
int main(void)
{
    lv_init();lv_aic_spi_display_t *d[2];lv_display_t *lv[2];lv_aic_spi_display_stats_t stats[2];
    for(unsigned i=0;i<2;i++) {
        d[i]=lv_aic_spi_display_create_buffered((void *)(uintptr_t)(i+1),8,4,0,1024,2,4096,20);assert(d[i]);
        lv[i]=lv_aic_spi_display_get(d[i]);lv_obj_t *screen=lv_display_get_screen_active(lv[i]);
        lv_obj_set_style_bg_color(screen,i?lv_color_black():lv_color_white(),0);
        lv_obj_set_style_bg_opa(screen,LV_OPA_COVER,0);lv_refr_now(lv[i]);
    }
    assert(workers[0].pending && workers[1].pending && workers[0].source!=workers[1].source);
    /* A's completion and claim must not consume B's pending frame. */
    workers[0].ready=true;assert(lv_aic_spi_display_claim_blit(d[0])==LV_AIC_SPI_OK);
    assert(!workers[0].pending && workers[1].pending && !workers[1].consumed);
    uint8_t white[64];memset(white,255,sizeof white);
    lv_aic_spi_rgb565_frame_t frame={white,64,16,8,4};
    assert(lv_aic_spi_display_blit(d[0],&frame,0)==LV_AIC_SPI_OK);
    workers[0].result=LV_AIC_SPI_FAULT;workers[0].ready=true;
    lv_aic_spi_result_t result;assert(lv_aic_spi_display_blit_take(d[0],&result) && result==LV_AIC_SPI_FAULT);
    assert(lv_aic_spi_display_stats(d[0],&stats[0]) && stats[0].failed==1 && stats[0].completed==1);
    assert(lv_aic_spi_display_close(d[0]) && workers[0].closed);
    assert(!workers[1].closed && !workers[1].stopped && workers[1].pending);
    for(unsigned i=0;i<4;i++) {
        lv_obj_invalidate(lv_display_get_screen_active(lv[1]));lv_refr_now(lv[1]);
    }
    assert(lv_aic_spi_display_stats(d[1],&stats[1]));
    assert(stats[1].accepted==5 && stats[1].completed==4 && !stats[1].failed && !stats[1].blit_owned);
    lv_display_delete(lv[1]);assert(!lv_aic_spi_display_get(d[1]));
    assert(!lv_aic_spi_display_close(d[1]));workers[1].ready=true;
    assert(lv_aic_spi_display_close(d[1]) && workers[1].closed && workers[1].consumed==5);
    assert(workers[0].submitted==2 && workers[0].consumed==2);return 0;
}
