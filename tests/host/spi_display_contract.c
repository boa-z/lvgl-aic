/* SPDX-License-Identifier: Apache-2.0 */
#include "lv_aic_spi_display.h"
#include <assert.h>
static unsigned submits,takes,sleeps,stops;
static bool pending,ready,stopped,fail_create;
static void *saved_cookie;
static const uint8_t *source;
static uint8_t expected_pixel=255,saved_pixel;
static lv_aic_spi_result_t completion=LV_AIC_SPI_OK;
lv_aic_spi_worker_t *lv_aic_spi_worker_create(lv_aic_spi_session_t *s,uint32_t stack,uint32_t priority)
{ assert(s && stack==4096 && priority==20);stopped=false;return fail_create?NULL:(void *)(uintptr_t)1; }
lv_aic_spi_result_t lv_aic_spi_worker_submit(lv_aic_spi_worker_t *w,
    const lv_aic_spi_rgb565_frame_t *f,unsigned degrees,void *cookie)
{
    assert(w && !pending && !stopped && degrees==90);
    assert(f->width==8 && f->height==4 && f->stride>=16 && f->capacity>=f->stride*4);
    assert(f->data[0]==expected_pixel && f->data[1]==expected_pixel);
    saved_pixel=expected_pixel; /* Actual LVGL rendered black/white RGB565. */
    source=f->data;saved_cookie=cookie;pending=true;ready=false;submits++;return LV_AIC_SPI_OK;
}
bool lv_aic_spi_worker_take(lv_aic_spi_worker_t *w,lv_aic_spi_result_t *result,void **cookie)
{
    assert(w);if(!pending || !ready) return false;
    assert(source[0]==saved_pixel && source[1]==saved_pixel);pending=false;takes++;
    *result=completion;*cookie=saved_cookie;return true;
}
void aicos_msleep(unsigned int ms) { assert(ms==1);sleeps++;ready=true; }
void lv_aic_spi_worker_stop(lv_aic_spi_worker_t *w) { assert(w);stopped=true;stops++; }
bool lv_aic_spi_worker_close(lv_aic_spi_worker_t *w) { assert(w);return stopped && !pending; }
static lv_aic_spi_display_t *create(void)
{
    lv_aic_spi_display_t *d=lv_aic_spi_display_create((void *)(uintptr_t)1,8,4,90,1024,4096,20);
    assert(d);lv_obj_t *screen=lv_display_get_screen_active(lv_aic_spi_display_get(d));
    lv_obj_set_style_bg_color(screen,lv_color_white(),0);
    lv_obj_set_style_bg_opa(screen,LV_OPA_COVER,0);return d;
}
int main(void)
{
    lv_init();
    assert(!lv_aic_spi_display_create((void *)(uintptr_t)1,8,4,90,1,4096,20));
    assert(!lv_aic_spi_display_create((void *)(uintptr_t)1,8,4,45,1024,4096,20));
    fail_create=true;assert(!lv_aic_spi_display_create((void *)(uintptr_t)1,8,4,90,1024,4096,20));fail_create=false;
    lv_aic_spi_display_t *d=create();lv_display_t *display=lv_aic_spi_display_get(d);
    lv_refr_now(display);assert(submits==1);
    lv_obj_invalidate(lv_display_get_screen_active(display));
    lv_refr_now(display);assert(submits==2 && takes>=1 && sleeps>=1);
    ready=true;completion=LV_AIC_SPI_FAULT;
    assert(lv_aic_spi_display_close(d));assert(takes==2 && stops==1);
    d=create();display=lv_aic_spi_display_get(d);lv_refr_now(display);
    lv_display_delete(display);assert(!lv_aic_spi_display_get(d));
    assert(!lv_aic_spi_display_close(d)); /* Live source must survive direct delete. */
    ready=true;assert(lv_aic_spi_display_close(d));assert(takes==3);
    size_t one=(size_t)lv_draw_buf_width_to_stride(8,LV_COLOR_FORMAT_RGB565)*4;
    assert(!lv_aic_spi_display_create_buffered((void *)(uintptr_t)1,8,4,90,one*2-1,2,4096,20));
    assert(!lv_aic_spi_display_create_buffered((void *)(uintptr_t)1,8,4,90,one*2,3,4096,20));
    d=lv_aic_spi_display_create_buffered((void *)(uintptr_t)1,8,4,90,one*2,2,4096,20);assert(d);
    display=lv_aic_spi_display_get(d);lv_obj_t *screen=lv_display_get_screen_active(display);
    lv_obj_set_style_bg_color(screen,lv_color_white(),0);lv_obj_set_style_bg_opa(screen,LV_OPA_COVER,0);
    const uint8_t *buffers[2]={NULL,NULL};completion=LV_AIC_SPI_OK;
    for(unsigned i=0;i<6;i++) {
        expected_pixel=(i%2)?0:255;
        lv_obj_set_style_bg_color(screen,(i%2)?lv_color_black():lv_color_white(),0);
        lv_obj_invalidate(screen);lv_refr_now(display);
        if(i<2) buffers[i]=source;
        assert(source==buffers[i%2]);
        if(i) assert(buffers[0]!=buffers[1]);
    }
    ready=true;assert(lv_aic_spi_display_close(d));assert(takes==9);
    expected_pixel=255;completion=LV_AIC_SPI_OK;d=create();display=lv_aic_spi_display_get(d);
    uint8_t external[64];for(unsigned i=0;i<sizeof external;i++) external[i]=255;
    lv_aic_spi_rgb565_frame_t external_frame={external,sizeof external,16,8,4};
    assert(lv_aic_spi_display_blit(d,&external_frame,90)==LV_AIC_SPI_INVALID);
    lv_refr_now(display);
    assert(lv_aic_spi_display_claim_blit(d)==LV_AIC_SPI_BUSY);
    assert(lv_aic_spi_display_blit(d,&external_frame,90)==LV_AIC_SPI_INVALID);
    ready=true;assert(lv_aic_spi_display_claim_blit(d)==LV_AIC_SPI_OK);
    assert(lv_aic_spi_display_claim_blit(d)==LV_AIC_SPI_OK);
    unsigned before=submits;lv_obj_invalidate(lv_display_get_screen_active(display));lv_refr_now(display);
    assert(submits==before);
    assert(lv_aic_spi_display_blit(d,&external_frame,90)==LV_AIC_SPI_OK && source==external);
    assert(lv_aic_spi_display_blit(d,&external_frame,90)==LV_AIC_SPI_BUSY);
    lv_aic_spi_result_t result;
    assert(!lv_aic_spi_display_blit_take(d,&result));
    lv_refr_now(display);assert(pending && submits==before+1);
    ready=true;assert(lv_aic_spi_display_blit_take(d,&result) && result==LV_AIC_SPI_OK);
    assert(!lv_aic_spi_display_blit_take(d,&result));
    assert(lv_aic_spi_display_blit(d,&external_frame,90)==LV_AIC_SPI_OK);
    assert(!lv_aic_spi_display_close(d));ready=true;assert(lv_aic_spi_display_close(d));
    return 0;
}
