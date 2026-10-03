/* SPDX-License-Identifier: Apache-2.0 */
#include "lvgl_aic_feature_config.h"
#if AIC_LVGL_USE_SPI_SDK && AIC_LVGL_USE_GE2D && AIC_LVGL_BSP_MPP
#include "lv_aic_spi_ge2d.h"
#include "lv_draw_aic_ge2d_scale.h"
#include "lvgl.h"
#include <aic_osal.h>
#include <mpp_ge.h>
#include <string.h>
struct lv_aic_spi_ge2d {
    struct mpp_ge *ge;
    uint8_t *src,*dst;
    size_t src_bytes,dst_bytes,src_stride,dst_stride;
    uint32_t max_width,max_height,width,height;
    bool busy,fault;
};
static bool dma_valid(const void *p,size_t bytes)
{
    uintptr_t a=(uintptr_t)p;
    if(!p || (a&63) || a>UINT32_MAX || bytes>(uint64_t)UINT32_MAX+1-a) return false;
#if defined(AIC_CHIP_D13X) || defined(AIC_CHIP_G73X)
    if(a<0x40000000U) return false;
#endif
    return true;
}
static bool overlaps(uintptr_t a,size_t as,uintptr_t b,size_t bs)
{ return (uint64_t)a<(uint64_t)b+bs && (uint64_t)b<(uint64_t)a+as; }
lv_aic_spi_ge2d_t *lv_aic_spi_ge2d_create(uint32_t mw,uint32_t mh,
    uint32_t w,uint32_t h,size_t budget)
{
    if(!mw || !mh || !w || !h || mw>4096 || mh>4096 || w>4096 || h>4096) return NULL;
    size_t ss=((size_t)mw*2+63)&~(size_t)63,ds=((size_t)w*2+63)&~(size_t)63;
    size_t sb=ss*mh,db=ds*h;
    if(sb+db>budget) return NULL;
    lv_aic_spi_ge2d_t *s=lv_malloc_zeroed(sizeof(*s));
    if(!s) return NULL;
    s->src=aicos_malloc_align(MEM_CMA,sb,64);
    s->dst=aicos_malloc_align(MEM_CMA,db,64);
    if(!dma_valid(s->src,sb) || !dma_valid(s->dst,db) ||
       overlaps((uintptr_t)s->src,sb,(uintptr_t)s->dst,db)) goto fail;
    s->ge=mpp_ge_open();
    if(!s->ge || mpp_ge_get_mode(s->ge)!=GE_MODE_CMDQ) goto fail;
    s->src_bytes=sb;s->dst_bytes=db;s->src_stride=ss;s->dst_stride=ds;
    s->max_width=mw;s->max_height=mh;s->width=w;s->height=h;
    return s;
fail:
    if(s->ge) mpp_ge_close(s->ge);
    if(s->dst) aicos_free_align(MEM_CMA,s->dst);
    if(s->src) aicos_free_align(MEM_CMA,s->src);
    lv_free(s);return NULL;
}
lv_aic_spi_result_t lv_aic_spi_ge2d_convert(lv_aic_spi_ge2d_t *s,
    const lv_aic_spi_rgb565_frame_t *source,uint8_t *output,size_t capacity,
    unsigned degrees,bool swap)
{
    if(!s) return LV_AIC_SPI_INVALID;
    if(s->busy) return LV_AIC_SPI_BUSY;
    if(s->fault) return LV_AIC_SPI_FAULT;
    if(!source || !output) return LV_AIC_SPI_INVALID;
    lv_aic_spi_rgb565_frame_t f=*source;
    if(!f.data || !f.width || !f.height || f.width>s->max_width || f.height>s->max_height ||
       f.stride<(size_t)f.width*2 || (degrees!=0 && degrees!=90 && degrees!=180 && degrees!=270))
        return LV_AIC_SPI_INVALID;
    bool rotated=degrees==90 || degrees==270;
    uint32_t rw=rotated?f.height:f.width,rh=rotated?f.width:f.height;
    /* SDK RGB scaler requires all input/output axes >= 4. Reject before any
     * commands: SDK may already have queued registers when it discovers this. */
    if((rw!=s->width || rh!=s->height) &&
       (f.width<4 || f.height<4 || s->width<4 || s->height<4)) return LV_AIC_SPI_INVALID;
    /* Match the draw backend's bounded scale and vendor split-risk policy.
     * The scaler runs before rotation: its horizontal destination is swapped. */
    uint32_t ow=rotated?s->height:s->width,oh=rotated?s->width:s->height;
    if(ow>f.width*16 || oh>f.height*16 || f.width>ow*16 || f.height>oh*16 ||
       lv_aic_ge2d_scale_split_risk((int32_t)((f.width*65536U)/ow),(int32_t)ow))
        return LV_AIC_SPI_INVALID;
    size_t row=(size_t)f.width*2,bytes=(size_t)s->width*s->height*2;
    if(f.height>1 && f.stride>(SIZE_MAX-row)/(f.height-1)) return LV_AIC_SPI_INVALID;
    size_t span=f.stride*(f.height-1)+row;
    uintptr_t in=(uintptr_t)f.data,out=(uintptr_t)output;
    if(span>f.capacity || bytes>capacity || span>UINTPTR_MAX-in || bytes>UINTPTR_MAX-out ||
       overlaps(in,span,out,bytes) ||
       overlaps(in,span,(uintptr_t)s->src,s->src_bytes) ||
       overlaps(in,span,(uintptr_t)s->dst,s->dst_bytes) ||
       overlaps(out,bytes,(uintptr_t)s->src,s->src_bytes) ||
       overlaps(out,bytes,(uintptr_t)s->dst,s->dst_bytes)) return LV_AIC_SPI_INVALID;
    s->busy=true;
    /* Zero row padding too: GE filtering must never read stale staging pixels. */
    memset(s->src,0,s->src_bytes);
    for(uint32_t y=0;y<f.height;y++) memcpy(s->src+y*s->src_stride,f.data+y*f.stride,row);
    aicos_dcache_clean_range((unsigned long *)s->src,(unsigned long)s->src_bytes);
    aicos_dcache_clean_invalid_range((unsigned long *)s->dst,(unsigned long)s->dst_bytes);
    struct ge_bitblt blt={0};
    blt.ctrl.flags=degrees==90?MPP_ROTATION_90:degrees==180?MPP_ROTATION_180:
                   degrees==270?MPP_ROTATION_270:0;
    blt.src_buf.buf_type=blt.dst_buf.buf_type=MPP_PHY_ADDR;
    blt.src_buf.phy_addr[0]=(uint32_t)(uintptr_t)s->src;
    blt.dst_buf.phy_addr[0]=(uint32_t)(uintptr_t)s->dst;
    blt.src_buf.stride[0]=(int)s->src_stride;blt.dst_buf.stride[0]=(int)s->dst_stride;
    blt.src_buf.size.width=(int)f.width;blt.src_buf.size.height=(int)f.height;
    blt.dst_buf.size.width=(int)s->width;blt.dst_buf.size.height=(int)s->height;
    blt.src_buf.format=blt.dst_buf.format=MPP_FMT_RGB_565;
    if(mpp_ge_bitblt(s->ge,&blt)<0 || mpp_ge_emit(s->ge)<0 || mpp_ge_sync(s->ge)<0) {
        s->fault=true;s->busy=false;return LV_AIC_SPI_FAULT;
    }
    aicos_dcache_invalid_range((unsigned long *)s->dst,(unsigned long)s->dst_bytes);
    for(uint32_t y=0;y<s->height;y++) for(uint32_t x=0;x<s->width;x++) {
        const uint8_t *p=s->dst+y*s->dst_stride+x*2;
        size_t offset=((size_t)y*s->width+x)*2;
        output[offset]=p[swap?1:0];output[offset+1]=p[swap?0:1];
    }
    s->busy=false;return LV_AIC_SPI_OK;
}
lv_aic_spi_result_t lv_aic_spi_ge2d_close(lv_aic_spi_ge2d_t *s)
{
    if(!s) return LV_AIC_SPI_INVALID;
    if(s->busy) return LV_AIC_SPI_BUSY;
    if(s->fault) return LV_AIC_SPI_FAULT;
    mpp_ge_close(s->ge);
    aicos_free_align(MEM_CMA,s->dst);aicos_free_align(MEM_CMA,s->src);
    lv_free(s);return LV_AIC_SPI_OK;
}
#endif
