/* SPDX-License-Identifier: Apache-2.0 */
#define AIC_LVGL_USE_PRIVATE_API 1
#include "canvas_image.h"
#if AIC_LVGL_USE_CANVAS && AIC_LVGL_BSP_MPP
#include <aic_osal.h>
#include <aic_core.h>
#if AIC_LVGL_USE_GE2D
#include "lv_draw_aic_ge2d.h"
#endif
/* Keep ownership independent of public descriptor edits. */
typedef struct allocation {
    struct lv_mpp_buf image;
    void *pixels;
    uint32_t bytes;
    struct allocation *next;
} allocation_t;
static allocation_t *allocations;
static bool dma_faulted(void)
{
#if AIC_LVGL_USE_GE2D
    return lv_draw_aic_ge2d_faulted();
#else
    return false;
#endif
}
static allocation_t **find_image(struct lv_mpp_buf *image)
{
    allocation_t **slot=&allocations;
    while(*slot && &(*slot)->image!=image) slot=&(*slot)->next;
    return slot;
}
struct lv_mpp_buf *lv_aic_mpp_image_alloc_bounded(int width,int height,
    enum mpp_pixel_format fmt,uint32_t budget)
{
    unsigned bpp=fmt==MPP_FMT_ARGB_8888?4:fmt==MPP_FMT_RGB_565?2:0;
    if(width<1 || height<1 || width>4096 || height>4096 || !bpp || dma_faulted()) return NULL;
    uint32_t stride=((uint32_t)width*bpp+63U)&~63U;
    uint64_t bytes=(uint64_t)stride*height;
    if(bytes>budget) return NULL;
    allocation_t *a=lv_malloc_zeroed(sizeof(*a));
    if(!a) return NULL;
    void *pixels=aicos_malloc_align(MEM_CMA,(size_t)bytes,64);
    uintptr_t address=(uintptr_t)pixels;
    if(!pixels || (address&63U) || address>UINT32_MAX || bytes>(uint64_t)UINT32_MAX-address+1) {
        if(pixels) aicos_free_align(MEM_CMA,pixels);
        lv_free(a); return NULL;
    }
    a->pixels=pixels; a->bytes=(uint32_t)bytes;
    a->image.data=pixels; a->image.size=(int)bytes;
    a->image.buf.buf_type=MPP_PHY_ADDR; a->image.buf.format=fmt;
    a->image.buf.size.width=width; a->image.buf.size.height=height;
    a->image.buf.stride[0]=stride; a->image.buf.phy_addr[0]=(uint32_t)address;
    aicos_dcache_clean_invalid_range((unsigned long *)pixels,(unsigned long)bytes);
    a->next=allocations; allocations=a;
    return &a->image;
}
struct lv_mpp_buf *lv_mpp_image_alloc(int width,int height,enum mpp_pixel_format fmt)
{
    return lv_aic_mpp_image_alloc_bounded(width,height,fmt,4U*1024U*1024U);
}
void lv_mpp_image_flush_cache(struct lv_mpp_buf *image)
{
    allocation_t *a=*find_image(image);
    if(!a || dma_faulted()) return;
    aicos_dcache_clean_invalid_range((unsigned long *)a->pixels,a->bytes);
}
void lv_mpp_image_free(struct lv_mpp_buf *image)
{
    allocation_t **slot=find_image(image),*a=*slot;
    if(!a || dma_faulted()) return;
    *slot=a->next;
    aicos_free_align(MEM_CMA,a->pixels);
    lv_free(a);
}
#endif
