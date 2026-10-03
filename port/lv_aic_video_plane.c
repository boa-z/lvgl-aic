/* SPDX-License-Identifier: Apache-2.0 */
#include "lvgl_aic_feature_config.h"
#if defined(AIC_LVGL_USE_VIDEO_PLANE) && AIC_LVGL_USE_VIDEO_PLANE
#include "lv_aic_video_plane.h"
#include "lv_aic_rgb_image_private.h"
#include "lv_aic_yuv_image_private.h"
#include "lv_aic_yuv_mpp.h"
#include "lv_aic_pixel_format.h"
#include <mpp_fb.h>
#if defined(AIC_LVGL_USE_GE2D) && AIC_LVGL_USE_GE2D
#include <mpp_ge.h>
#endif
#include <aic_osal.h>
typedef struct {
    lv_aic_rgb_image_t *rgb;lv_aic_yuv_image_t *yuv;
    void *copy;size_t bytes;
} reader_t;
struct lv_aic_video_plane {
    struct mpp_fb *fb;
    struct mpp_ge *ge;
    bool ge_fault;
    struct aicfb_screeninfo screen;
    reader_t current,pending;
    struct aicfb_alpha_config saved_alpha;
    bool alpha_saved,alpha_fault;
    bool faulted,submitted;
};
static lv_aic_video_plane_t *owner;
static void release(reader_t *r)
{
    if(r->rgb) lv_aic_rgb_image_release_lease(r->rgb);
    if(r->yuv) lv_aic_yuv_image_release_lease(r->yuv);
    if(r->copy) aicos_free_align(MEM_CMA,r->copy);
    *r=(reader_t){0};
}
static bool sync_scanout(lv_aic_video_plane_t *p)
{
    /* The second edge also covers a previously signalled SDK wait queue and
     * a submission at the current frame boundary. No release on either error. */
    return mpp_fb_ioctl(p->fb,AICFB_WAIT_FOR_VSYNC,NULL)>=0 &&
           mpp_fb_ioctl(p->fb,AICFB_WAIT_FOR_VSYNC,NULL)>=0;
}
lv_aic_video_plane_t *lv_aic_video_plane_open(void)
{
    if(owner) return NULL;
    lv_aic_video_plane_t *p=lv_malloc_zeroed(sizeof(*p));if(!p) return NULL;
    p->fb=mpp_fb_open();
    struct aicfb_layer_data layer={.layer_id=AICFB_LAYER_TYPE_VIDEO};
    if(!p->fb || mpp_fb_ioctl(p->fb,AICFB_GET_SCREENINFO,&p->screen)<0 ||
       !p->screen.width || !p->screen.height ||
       mpp_fb_ioctl(p->fb,AICFB_GET_LAYER_CONFIG,&layer)<0 || layer.enable) {
        if(p->fb) mpp_fb_close(p->fb);
        lv_free(p);return NULL;
    }
    owner=p;return p;
}
bool lv_aic_video_plane_enable_ui_alpha(lv_aic_video_plane_t *p)
{
    if(!p || p!=owner || p->faulted || p->alpha_fault || p->ge_fault) return false;
    if(p->alpha_saved) return true;
    if(p->submitted || p->screen.format!=MPP_FMT_ARGB_8888) return false;
    struct aicfb_alpha_config old={.layer_id=AICFB_LAYER_TYPE_UI};
    if(mpp_fb_ioctl(p->fb,AICFB_GET_ALPHA_CONFIG,&old)<0 || old.layer_id!=AICFB_LAYER_TYPE_UI ||
       old.enable>1 || old.mode>AICFB_MIXDER_ALPHA_MODE || old.value>255) return false;
    /* Save BEFORE a potentially partial SDK update. A failed apply still owns
     * restoration and must keep the session until close succeeds. */
    p->saved_alpha=old;p->alpha_saved=true;
    struct aicfb_alpha_config pixel={.layer_id=AICFB_LAYER_TYPE_UI,.enable=1,
        .mode=AICFB_PIXEL_ALPHA_MODE,.value=255};
    if(mpp_fb_ioctl(p->fb,AICFB_UPDATE_ALPHA_CONFIG,&pixel)<0 || !sync_scanout(p)) {
        p->alpha_fault=true;return false;
    }
    return true;
}
static bool acquire(const void *source,reader_t *reader,struct mpp_buf *buf)
{
    const lv_aic_rgb_frame_t *rgb=NULL;const lv_aic_yuv_frame_t *yuv=NULL;
    uint32_t floor=0;
#if defined(AIC_CHIP_D13X) || defined(AIC_CHIP_G73X)
    floor=0x40000000;
#endif
    reader->rgb=lv_aic_rgb_image_acquire(source,&rgb);
    if(rgb) {
        uintptr_t address=(uintptr_t)rgb->data;
        uint64_t span=(uint64_t)rgb->stride*rgb->height;
        if(address<floor || address>UINT32_MAX || span>rgb->capacity ||
           span>(uint64_t)UINT32_MAX-address+1 || rgb->stride>UINT16_MAX ||
           !lv_aic_pixel_format_to_mpp(rgb->format,&buf->format)) goto bad;
        buf->buf_type=MPP_PHY_ADDR;buf->phy_addr[0]=(uint32_t)address;
        buf->stride[0]=rgb->stride;buf->size.width=rgb->width;buf->size.height=rgb->height;
        aicos_dcache_clean_invalid_range((unsigned long *)rgb->data,(unsigned long)span);
        return true;
    }
    reader->yuv=lv_aic_yuv_image_acquire(source,&yuv);
    if(yuv && lv_aic_yuv_to_mpp(yuv,floor,buf)) {
        lv_aic_yuv_layout_t layout;
        if(!lv_aic_yuv_layout(yuv->format,yuv->width,yuv->height,&layout)) goto bad;
        for(unsigned i=0;i<layout.planes;i++)
            aicos_dcache_clean_invalid_range((unsigned long *)yuv->planes[i].data,
                (unsigned long)yuv->planes[i].stride*layout.rows[i]);
        return true;
    }
bad:
    release(reader);return false;
}
#if defined(AIC_LVGL_USE_GE2D) && AIC_LVGL_USE_GE2D
static bool rotate_copy(lv_aic_video_plane_t *p,struct mpp_buf *buf,unsigned degrees,size_t budget)
{
    /* Keep the existing GE YUV minimum geometry guard. DE scaling happens
     * afterward; GE only rotates/converts the complete native frame. */
    if(buf->size.width<8 || buf->size.height<8) return false;
    bool swap=degrees!=180;
    unsigned w=swap?buf->size.height:buf->size.width;
    unsigned h=swap?buf->size.width:buf->size.height;
    size_t stride=(w*4U+63U)&~(size_t)63;
    size_t bytes=stride*h;
    if(bytes>budget || p->current.bytes>budget-bytes) return false;
    if(!p->ge) p->ge=mpp_ge_open();
    if(!p->ge) return false;
    p->pending.copy=aicos_malloc_align(MEM_CMA,bytes,64);
    if(!p->pending.copy) return false;
    p->pending.bytes=bytes;
    uintptr_t address=(uintptr_t)p->pending.copy;
    uint32_t floor=0;
#if defined(AIC_CHIP_D13X) || defined(AIC_CHIP_G73X)
    floor=0x40000000;
#endif
    if(address<floor || address>UINT32_MAX || bytes>(uint64_t)UINT32_MAX-address+1) return false;
    for(unsigned i=0;i<3;i++) {
        if(!buf->phy_addr[i] || !buf->stride[i]) continue;
        unsigned rows=buf->size.height;
        if(i && (buf->format==MPP_FMT_YUV420P || buf->format==MPP_FMT_NV12 || buf->format==MPP_FMT_NV21)) rows/=2;
        uint64_t end=(uint64_t)buf->phy_addr[i]+(uint64_t)buf->stride[i]*rows;
        if((uint64_t)address<end && (uint64_t)buf->phy_addr[i]<(uint64_t)address+bytes) return false;
    }
    struct ge_bitblt blt={0};blt.src_buf=*buf;
    blt.dst_buf=(struct mpp_buf){.buf_type=MPP_PHY_ADDR,.format=MPP_FMT_ARGB_8888,
        .size={w,h},.phy_addr={(uint32_t)address},.stride={stride}};
    blt.ctrl.flags=degrees==90?MPP_ROTATION_90:degrees==180?MPP_ROTATION_180:MPP_ROTATION_270;
    blt.ctrl.src_global_alpha=255;
    aicos_dcache_clean_invalid_range(p->pending.copy,bytes);
    if(mpp_ge_bitblt(p->ge,&blt)<0 || mpp_ge_emit(p->ge)<0 || mpp_ge_sync(p->ge)<0) {
        p->ge_fault=true;return false;
    }
    /* Successful GE completion retires only the source lease. The destination
     * belongs to scanout until a subsequent verified replacement/disable. */
    if(p->pending.rgb) lv_aic_rgb_image_release_lease(p->pending.rgb);
    if(p->pending.yuv) lv_aic_yuv_image_release_lease(p->pending.yuv);
    p->pending.rgb=NULL;p->pending.yuv=NULL;
    *buf=blt.dst_buf;return true;
}
#endif
bool lv_aic_video_plane_present_rotated(lv_aic_video_plane_t *p,const void *source,
    int32_t x,int32_t y,uint32_t width,uint32_t height,unsigned degrees,size_t cma_budget)
{
    if(!p || p!=owner || p->faulted || p->alpha_fault || p->ge_fault || !source ||
       (degrees!=0 && degrees!=90 && degrees!=180 && degrees!=270) ||
       x<0 || y<0 || !width || !height ||
       (uint64_t)x+width>p->screen.width || (uint64_t)y+height>p->screen.height) return false;
    struct aicfb_layer_data layer={.enable=1,.layer_id=AICFB_LAYER_TYPE_VIDEO,
        .pos={x,y},.scale_size={width,height}};
    if(!acquire(source,&p->pending,&layer.buf)) return false;
    if(degrees) {
#if defined(AIC_LVGL_USE_GE2D) && AIC_LVGL_USE_GE2D
        if(!rotate_copy(p,&layer.buf,degrees,cma_budget)) {
            if(!p->ge_fault) release(&p->pending);
            return false;
        }
#else
        (void)cma_budget;release(&p->pending);return false;
#endif
    }
    p->submitted=true;
    if(mpp_fb_ioctl(p->fb,AICFB_UPDATE_LAYER_CONFIG,&layer)<0 || !sync_scanout(p)) {
        p->faulted=true;return false;
    }
    release(&p->current);p->current=p->pending;p->pending=(reader_t){0};return true;
}
bool lv_aic_video_plane_present(lv_aic_video_plane_t *p,const void *source,
                                int32_t x,int32_t y,uint32_t width,uint32_t height)
{ return lv_aic_video_plane_present_rotated(p,source,x,y,width,height,0,0); }
bool lv_aic_video_plane_faulted(const lv_aic_video_plane_t *p)
{ return p && p==owner && (p->faulted || p->alpha_fault || p->ge_fault); }
bool lv_aic_video_plane_hide(lv_aic_video_plane_t *p)
{
    if(!p || p!=owner || p->ge_fault) return false;
    if(!p->submitted) return true;
    struct aicfb_layer_data layer={.layer_id=AICFB_LAYER_TYPE_VIDEO};
    if(mpp_fb_ioctl(p->fb,AICFB_UPDATE_LAYER_CONFIG,&layer)<0 || !sync_scanout(p)) {
        p->faulted=true;return false;
    }
    release(&p->current);release(&p->pending);p->submitted=p->faulted=false;return true;
}
bool lv_aic_video_plane_close(lv_aic_video_plane_t *p)
{
    if(!lv_aic_video_plane_hide(p)) return false;
    if(p->alpha_saved) {
        if(mpp_fb_ioctl(p->fb,AICFB_UPDATE_ALPHA_CONFIG,&p->saved_alpha)<0 || !sync_scanout(p)) {
            p->alpha_fault=true;return false;
        }
        p->alpha_saved=p->alpha_fault=false;
    }
#if defined(AIC_LVGL_USE_GE2D) && AIC_LVGL_USE_GE2D
    if(p->ge) mpp_ge_close(p->ge);
#endif
    mpp_fb_close(p->fb);owner=NULL;lv_free(p);return true;
}
#endif
