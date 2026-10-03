/* SPDX-License-Identifier: Apache-2.0 */
#include "lv_aic_apng_compose.h"
#include <string.h>
static bool span(uint32_t w,uint32_t h,size_t stride,size_t capacity,size_t *out)
{
    if(!w || !h || w>4096 || h>4096 || stride<(size_t)w*4) return false;
    size_t row=(size_t)w*4;
    if(h>1 && stride>(SIZE_MAX-row)/(h-1)) return false;
    *out=(h-1)*stride+row; return *out<=capacity;
}
static bool separate(const void *a,size_t an,const void *b,size_t bn)
{
    uintptr_t x=(uintptr_t)a,y=(uintptr_t)b;
    if(!a || !b || an>UINTPTR_MAX-x || bn>UINTPTR_MAX-y) return false;
    return x<y?y-x>=an:x-y>=bn;
}
void lv_aic_apng_canvas_reset(lv_aic_apng_canvas_t *c)
{
    if(!c || !c->pixels) return;
    for(uint32_t y=0;y<c->height;y++) memset(c->pixels+y*c->stride,0,(size_t)c->width*4);
    c->have_previous=false;
}
bool lv_aic_apng_canvas_init(lv_aic_apng_canvas_t *out,uint32_t w,uint32_t h,
    void *pixels,size_t stride,size_t capacity,void *scratch,size_t scratch_capacity)
{
    size_t bytes;
    if(!out || !span(w,h,stride,capacity,&bytes) || scratch_capacity<(size_t)w*h*4 ||
       !separate(pixels,bytes,scratch,(size_t)w*h*4)) return false;
    *out=(lv_aic_apng_canvas_t){.pixels=pixels,.scratch=scratch,.stride=stride,.capacity=capacity,
        .scratch_capacity=scratch_capacity,.width=w,.height=h};
    lv_aic_apng_canvas_reset(out); return true;
}
static void rectangle(lv_aic_apng_canvas_t *c,const lv_aic_apng_frame_t *f,unsigned op)
{
    for(uint32_t y=0;y<f->height;y++) {
        uint8_t *row=c->pixels+(f->y+y)*c->stride+(size_t)f->x*4;
        uint8_t *saved=c->scratch+(size_t)y*f->width*4;
        if(op==0) memset(row,0,(size_t)f->width*4);
        else if(op==1) memcpy(row,saved,(size_t)f->width*4);
        else memcpy(saved,row,(size_t)f->width*4);
    }
}
bool lv_aic_apng_compose(lv_aic_apng_canvas_t *c,const lv_aic_apng_frame_t *f,
    const void *rgba,size_t stride,size_t capacity)
{
    size_t bytes,canvas_bytes;
    if(!c || !f || !span(f->width,f->height,stride,capacity,&bytes) ||
       !span(c->width,c->height,c->stride,c->capacity,&canvas_bytes) ||
       f->x>c->width || f->y>c->height || f->width>c->width-f->x || f->height>c->height-f->y ||
       f->dispose>2 || f->blend>1 || c->scratch_capacity<(size_t)c->width*c->height*4 ||
       !separate(rgba,bytes,c->pixels,canvas_bytes) ||
       !separate(rgba,bytes,c->scratch,(size_t)c->width*c->height*4)) return false;
    if(c->have_previous) {
        if(c->previous.dispose==1) rectangle(c,&c->previous,0);
        else if(c->previous.dispose==2) rectangle(c,&c->previous,1);
    }
    if(f->dispose==2) rectangle(c,f,2);
    for(uint32_t y=0;y<f->height;y++) {
        const uint8_t *src=(const uint8_t *)rgba+y*stride;
        uint8_t *dst=c->pixels+(f->y+y)*c->stride+(size_t)f->x*4;
        if(!f->blend) { memcpy(dst,src,(size_t)f->width*4); continue; }
        for(uint32_t x=0;x<f->width;x++,src+=4,dst+=4) {
            unsigned sa=src[3],da=dst[3],inverse=255-sa;
            if(!sa) continue;
            if(sa==255) { memcpy(dst,src,4); continue; }
            unsigned alpha=sa*255+da*inverse;
            for(unsigned channel=0;channel<3;channel++)
                dst[channel]=(uint8_t)((src[channel]*sa*255+dst[channel]*da*inverse+alpha/2)/alpha);
            dst[3]=(uint8_t)((alpha+127)/255);
        }
    }
    c->previous=*f; c->have_previous=true; return true;
}
