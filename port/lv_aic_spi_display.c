/* SPDX-License-Identifier: Apache-2.0 */
#include "lvgl_aic_feature_config.h"
#if defined(AIC_LVGL_USE_SPI_SDK) && AIC_LVGL_USE_SPI_SDK
#include "lv_aic_spi_display.h"
#include <aic_osal.h>
struct lv_aic_spi_display {
    lv_display_t *display;
    lv_draw_buf_t *buffer,*second;
    lv_aic_spi_worker_t *worker;
    unsigned degrees;
    bool pending,closing;
    lv_aic_spi_result_t result;
};
static bool collect(lv_aic_spi_display_t *d)
{
    if(!d->pending) return true;
    void *cookie;
    if(!lv_aic_spi_worker_take(d->worker,&d->result,&cookie)) return false;
    d->pending=false;
    if(d->display) lv_display_flush_ready(d->display);
    return true;
}
static void flush(lv_display_t *display,const lv_area_t *area,uint8_t *pixels)
{
    lv_aic_spi_display_t *d=lv_display_get_driver_data(display);
    (void)area;(void)pixels;
    if(d->closing || !lv_display_flush_is_last(display)) {
        lv_display_flush_ready(display);return;
    }
    lv_draw_buf_t *active=lv_display_get_buf_active(display);
    if(!active || (active!=d->buffer && active!=d->second)) {
        d->result=LV_AIC_SPI_INVALID;lv_display_flush_ready(display);return;
    }
    lv_aic_spi_rgb565_frame_t frame={active->data,active->data_size,
        active->header.stride,active->header.w,active->header.h};
    d->result=lv_aic_spi_worker_submit(d->worker,&frame,d->degrees,d);
    if(d->result==LV_AIC_SPI_OK) d->pending=true;
    else lv_display_flush_ready(display);
}
static void wait_flush(lv_display_t *display)
{
    lv_aic_spi_display_t *d=lv_display_get_driver_data(display);
    while(!collect(d)) aicos_msleep(1);
}
static void deleted(lv_event_t *event)
{
    lv_aic_spi_display_t *d=lv_event_get_user_data(event);
    d->display=NULL;d->closing=true;
    if(d->worker) lv_aic_spi_worker_stop(d->worker);
}
lv_aic_spi_display_t *lv_aic_spi_display_create(lv_aic_spi_session_t *session,
    uint32_t width,uint32_t height,unsigned degrees,size_t budget,uint32_t stack,uint32_t priority)
{
    return lv_aic_spi_display_create_buffered(session,width,height,degrees,budget,1,stack,priority);
}
lv_aic_spi_display_t *lv_aic_spi_display_create_buffered(lv_aic_spi_session_t *session,
    uint32_t width,uint32_t height,unsigned degrees,size_t budget,unsigned count,uint32_t stack,uint32_t priority)
{
    if((count!=1 && count!=2) || !session || !width || !height || width>4096 || height>4096 ||
       (degrees!=0 && degrees!=90 && degrees!=180 && degrees!=270)) return NULL;
    uint32_t stride=lv_draw_buf_width_to_stride(width,LV_COLOR_FORMAT_RGB565);
    if(stride<width*2 || (uint64_t)stride*height*count>budget) return NULL;
    lv_aic_spi_display_t *d=lv_malloc_zeroed(sizeof(*d));
    if(!d) return NULL;
    d->buffer=lv_draw_buf_create(width,height,LV_COLOR_FORMAT_RGB565,stride);
    if(!d->buffer) { lv_free(d);return NULL; }
    if(count==2) {
        d->second=lv_draw_buf_create(width,height,LV_COLOR_FORMAT_RGB565,stride);
        if(!d->second) { lv_draw_buf_destroy(d->buffer);lv_free(d);return NULL; }
    }
    d->display=lv_display_create(width,height);
    if(!d->display) { lv_draw_buf_destroy(d->second);lv_draw_buf_destroy(d->buffer);lv_free(d);return NULL; }
    d->worker=lv_aic_spi_worker_create(session,stack,priority);
    if(!d->worker) {
        lv_display_delete(d->display);lv_draw_buf_destroy(d->second);lv_draw_buf_destroy(d->buffer);lv_free(d);return NULL;
    }
    d->degrees=degrees;
    lv_display_set_color_format(d->display,LV_COLOR_FORMAT_RGB565);
    lv_display_set_draw_buffers(d->display,d->buffer,d->second);
    lv_display_set_render_mode(d->display,LV_DISPLAY_RENDER_MODE_FULL);
    lv_display_set_driver_data(d->display,d);
    lv_display_set_flush_cb(d->display,flush);
    lv_display_set_flush_wait_cb(d->display,wait_flush);
    lv_display_add_event_cb(d->display,deleted,LV_EVENT_DELETE,d);
    return d;
}
lv_display_t *lv_aic_spi_display_get(lv_aic_spi_display_t *d) { return d?d->display:NULL; }
lv_aic_spi_result_t lv_aic_spi_display_result(lv_aic_spi_display_t *d)
{ return d?d->result:LV_AIC_SPI_INVALID; }
bool lv_aic_spi_display_close(lv_aic_spi_display_t *d)
{
    if(!d) return false;
    if(!d->closing) {
        d->closing=true;
        lv_timer_pause(lv_display_get_refr_timer(d->display));
        lv_aic_spi_worker_stop(d->worker);
    }
    if(!collect(d) || !lv_aic_spi_worker_close(d->worker)) return false;
    d->worker=NULL;
    if(d->display) lv_display_delete(d->display);
    lv_draw_buf_destroy(d->second);lv_draw_buf_destroy(d->buffer);lv_free(d);return true;
}
#endif
