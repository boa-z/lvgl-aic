/* SPDX-License-Identifier: Apache-2.0
 * Private shared staging: caller owns the buffer and retains it after DMA faults. */
#ifndef LV_DRAW_AIC_GE2D_ALPHA_H
#define LV_DRAW_AIC_GE2D_ALPHA_H
#include "lvgl_aic_private.h"
#include "lv_draw_aic_ge2d_utils.h"
#include <aic_osal.h>
#include <limits.h>
#ifndef AIC_LVGL_GE2D_ALPHA_BYTES
#define AIC_LVGL_GE2D_ALPHA_BYTES (2U * 1024U * 1024U)
#endif

static inline bool lv_aic_ge2d_alpha_prepare(lv_draw_task_t *task,const lv_area_t *visible,
    lv_draw_buf_t *surface,lv_layer_t *layer,lv_draw_task_t *copy)
{
    if(!task || !task->target_layer || !visible || !surface || surface->data) return false;
    const lv_layer_t *dst=task->target_layer;
    const lv_draw_buf_t *buf=dst->draw_buf;
    lv_area_t area;
    if(!buf || !buf->data || (buf->header.flags&LV_IMAGE_FLAGS_PREMULTIPLIED) ||
       buf->header.cf!=LV_COLOR_FORMAT_ARGB8888 || dst->color_format!=LV_COLOR_FORMAT_ARGB8888 ||
       !lv_area_intersect(&area,visible,&dst->buf_area)) return false;
    int64_t width=(int64_t)dst->buf_area.x2-dst->buf_area.x1+1;
    int64_t height=(int64_t)dst->buf_area.y2-dst->buf_area.y1+1;
    if(width<=0 || height<=0 || width>buf->header.w || height>buf->header.h ||
       width*4>buf->header.stride || (uint64_t)buf->header.stride*height>buf->data_size) return false;
    uint32_t w=lv_area_get_width(&area),h=lv_area_get_height(&area);
    if(w>4096 || h>4096) return false;
    uint32_t stride=(w*4U+63U)&~63U;
    uint64_t bytes=(uint64_t)stride*h;
    if(bytes>AIC_LVGL_GE2D_ALPHA_BYTES || bytes>UINT32_MAX || !bytes) return false;
    void *data=aicos_malloc_align(MEM_CMA,(size_t)bytes,64);
    if(!data) return false;
    if(((uintptr_t)data&63U) ||
       lv_draw_buf_init(surface,w,h,LV_COLOR_FORMAT_ARGB8888,stride,data,(uint32_t)bytes)!=LV_RESULT_OK ||
       !lv_draw_aic_ge2d_buf_address_valid(surface)) {
        aicos_free_align(MEM_CMA,data);lv_memzero(surface,sizeof *surface);return false;
    }
    lv_memzero(surface->data,surface->data_size);
    lv_memzero(layer,sizeof *layer);layer->draw_buf=surface;
    layer->color_format=LV_COLOR_FORMAT_ARGB8888;layer->buf_area=area;
    *copy=*task;copy->target_layer=layer;
    lv_area_t relative=area;lv_area_move(&relative,-dst->buf_area.x1,-dst->buf_area.y1);
    lv_draw_aic_ge2d_prepare_dst_cache(buf,&relative);
    return true;
}

/* GE v1.1 leaves the source Y sample in the destination alpha lane when a YUV
 * frame is converted into the private ARGB surface (board run 9: uniform Y=100
 * produced a 39-level alpha error against the opaque oracle). YUV has no source
 * alpha, so the opaque staging pass cannot ask the engine to pass one through.
 * Every crop the engine actually wrote must therefore have its alpha lane
 * normalized before any CPU pass reads the surface. Gaps the engine never wrote
 * keep their zeroed alpha and stay excluded from the tail blend. */
static inline void lv_aic_ge2d_alpha_mark_opaque(lv_draw_buf_t *surface,const lv_area_t *rel_area)
{
    if(!surface || !surface->data || !rel_area ||
       surface->header.cf!=LV_COLOR_FORMAT_ARGB8888 ||
       rel_area->x1<0 || rel_area->y1<0) return;
    uint32_t x1=(uint32_t)rel_area->x1,y1=(uint32_t)rel_area->y1;
    uint32_t x2=(uint32_t)rel_area->x2,y2=(uint32_t)rel_area->y2;
    if(x1>=surface->header.w || y1>=surface->header.h) return;
    if(x2>=surface->header.w) x2=surface->header.w-1;
    if(y2>=surface->header.h) y2=surface->header.h-1;
    for(uint32_t y=y1;y<=y2;y++) {
        uint8_t *row=surface->data+(size_t)y*surface->header.stride;
        for(uint32_t x=x1;x<=x2;x++) row[x*4U+3U]=0xff;
    }
}

static inline void lv_aic_ge2d_alpha_finish(lv_draw_task_t *task,const lv_draw_buf_t *surface,
    const lv_layer_t *layer,lv_opa_t opa,lv_color_format_t source_cf,bool opaque_coverage)
{
    aicos_dcache_invalid_range((unsigned long *)surface->data,surface->data_size);
    lv_draw_sw_blend_dsc_t blend={0};
    blend.blend_area=&layer->buf_area;blend.src_area=&layer->buf_area;
    blend.src_buf=surface->data;blend.src_stride=surface->header.stride;
    blend.src_color_format=source_cf;
    blend.opa=opa;blend.blend_mode=LV_BLEND_MODE_NORMAL;
    if(!opaque_coverage) lv_draw_sw_blend(task,&blend);
    else {
        /* Opaque YUV has alpha zero only in unwritten tile/rotation holes.
         * Skip those spans: even hidden RGB bytes of a transparent target
         * must survive where no source pixel was rendered. */
        for(uint32_t y=0;y<surface->header.h;y++) {
            const uint8_t *row=surface->data+y*surface->header.stride;
            uint32_t x=0;
            while(x<surface->header.w) {
                while(x<surface->header.w && row[x*4+3]==0) x++;
                uint32_t start=x;
                while(x<surface->header.w && row[x*4+3]!=0) x++;
                if(x==start) break;
                lv_area_t span={layer->buf_area.x1+(int32_t)start,layer->buf_area.y1+(int32_t)y,
                                layer->buf_area.x1+(int32_t)x-1,layer->buf_area.y1+(int32_t)y};
                blend.blend_area=&span;lv_draw_sw_blend(task,&blend);
            }
        }
    }
    /* ENGINE callers may invalidate/read the target immediately, and display
     * DMA must see these final CPU writes just as it sees GE-written pixels. */
    lv_area_t relative=layer->buf_area;
    lv_area_move(&relative,-task->target_layer->buf_area.x1,-task->target_layer->buf_area.y1);
    lv_draw_aic_ge2d_prepare_src_cache(task->target_layer->draw_buf,&relative);
}

static inline void lv_aic_ge2d_alpha_release(lv_draw_buf_t *surface)
{
    if(surface->data) aicos_free_align(MEM_CMA,surface->data);
    lv_memzero(surface,sizeof *surface);
}

#endif
