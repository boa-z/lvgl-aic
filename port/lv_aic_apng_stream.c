/* SPDX-License-Identifier: Apache-2.0 */
#include "lvgl_aic_feature_config.h"
#if defined(AIC_LVGL_USE_APNG) && AIC_LVGL_USE_APNG
#include "lv_aic_apng_stream.h"
#include "lv_aic_apng_decoder.h"
#include "lv_aic_apng_compose.h"
#include <stdlib.h>
#include <string.h>
struct lv_aic_apng_stream {
    lv_aic_apng_t doc;
    lv_aic_apng_canvas_t canvas;
    lv_aic_apng_timeline_t clock;
    lv_aic_apng_decoder_t *decoder;
    uint8_t *storage,*png,*rectangle;
    size_t png_bytes,rectangle_bytes,cursor;
    uint64_t sequence;
    uint64_t (*now_us)(void *context);
    void *clock_context;
    bool fault;
};
static bool add(size_t *total,size_t n,size_t limit)
{ if(*total>limit || n>limit-*total) return false; *total+=n; return true; }
lv_aic_apng_stream_t *lv_aic_apng_stream_open(const void *data,size_t bytes,
    const lv_aic_apng_stream_config_t *c)
{
    lv_aic_apng_t doc;
    if(!c || !c->now_us || !data || !bytes || bytes-1>UINTPTR_MAX-(uintptr_t)data ||
       !lv_aic_apng_open(data,bytes,&c->limits,&doc)) return NULL;
    size_t png_bytes=0,rectangle_bytes=0,cursor=0;
    lv_aic_apng_frame_t f;
    for(uint32_t i=0;i<doc.frames;i++) {
        if(!lv_aic_apng_next(&doc,&cursor,&f)) return NULL;
        size_t n=(size_t)f.width*f.height*4;
        if(n>rectangle_bytes) rectangle_bytes=n;
        if(f.png_bytes>png_bytes) png_bytes=f.png_bytes;
    }
    if(png_bytes>SIZE_MAX-255 || ((png_bytes+255)&~(size_t)255)>c->packet_limit) return NULL;
    size_t canvas_bytes=(size_t)doc.width*doc.height*4,total=sizeof(lv_aic_apng_stream_t);
    if(!add(&total,bytes,c->cpu_budget) || !add(&total,png_bytes,c->cpu_budget) ||
       !add(&total,rectangle_bytes,c->cpu_budget) || !add(&total,canvas_bytes,c->cpu_budget) ||
       !add(&total,canvas_bytes,c->cpu_budget)) return NULL;
    lv_aic_apng_timeline_t clock;
    if(!lv_aic_apng_timeline_init(&clock,doc.frames,doc.plays,doc.animated,
                                c->minimum_delay_us,c->now_us(c->clock_context))) return NULL;
    lv_aic_apng_stream_t *s=calloc(1,sizeof(*s));
    if(!s) return NULL;
    s->storage=malloc(total-sizeof(*s));
    if(!s->storage) { free(s); return NULL; }
    s->decoder=lv_aic_apng_decoder_create(c->frame_budget,c->packet_limit);
    if(!s->decoder) { free(s->storage);free(s);return NULL; }
    memcpy(s->storage,data,bytes);doc.data=s->storage;s->doc=doc;
    s->png=s->storage+bytes;s->png_bytes=png_bytes;
    s->rectangle=s->png+png_bytes;s->rectangle_bytes=rectangle_bytes;
    uint8_t *pixels=s->rectangle+rectangle_bytes;
    if(!lv_aic_apng_canvas_init(&s->canvas,doc.width,doc.height,pixels,(size_t)doc.width*4,
                               canvas_bytes,pixels+canvas_bytes,canvas_bytes)) {
        lv_aic_apng_stream_close(s);return NULL;
    }
    s->clock=clock;s->now_us=c->now_us;s->clock_context=c->clock_context;
    return s;
}
bool lv_aic_apng_stream_close(lv_aic_apng_stream_t *s)
{
    if(!s) return true;
    /* A pending frame return must keep every owner alive for retry. */
    s->fault=true;
    if(!lv_aic_apng_decoder_destroy(s->decoder)) return false;
    free(s->storage);free(s);return true;
}
bool lv_aic_apng_stream_tick(lv_aic_apng_stream_t *s,lv_aic_apng_stream_view_t *view)
{
    if(!s || !view || s->fault) return false;
    lv_aic_apng_step_t step;
    if(!lv_aic_apng_timeline_poll(&s->clock,s->now_us(s->clock_context),&step)) goto fault;
    if(step.action==LV_AIC_APNG_FRAME) {
        if(s->sequence==UINT64_MAX) goto fault;
        size_t cursor=step.reset_canvas?0:s->cursor;
        lv_aic_apng_frame_t f;
        if(!lv_aic_apng_next(&s->doc,&cursor,&f) ||
           !lv_aic_apng_extract(&s->doc,&f,s->png,s->png_bytes) ||
           !lv_aic_apng_decoder_decode(s->decoder,s->png,f.png_bytes,f.width,f.height,
                                      s->rectangle,(size_t)f.width*4,s->rectangle_bytes)) goto fault;
        if(step.reset_canvas) lv_aic_apng_canvas_reset(&s->canvas);
        if(!lv_aic_apng_compose(&s->canvas,&f,s->rectangle,(size_t)f.width*4,s->rectangle_bytes) ||
           !lv_aic_apng_timeline_commit(&s->clock,s->now_us(s->clock_context),f.delay_num,f.delay_den)) goto fault;
        s->cursor=cursor;s->sequence++;
    }
    *view=(lv_aic_apng_stream_view_t){.step=step,.rgba=s->sequence?s->canvas.pixels:NULL,
        .width=s->doc.width,.height=s->doc.height,.stride=s->canvas.stride,
        .bytes=s->canvas.capacity,.sequence=s->sequence};return true;
fault:
    s->fault=true;return false;
}
bool lv_aic_apng_stream_pause(lv_aic_apng_stream_t *s,bool paused)
{ return s && !s->fault && lv_aic_apng_timeline_pause(&s->clock,s->now_us(s->clock_context),paused); }
bool lv_aic_apng_stream_rate(lv_aic_apng_stream_t *s,uint32_t num,uint32_t den)
{ return s && !s->fault && lv_aic_apng_timeline_rate(&s->clock,s->now_us(s->clock_context),num,den); }
bool lv_aic_apng_stream_restart(lv_aic_apng_stream_t *s)
{
    if(!s || s->fault) return false;
    uint64_t now=s->now_us(s->clock_context);
    if(now<s->clock.last_us) return false;
    lv_aic_apng_timeline_t clock;
    if(!lv_aic_apng_timeline_init(&clock,s->doc.frames,s->doc.plays,s->doc.animated,
                                s->clock.minimum_delay_us,now) ||
       !lv_aic_apng_timeline_rate(&clock,now,s->clock.rate_num,s->clock.rate_den) ||
       !lv_aic_apng_timeline_pause(&clock,now,s->clock.paused)) return false;
    s->clock=clock;s->cursor=0;return true;
}
#endif
