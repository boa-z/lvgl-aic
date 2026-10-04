/* SPDX-License-Identifier: Apache-2.0 */
#include "lvgl_aic_feature_config.h"
#if defined(AIC_LVGL_USE_SPI_SDK) && AIC_LVGL_USE_SPI_SDK
#include "lv_aic_spi_display.h"
#include "lv_aic_spi_pipeline.h"
#include <aic_osal.h>
typedef struct {
    bool used,blit;
    uint32_t submitted_at;
} display_job_t;
struct lv_aic_spi_display {
    lv_display_t *display;
    lv_draw_buf_t *buffer,*second;
    lv_aic_spi_worker_t *worker;
    lv_aic_spi_pipeline_t *pipeline;
    display_job_t jobs[2];
    unsigned inflight;
    bool source_pending;
    unsigned degrees;
    bool pending,closing,claiming,blit,pending_blit;
    lv_aic_spi_result_t result;
    lv_aic_spi_display_stats_t stats;
    uint32_t submitted_at;
};
static void increment(uint32_t *counter) { if(*counter<UINT32_MAX) (*counter)++; }
static void accepted(lv_aic_spi_display_t *d)
{
    d->pending=true;d->submitted_at=lv_tick_get();increment(&d->stats.accepted);
}
static void record_completion(lv_aic_spi_display_t *d,uint32_t submitted_at)
{
    d->stats.last_completion=d->result;
    if(d->result==LV_AIC_SPI_OK) increment(&d->stats.completed);
    else increment(&d->stats.failed);
    d->stats.last_observed_ms=lv_tick_elaps(submitted_at);
    if(d->stats.last_observed_ms>d->stats.max_observed_ms)
        d->stats.max_observed_ms=d->stats.last_observed_ms;
}
static bool collect(lv_aic_spi_display_t *d)
{
    if(!d->pending) return true;
    if(d->pipeline) {
        lv_aic_spi_event_t event;
        while(lv_aic_spi_pipeline_take(d->pipeline,&event)) {
            display_job_t *job=event.cookie;
            if(event.kind==LV_AIC_SPI_SOURCE_RELEASED) {
                if(!job->blit) {
                    d->source_pending=false;
                    if(d->display) lv_display_flush_ready(d->display);
                }
            } else {
                d->result=event.result;d->stats.last_worker=event.timing;
                record_completion(d,job->submitted_at);
                if(job->blit) d->pending_blit=false;
                job->used=false;d->inflight--;
            }
        }
        d->pending=d->inflight!=0;return !d->pending;
    }
    void *cookie;
    if(!lv_aic_spi_worker_take_timed(d->worker,&d->result,&cookie,&d->stats.last_worker)) return false;
    d->pending=false;
    record_completion(d,d->submitted_at);
    if(d->display && !d->pending_blit) lv_display_flush_ready(d->display);
    d->pending_blit=false;
    return true;
}
static lv_aic_spi_result_t submit(lv_aic_spi_display_t *d,
    const lv_aic_spi_rgb565_frame_t *frame,unsigned degrees,bool blit)
{
    if(d->pipeline) {
        display_job_t *job=NULL;
        for(unsigned i=0;i<2;i++) if(!d->jobs[i].used) { job=&d->jobs[i];break; }
        if(!job) return LV_AIC_SPI_BUSY;
        lv_aic_spi_result_t result=lv_aic_spi_pipeline_submit(d->pipeline,frame,degrees,job);
        if(result!=LV_AIC_SPI_OK) return result;
        job->used=true;job->blit=blit;job->submitted_at=lv_tick_get();
        d->inflight++;if(!blit) d->source_pending=true;
    } else {
        lv_aic_spi_result_t result=lv_aic_spi_worker_submit(d->worker,frame,degrees,d);
        if(result!=LV_AIC_SPI_OK) return result;
    }
    accepted(d);return LV_AIC_SPI_OK;
}
static void stop(lv_aic_spi_display_t *d)
{
    if(d->pipeline) lv_aic_spi_pipeline_stop(d->pipeline);
    else if(d->worker) lv_aic_spi_worker_stop(d->worker);
}
static void flush(lv_display_t *display,const lv_area_t *area,uint8_t *pixels)
{
    lv_aic_spi_display_t *d=lv_display_get_driver_data(display);
    (void)area;(void)pixels;
    if(d->closing || d->claiming || !lv_display_flush_is_last(display)) {
        lv_display_flush_ready(display);return;
    }
    lv_draw_buf_t *active=lv_display_get_buf_active(display);
    if(!active || (active!=d->buffer && active!=d->second)) {
        increment(&d->stats.rejected);d->result=LV_AIC_SPI_INVALID;lv_display_flush_ready(display);return;
    }
    lv_aic_spi_rgb565_frame_t frame={active->data,active->data_size,
        active->header.stride,active->header.w,active->header.h};
    if(d->pipeline) {
        /* Source release can precede DMA completion. Wait for an event slot
         * only when both frames still have unconsumed checked results. */
        collect(d);
        while(d->inflight==2) { aicos_msleep(1);collect(d); }
    }
    d->result=submit(d,&frame,d->degrees,false);
    if(d->result!=LV_AIC_SPI_OK) { increment(&d->stats.rejected);lv_display_flush_ready(display); }
}
static void wait_flush(lv_display_t *display)
{
    lv_aic_spi_display_t *d=lv_display_get_driver_data(display);
    if(d->pending_blit) return;
    if(d->pipeline) {
        while(d->source_pending) { collect(d);if(d->source_pending) aicos_msleep(1); }
    } else while(!collect(d)) aicos_msleep(1);
}
static void deleted(lv_event_t *event)
{
    lv_aic_spi_display_t *d=lv_event_get_user_data(event);
    d->display=NULL;d->closing=true;
    stop(d);
}
static lv_aic_spi_display_t *create(lv_aic_spi_session_t *session,
    uint32_t width,uint32_t height,unsigned degrees,size_t budget,unsigned count,
    uint32_t stack,uint32_t priority,bool pipeline)
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
    if(pipeline) d->pipeline=lv_aic_spi_pipeline_create(session,stack,priority);
    else d->worker=lv_aic_spi_worker_create(session,stack,priority);
    if(!d->worker && !d->pipeline) {
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
lv_aic_spi_display_t *lv_aic_spi_display_create(lv_aic_spi_session_t *session,
    uint32_t width,uint32_t height,unsigned degrees,size_t budget,uint32_t stack,uint32_t priority)
{ return create(session,width,height,degrees,budget,1,stack,priority,false); }
lv_aic_spi_display_t *lv_aic_spi_display_create_buffered(lv_aic_spi_session_t *session,
    uint32_t width,uint32_t height,unsigned degrees,size_t budget,unsigned count,uint32_t stack,uint32_t priority)
{ return create(session,width,height,degrees,budget,count,stack,priority,false); }
lv_aic_spi_display_t *lv_aic_spi_display_create_pipelined(lv_aic_spi_session_t *session,
    uint32_t width,uint32_t height,unsigned degrees,size_t budget,unsigned count,uint32_t stack,uint32_t priority)
{ return create(session,width,height,degrees,budget,count,stack,priority,true); }
bool lv_aic_spi_display_poll(lv_aic_spi_display_t *d)
{ return d && !d->pending_blit && collect(d); }
lv_display_t *lv_aic_spi_display_get(lv_aic_spi_display_t *d) { return d?d->display:NULL; }
lv_aic_spi_result_t lv_aic_spi_display_result(lv_aic_spi_display_t *d)
{ return d?d->result:LV_AIC_SPI_INVALID; }
lv_aic_spi_result_t lv_aic_spi_display_claim_blit(lv_aic_spi_display_t *d)
{
    if(!d || d->closing || !d->display) return LV_AIC_SPI_INVALID;
    if(d->blit) return LV_AIC_SPI_OK;
    if(!d->claiming) {
        d->claiming=true;
        lv_timer_pause(lv_display_get_refr_timer(d->display));
        lv_display_enable_invalidation(d->display,false);
    }
    if(!collect(d)) return LV_AIC_SPI_BUSY;
    if(d->result==LV_AIC_SPI_FAULT || d->result==LV_AIC_SPI_BUSY) return d->result;
    d->blit=true;return LV_AIC_SPI_OK;
}
lv_aic_spi_result_t lv_aic_spi_display_blit(lv_aic_spi_display_t *d,
    const lv_aic_spi_rgb565_frame_t *frame,unsigned degrees)
{
    if(!d) return LV_AIC_SPI_INVALID;
    if(!d->blit || d->closing || !frame) { increment(&d->stats.rejected);return LV_AIC_SPI_INVALID; }
    if(d->pending) { increment(&d->stats.rejected);return LV_AIC_SPI_BUSY; }
    d->result=submit(d,frame,degrees,true);
    if(d->result==LV_AIC_SPI_OK) d->pending_blit=true;
    else increment(&d->stats.rejected);
    return d->result;
}
bool lv_aic_spi_display_blit_take(lv_aic_spi_display_t *d,lv_aic_spi_result_t *result)
{
    if(!d || !result || !d->pending_blit || !collect(d)) return false;
    *result=d->result;return true;
}
bool lv_aic_spi_display_stats(lv_aic_spi_display_t *d,lv_aic_spi_display_stats_t *stats)
{
    if(!d || !stats) return false;
    *stats=d->stats;stats->pending=d->pending;stats->blit_owned=d->blit;stats->closing=d->closing;
    return true;
}
bool lv_aic_spi_display_close(lv_aic_spi_display_t *d)
{
    if(!d) return false;
    if(!d->closing) {
        d->closing=true;
        lv_timer_pause(lv_display_get_refr_timer(d->display));
        stop(d);
    }
    if(!collect(d)) return false;
    if(d->pipeline) { if(!lv_aic_spi_pipeline_close(d->pipeline)) return false; }
    else if(!lv_aic_spi_worker_close(d->worker)) return false;
    d->worker=NULL;d->pipeline=NULL;
    if(d->display) lv_display_delete(d->display);
    lv_draw_buf_destroy(d->second);lv_draw_buf_destroy(d->buffer);lv_free(d);return true;
}
#endif
