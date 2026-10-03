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

#if AIC_LVGL_USE_GE2D
/* Normalize YUV subsampling exactly where the SDK fill checker rounds down.
 * Only linear layouts: the GE tiled formats are source layouts, not outputs. */
static unsigned fill_layout(struct mpp_buf *buf,unsigned rows[3],unsigned pitch[3],unsigned *alignment)
{
    unsigned planes=1,bpp=1,sx=1,sy=1;
    if(buf->format>=MPP_FMT_ARGB_8888 && buf->format<=MPP_FMT_BGRA_4444) {
        bpp=buf->format<=MPP_FMT_BGRX_8888?4:buf->format<=MPP_FMT_BGR_888?3:2;
    }
    else {
        switch(buf->format) {
        case MPP_FMT_YUV420P: planes=3;sx=2;sy=2;break;
        case MPP_FMT_YUV422P: planes=3;sx=2;break;
        case MPP_FMT_YUV444P: planes=3;break;
        case MPP_FMT_NV12: case MPP_FMT_NV21: planes=2;sx=2;sy=2;break;
        case MPP_FMT_NV16: case MPP_FMT_NV61: planes=2;sx=2;break;
        case MPP_FMT_YUYV: case MPP_FMT_YVYU: case MPP_FMT_UYVY: case MPP_FMT_VYUY:
            bpp=2;sx=2;break;
        case MPP_FMT_YUV400: break;
        default: return 0;
        }
        if(!buf->crop_en) buf->crop=(struct mpp_rect){0,0,buf->size.width,buf->size.height};
        buf->size.width-=buf->size.width%sx;buf->size.height-=buf->size.height%sy;
        buf->crop.x-=buf->crop.x%sx;buf->crop.y-=buf->crop.y%sy;
        buf->crop.width-=buf->crop.width%sx;buf->crop.height-=buf->crop.height%sy;
        if(buf->crop.x>=buf->size.width || buf->crop.y>=buf->size.height) return 0;
        if(buf->crop.width>buf->size.width-buf->crop.x) buf->crop.width=buf->size.width-buf->crop.x;
        if(buf->crop.height>buf->size.height-buf->crop.y) buf->crop.height=buf->size.height-buf->crop.y;
        if(buf->crop.width<8 || buf->crop.height<8) return 0;
        if(planes==3 && buf->stride[1]!=buf->stride[2]) return 0;
    }
    *alignment=bpp==3?1:bpp;
    rows[0]=buf->size.height;pitch[0]=buf->size.width*bpp;
    for(unsigned i=1;i<planes;i++) {
        rows[i]=buf->size.height/sy;
        pitch[i]=buf->size.width/(planes==3?sx:1);
    }
    return planes;
}
#endif

int lv_ge_fill(struct mpp_buf *buf,enum ge_fillrect_type type,
               unsigned int start_color,unsigned int end_color,int blend)
{
#if AIC_LVGL_USE_GE2D
    if(!buf || dma_faulted() || buf->buf_type!=MPP_PHY_ADDR ||
       type<GE_NO_GRADIENT || type>GE_V_LINEAR_GRADIENT ||
       buf->size.width<1 || buf->size.width>4096 ||
       buf->size.height<1 || buf->size.height>4096 || buf->crop_en>1) return LV_RESULT_INVALID;
    if(buf->crop_en && (buf->crop.x<0 || buf->crop.y<0 ||
       buf->crop.width<1 || buf->crop.height<1 ||
       (int64_t)buf->crop.x+buf->crop.width>buf->size.width ||
       (int64_t)buf->crop.y+buf->crop.height>buf->size.height)) return LV_RESULT_INVALID;
    struct mpp_buf dst=*buf;
    unsigned rows[3]={0},pitch[3]={0},alignment;
    unsigned planes=fill_layout(&dst,rows,pitch,&alignment);
    if(!planes) return LV_RESULT_INVALID;
    allocation_t *owners[3]={0};
    uint64_t spans[3]={0};
    for(unsigned i=0;i<planes;i++) {
        uint32_t address=dst.phy_addr[i];
        uint64_t bytes=(uint64_t)dst.stride[i]*rows[i];
        /* GE DST/OUTPUT_STRIDE_SET masks each pitch to 16 bits. Reject
         * truncation before cache handoff or submission, including foreign
         * buffers whose large capacity would otherwise pass span checks. */
        if(!address || address%alignment || dst.stride[i]>UINT16_MAX || dst.stride[i]%alignment ||
           dst.stride[i]<pitch[i] || bytes>(uint64_t)UINT32_MAX-address+1) return LV_RESULT_INVALID;
#if defined(AIC_CHIP_D13X) || defined(AIC_CHIP_G73X)
        if(address<0x40000000U) return LV_RESULT_INVALID;
#endif
        for(unsigned j=0;j<i;j++) {
            if((uint64_t)address<(uint64_t)dst.phy_addr[j]+spans[j] &&
               (uint64_t)address+bytes>dst.phy_addr[j]) return LV_RESULT_INVALID;
        }
        spans[i]=bytes;
        for(allocation_t *a=allocations;a;a=a->next) {
            uint64_t begin=(uintptr_t)a->pixels,end=begin+a->bytes;
            if((uint64_t)address<end && (uint64_t)address+bytes>begin) {
                if(address<begin || (uint64_t)address+bytes>end) return LV_RESULT_INVALID;
                owners[i]=a;
                for(unsigned j=0;j<i;j++) if(owners[j]==a) owners[i]=NULL;
                break;
            }
        }
    }
    struct mpp_ge *ge=lv_draw_aic_ge2d_device();
    if(!ge) return LV_RESULT_INVALID;
    struct ge_fillrect fill={0};
    fill.type=type;fill.start_color=start_color;fill.end_color=end_color;
    fill.dst_buf=dst;
    fill.ctrl.alpha_en=blend!=0;fill.ctrl.alpha_rules=GE_PD_NONE;
    /* Preserve SDK per-pixel alpha and straight-alpha defaults. The hardware
     * helper exposes native GE behavior, not LVGL gradient pixel equivalence. */
    for(unsigned i=0;i<planes;i++) if(owners[i])
        aicos_dcache_clean_invalid_range((unsigned long *)owners[i]->pixels,owners[i]->bytes);
    if(mpp_ge_fillrect(ge,&fill)<0 || mpp_ge_emit(ge)<0 || mpp_ge_sync(ge)<0) {
        lv_draw_aic_ge2d_quarantine();return LV_RESULT_INVALID;
    }
    for(unsigned i=0;i<planes;i++) if(owners[i])
        aicos_dcache_invalid_range((unsigned long *)owners[i]->pixels,owners[i]->bytes);
    return LV_RESULT_OK;
#else
    LV_UNUSED(buf);LV_UNUSED(type);LV_UNUSED(start_color);LV_UNUSED(end_color);LV_UNUSED(blend);
    return LV_RESULT_INVALID;
#endif
}
#endif
