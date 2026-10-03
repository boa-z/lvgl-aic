/* SPDX-License-Identifier: Apache-2.0 */
#define AIC_LVGL_USE_PRIVATE_API 1
#include "lv_draw_aic_ge2d.h"
#if AIC_LVGL_USE_GE2D && AIC_LVGL_BSP_MPP
#include "lvgl_aic_private.h"
#include <aic_osal.h>
#include <aic_core.h>
#ifndef AIC_LVGL_GE_DRAW_BUF_BUDGET
#define AIC_LVGL_GE_DRAW_BUF_BUDGET (4U * 1024U * 1024U)
#endif
/* LVGL owner thread. Per-buffer handlers avoid mutating global handlers and
 * preserve correct frees across software fallback and subsequent lifetimes. */
typedef struct allocation {
    void *data;
    size_t bytes;
    struct allocation *next;
} allocation_t;
static allocation_t *allocations;
static size_t used;
static lv_draw_buf_handlers_t handlers;
static bool initialized;
extern lv_draw_buf_t *__real_lv_draw_buf_create(uint32_t w,uint32_t h,
    lv_color_format_t cf,uint32_t stride);
static void *allocate(size_t size,lv_color_format_t cf)
{
    LV_UNUSED(cf);
    if(lv_draw_aic_ge2d_faulted() || size>AIC_LVGL_GE_DRAW_BUF_BUDGET ||
       used>AIC_LVGL_GE_DRAW_BUF_BUDGET-size) return NULL;
    allocation_t *a=lv_malloc(sizeof(*a));
    if(!a) return NULL;
    size_t alignment=LV_DRAW_BUF_ALIGN>64?LV_DRAW_BUF_ALIGN:64;
    void *data=aicos_malloc_align(MEM_CMA,size,alignment);
    uintptr_t address=(uintptr_t)data;
    if(!data || address%alignment || address>UINT32_MAX ||
       size>(uint64_t)UINT32_MAX-address+1
#if defined(AIC_CHIP_D13X) || defined(AIC_CHIP_G73X)
       || address<0x40000000U
#endif
       ) {
        if(data) aicos_free_align(MEM_CMA,data);
        lv_free(a);return NULL;
    }
    a->data=data;a->bytes=size;a->next=allocations;allocations=a;used+=size;
    return data;
}
static void release(void *data)
{
    allocation_t **p=&allocations;
    while(*p && (*p)->data!=data) p=&(*p)->next;
    if(!*p || lv_draw_aic_ge2d_faulted()) return;
    allocation_t *a=*p;*p=a->next;used-=a->bytes;
    aicos_free_align(MEM_CMA,a->data);lv_free(a);
}
static void *align_data(void *data,lv_color_format_t cf) { LV_UNUSED(cf);return data; }
/* Only default creates are redirected. Fonts, decoder-specific handlers and
 * caller-supplied framebuffer storage keep their original allocation policy.
 * No DMA is started here. CMA exhaustion safely uses the original allocator. */
lv_draw_buf_t *__wrap_lv_draw_buf_create(uint32_t w,uint32_t h,lv_color_format_t cf,uint32_t stride)
{
    if(lv_draw_aic_ge2d_faulted()) return NULL;
    if(!initialized) {
        lv_draw_buf_init_with_default_handlers(&handlers);
        handlers.buf_malloc_cb=allocate;handlers.buf_free_cb=release;
        handlers.align_pointer_cb=align_data;
        initialized=true;
    }
    if(w && h && w<=4096 && h<=4096 &&
       (cf==LV_COLOR_FORMAT_RGB565 || cf==LV_COLOR_FORMAT_RGB888 ||
        cf==LV_COLOR_FORMAT_XRGB8888 || cf==LV_COLOR_FORMAT_ARGB8888)) {
        uint32_t pitch=stride?stride:lv_draw_buf_width_to_stride(w,cf);
        if(pitch>=(uint64_t)w*lv_color_format_get_size(cf) &&
           (uint64_t)pitch*h+LV_DRAW_BUF_ALIGN<=AIC_LVGL_GE_DRAW_BUF_BUDGET) {
            lv_draw_buf_t *buf=lv_draw_buf_create_ex(&handlers,w,h,cf,pitch);
            if(buf) return buf;
        }
    }
    return __real_lv_draw_buf_create(w,h,cf,stride);
}
#endif
