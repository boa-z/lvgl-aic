/* SPDX-License-Identifier: Apache-2.0 */
#include "lv_aic_canvas.h"
#define AIC_LVGL_USE_PRIVATE_API 1
#include "lvgl_aic_private.h"
#if AIC_LVGL_USE_CANVAS
#include <string.h>
#include <limits.h>
#if AIC_LVGL_BSP_MPP
#include <aic_osal.h>
#include <aic_core.h>
#endif
#if AIC_LVGL_USE_GE2D && AIC_LVGL_BSP_MPP
#include "lv_draw_aic_ge2d.h"
#endif

typedef struct {
    lv_canvas_t canvas;
    lv_draw_buf_t buffer;
    void *pixels;
    uint32_t bytes, budget;
} aic_canvas_t;
static bool faulted(void)
{
#if AIC_LVGL_USE_GE2D && AIC_LVGL_BSP_MPP
    return lv_draw_aic_ge2d_faulted();
#else
    return false;
#endif
}
static void *allocate(uint32_t bytes)
{
#if AIC_LVGL_BSP_MPP
    return aicos_malloc_align(MEM_CMA, bytes, 64);
#else
    return lv_malloc(bytes);
#endif
}
static void release(void *pixels)
{
    if (!pixels) return;
#if AIC_LVGL_BSP_MPP
    aicos_free_align(MEM_CMA, pixels);
#else
    lv_free(pixels);
#endif
}
static void flush(aic_canvas_t *c)
{
#if AIC_LVGL_BSP_MPP
    aicos_dcache_clean_invalid_range((unsigned long *)c->pixels, c->bytes);
#else
    LV_UNUSED(c);
#endif
}
static void constructor(const lv_obj_class_t *cls, lv_obj_t *obj)
{ LV_UNUSED(cls); ((aic_canvas_t *)obj)->budget=4U*1024U*1024U; }
static void destructor(const lv_obj_class_t *cls, lv_obj_t *obj)
{
    LV_UNUSED(cls);
    aic_canvas_t *c=(aic_canvas_t *)obj;
    lv_image_cache_drop(&c->buffer);
    c->canvas.draw_buf=NULL;
    /* Uncertain DMA may still read these pixels. Full LVGL teardown during
     * a fault remains unsupported; never recycle the allocation here. */
    if (!faulted()) release(c->pixels);
}
const lv_obj_class_t lv_aic_canvas_class={
    .constructor_cb=constructor, .destructor_cb=destructor,
    .base_class=&lv_canvas_class, .instance_size=sizeof(aic_canvas_t),
    .name="aic_canvas",
};
lv_obj_t *lv_aic_canvas_create(lv_obj_t *parent)
{
    lv_obj_t *obj=lv_obj_class_create_obj(&lv_aic_canvas_class,parent);
    if(obj) lv_obj_class_init_obj(obj);
    return obj;
}
bool lv_aic_canvas_set_budget(lv_obj_t *obj,uint32_t bytes)
{
    LV_CHECK_OBJ(obj,&lv_aic_canvas_class,return false);
    aic_canvas_t *c=(aic_canvas_t *)obj;
    if(!bytes || bytes<c->bytes || faulted()) return false;
    c->budget=bytes; return true;
}
lv_result_t lv_aic_canvas_alloc_buffer(lv_obj_t *obj,int32_t w,int32_t h)
{
    LV_CHECK_OBJ(obj,&lv_aic_canvas_class,return LV_RESULT_INVALID);
    aic_canvas_t *c=(aic_canvas_t *)obj;
    if(w<1 || h<1 || w>4096 || h>4096 || faulted()) return LV_RESULT_INVALID;
    uint32_t stride=((uint32_t)w*4U+63U)&~63U;
    uint64_t bytes=(uint64_t)stride*h;
    if(bytes+c->bytes>c->budget) return LV_RESULT_INVALID;
    void *pixels=allocate((uint32_t)bytes);
    if(!pixels) return LV_RESULT_INVALID;
    lv_draw_buf_t buffer;
    if(lv_draw_buf_init(&buffer,w,h,LV_COLOR_FORMAT_ARGB8888,stride,pixels,(uint32_t)bytes)!=LV_RESULT_OK) {
        release(pixels); return LV_RESULT_INVALID;
    }
    memset(pixels,0,(size_t)bytes);
    void *old=c->pixels;
    lv_image_cache_drop(&c->buffer);
    c->buffer=buffer; c->pixels=pixels; c->bytes=(uint32_t)bytes;
    flush(c);
    lv_canvas_set_draw_buf(obj,&c->buffer);
    release(old);
    return LV_RESULT_OK;
}
void lv_aic_canvas_draw_text(lv_obj_t *obj,int32_t x,int32_t y,int32_t max_w,
                             lv_draw_label_dsc_t *dsc,const char *text)
{
    LV_CHECK_OBJ(obj,&lv_aic_canvas_class,return);
    aic_canvas_t *c=(aic_canvas_t *)obj;
    if(!c->pixels || !dsc || !dsc->font || !text || max_w<1 || max_w>4096 ||
       x < -4096 || x > 4096 || y < -4096 || y > 4096 || faulted()) return;
    lv_draw_label_dsc_t label=*dsc;
    label.text=text; label.text_local=0;
    lv_point_t size;
    lv_text_get_size(&size,text,label.font,label.letter_space,label.line_space,max_w,label.flag);
    if(size.y<=0 || (int64_t)y+size.y-1>INT32_MAX) return;
    lv_area_t area={x,y,x+max_w-1,y+size.y-1};
    lv_layer_t layer;
    lv_canvas_init_layer(obj,&layer);
    lv_draw_label(&layer,&label,&area);
    lv_canvas_finish_layer(obj,&layer);
    flush(c);
}
void lv_aic_canvas_draw_text_to_center(lv_obj_t *obj,lv_draw_label_dsc_t *dsc,const char *text)
{
    LV_CHECK_OBJ(obj,&lv_aic_canvas_class,return);
    aic_canvas_t *c=(aic_canvas_t *)obj;
    if(!c->pixels || !dsc || !dsc->font || !text || faulted()) return;
    lv_point_t size;
    /* SDK center helper clears first and measures unwrapped text at zero
     * spacing. Preserve that behavior without relying on its binary ABI. */
    lv_text_get_size(&size,text,dsc->font,0,0,LV_COORD_MAX,LV_TEXT_FLAG_NONE);
    if(size.x>4096 || size.y>8192) return;
    memset(c->pixels,0,c->bytes);
    flush(c); lv_obj_invalidate(obj);
    if(size.x<=0) return;
    lv_aic_canvas_draw_text(obj,((int32_t)c->buffer.header.w-size.x)/2,
        ((int32_t)c->buffer.header.h-size.y)/2,size.x,dsc,text);
}
#endif
