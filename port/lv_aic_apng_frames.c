/* SPDX-License-Identifier: Apache-2.0 */
#include "lvgl_aic_feature_config.h"
#if defined(AIC_LVGL_USE_APNG) && AIC_LVGL_USE_APNG
#include "lv_aic_apng_frames.h"
#include <aic_osal.h>
#include <stdlib.h>
enum { FREE,WRITING,READY,CLAIMED,LEASED };
struct slot {
    struct lv_aic_apng_frames *pool;
    uint8_t *pixels;
    uint64_t sequence;
    unsigned state;
};
struct lv_aic_apng_frames {
    aicos_mutex_t mutex;
    uint32_t width,height;
    size_t bytes;
    unsigned count;
    uint64_t last_sequence;
    bool closed;
    struct slot slots[8];
};
static void lock(lv_aic_apng_frames_t *p) { aicos_mutex_take(p->mutex,AICOS_WAIT_FOREVER); }
static void unlock(lv_aic_apng_frames_t *p) { aicos_mutex_give(p->mutex); }
lv_aic_apng_frames_t *lv_aic_apng_frames_create(uint32_t w,uint32_t h,unsigned count,size_t budget)
{
    if(!w || !h || w>4096 || h>4096 || (uint64_t)w*h>8U*1024U*1024U || count<2 || count>8) return NULL;
    size_t bytes=(size_t)w*h*4;
    if(budget<sizeof(lv_aic_apng_frames_t) || bytes>(budget-sizeof(lv_aic_apng_frames_t))/count) return NULL;
    lv_aic_apng_frames_t *p=calloc(1,sizeof(*p)+bytes*count);
    if(!p) return NULL;
    p->mutex=aicos_mutex_create();if(!p->mutex) { free(p);return NULL; }
    p->width=w;p->height=h;p->count=count;p->bytes=bytes;
    for(unsigned i=0;i<count;i++) {
        p->slots[i].pool=p;p->slots[i].pixels=(uint8_t *)(p+1)+bytes*i;
    }
    return p;
}
bool lv_aic_apng_frames_publish(lv_aic_apng_frames_t *p,const void *rgba,size_t stride,size_t capacity,uint64_t seq)
{
    if(!p || !rgba || !seq || stride<(size_t)p->width*4 ||
       (p->height>1 && stride>(SIZE_MAX-(size_t)p->width*4)/(p->height-1)) ||
       (size_t)(p->height-1)*stride+(size_t)p->width*4>capacity ||
       !capacity || capacity-1>UINTPTR_MAX-(uintptr_t)rgba) return false;
    lock(p);
    if(p->closed || seq<=p->last_sequence) { unlock(p);return false; }
    struct slot *slot=NULL;
    for(unsigned i=0;i<p->count;i++) if(p->slots[i].state==READY) { slot=&p->slots[i];break; }
    if(!slot) for(unsigned i=0;i<p->count;i++) if(p->slots[i].state==FREE) { slot=&p->slots[i];break; }
    if(!slot) { unlock(p);return false; }
    /* Disallow publishing any pool-backed input, including a leased snapshot. */
    uintptr_t src=(uintptr_t)rgba,base=(uintptr_t)(p+1);size_t total=p->bytes*p->count;
    if(src<=base?base-src<capacity:src-base<total) { unlock(p);return false; }
    slot->state=WRITING;unlock(p);
    for(uint32_t y=0;y<p->height;y++) {
        const uint8_t *s=(const uint8_t *)rgba+(size_t)y*stride;
        uint32_t *out=(uint32_t *)(slot->pixels+(size_t)y*p->width*4);
        for(uint32_t x=0;x<p->width;x++,s+=4)
            out[x]=((uint32_t)s[3]<<24)|((uint32_t)s[0]<<16)|((uint32_t)s[1]<<8)|s[2];
    }
    lock(p);
    bool accepted=!p->closed;
    if(accepted) { slot->sequence=seq;p->last_sequence=seq;slot->state=READY; }
    else slot->state=FREE;
    unlock(p);return accepted;
}
static bool retain(void *context)
{
    struct slot *s=context;lock(s->pool);
    bool ok=s->state==CLAIMED;
    if(ok) s->state=LEASED;
    unlock(s->pool);return ok;
}
static void release(void *context)
{ struct slot *s=context;lock(s->pool);s->state=FREE;unlock(s->pool); }
bool lv_aic_apng_frames_poll(lv_aic_apng_frames_t *p,lv_aic_rgb_image_t **out,uint64_t *sequence)
{
    if(!p || !out || !sequence) return false;
    lock(p);struct slot *s=NULL;
    if(!p->closed) for(unsigned i=0;i<p->count;i++) if(p->slots[i].state==READY) { s=&p->slots[i];break; }
    if(!s) { unlock(p);return false; }
    s->state=CLAIMED;uint64_t seq=s->sequence;unlock(p);
    lv_aic_rgb_frame_t frame={.format=LV_COLOR_FORMAT_ARGB8888,.width=p->width,.height=p->height,
        .stride=p->width*4,.data=s->pixels,.capacity=p->bytes};
    lv_aic_rgb_image_t *image=lv_aic_rgb_image_create(&frame,retain,release,s);
    if(!image) { lock(p);s->state=FREE;unlock(p);return false; }
    *out=image;*sequence=seq;return true;
}
void lv_aic_apng_frames_close(lv_aic_apng_frames_t *p)
{
    if(!p) return;
    lock(p);p->closed=true;
    for(unsigned i=0;i<p->count;i++) if(p->slots[i].state==READY) p->slots[i].state=FREE;
    unlock(p);
}
bool lv_aic_apng_frames_destroy(lv_aic_apng_frames_t *p)
{
    if(!p) return true;
    lv_aic_apng_frames_close(p);lock(p);
    for(unsigned i=0;i<p->count;i++) if(p->slots[i].state!=FREE) { unlock(p);return false; }
    unlock(p);aicos_mutex_delete(p->mutex);free(p);return true;
}
#endif
