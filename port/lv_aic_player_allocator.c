/* SPDX-License-Identifier: Apache-2.0 */
#include "lvgl_aic_feature_config.h"
#if (defined(AIC_LVGL_USE_PLAYER_SESSION) && AIC_LVGL_USE_PLAYER_SESSION) || \
    (defined(AIC_LVGL_USE_APNG) && AIC_LVGL_USE_APNG)
#include "lv_aic_player_allocator.h"
#include <aic_osal.h>
#include <stdlib.h>
#include <string.h>
#define FRAME_COUNT 32
#define LINE 32U
struct allocation {
    struct mpp_buf buf;
    size_t capacity[3];
    uint64_t ticket;
    bool live, retired;
};
struct lv_aic_player_allocator {
    struct frame_allocator base;
    aicos_mutex_t mutex;
    size_t budget, used;
    uint64_t next_ticket;
    struct allocation frames[FRAME_COUNT];
};
static void lock(lv_aic_player_allocator_t *a) { aicos_mutex_take(a->mutex,AICOS_WAIT_FOREVER); }
static void unlock(lv_aic_player_allocator_t *a) { aicos_mutex_give(a->mutex); }
static void dispose(lv_aic_player_allocator_t *a, struct allocation *r)
{
    for(unsigned i=0;i<3;i++) if(r->capacity[i]) {
        aicos_free_align(MEM_CMA,(void *)(uintptr_t)r->buf.phy_addr[i]);
        a->used-=r->capacity[i];
    }
    memset(r,0,sizeof(*r));
}
/* SDK width argument is byte stride, not visible pixel width. Match the
 * decoder's add_dmabuf pitch rules; refuse formats that it cannot register. */
static bool layout(struct allocation *r,int stride,int height,enum mpp_pixel_format format)
{
    int width=r->buf.size.width;
    unsigned planes=1, bytes=1;
    bool half_x=false,half_y=false;
    if(width<1 || width>4096 || height<1 || height>4096 || stride<1 ||
       (uint64_t)width*height>8U*1024U*1024U) return false;
    switch(format) {
    case MPP_FMT_YUV420P: planes=3; half_x=true; half_y=true; break;
    case MPP_FMT_YUV422P: planes=3; half_x=true; break;
    case MPP_FMT_YUV444P: planes=3; break;
    case MPP_FMT_NV12: case MPP_FMT_NV21: planes=2; half_x=true; half_y=true; break;
    case MPP_FMT_YUV400: break;
    case MPP_FMT_RGB_565: case MPP_FMT_BGR_565: bytes=2; break;
    case MPP_FMT_RGB_888: case MPP_FMT_BGR_888: bytes=3; break;
    case MPP_FMT_ARGB_8888: case MPP_FMT_ABGR_8888:
    case MPP_FMT_RGBA_8888: case MPP_FMT_BGRA_8888: bytes=4; break;
    default: return false;
    }
    if((unsigned)stride<(unsigned)width*bytes || (half_x && (stride&1)) ||
       (half_y && (height&1))) return false;
    r->buf.buf_type=MPP_PHY_ADDR; r->buf.format=format; r->buf.size.height=height;
    for(unsigned i=0;i<planes;i++) {
        unsigned pitch=(i && planes==3 && half_x)?(unsigned)stride/2:(unsigned)stride;
        unsigned rows=i && half_y?(unsigned)height/2:(unsigned)height;
        uint64_t size=(uint64_t)pitch*rows;
        if(size>UINT32_MAX-(LINE-1)) return false;
        r->buf.stride[i]=pitch;
        r->capacity[i]=((size_t)size+LINE-1)&~(size_t)(LINE-1);
    }
    return true;
}
static int allocate(struct frame_allocator *base,struct mpp_frame *frame,
                    int stride,int height,enum mpp_pixel_format format)
{
    lv_aic_player_allocator_t *a=(void *)base;
    struct allocation next={0};
    if(!frame) return -1;
    next.buf.size.width=frame->buf.size.width;
    if(!layout(&next,stride,height,format)) return -1;
    lock(a);
    unsigned slot=0;
    while(slot<FRAME_COUNT && a->frames[slot].live) slot++;
    uint64_t bytes=(uint64_t)next.capacity[0]+next.capacity[1]+next.capacity[2];
    if(slot==FRAME_COUNT || bytes>a->budget-a->used) { unlock(a); return -1; }
    for(unsigned i=0;i<3;i++) if(next.capacity[i]) {
        void *data=aicos_malloc_align(MEM_CMA,next.capacity[i],LINE);
        uintptr_t address=(uintptr_t)data;
        if(!data || (address&(LINE-1)) || address>UINT32_MAX ||
           next.capacity[i]>(uint64_t)UINT32_MAX-address+1) {
            if(data) aicos_free_align(MEM_CMA,data);
            for(unsigned j=0;j<i;j++) if(next.capacity[j])
                aicos_free_align(MEM_CMA,(void *)(uintptr_t)next.buf.phy_addr[j]);
            unlock(a); return -1;
        }
        next.buf.phy_addr[i]=(uint32_t)address;
        /* Remove dirty CPU cache ownership before handing memory to VE. */
        aicos_dcache_clean_invalid_range((unsigned long *)data,(unsigned long)next.capacity[i]);
    }
    next.live=true; a->frames[slot]=next; a->used+=(size_t)bytes;
    /* Preserve frame id/pts/flags and decoder-supplied crop metadata. */
    next.buf.crop_en=frame->buf.crop_en; next.buf.crop=frame->buf.crop;
    next.buf.flags=frame->buf.flags; frame->buf=next.buf;
    unlock(a); return 0;
}
static bool matches(const struct allocation *r,const struct mpp_frame *frame)
{
    return r->live && frame->buf.buf_type==MPP_PHY_ADDR &&
        !memcmp(r->buf.phy_addr,frame->buf.phy_addr,sizeof(r->buf.phy_addr)) &&
        r->buf.format==frame->buf.format;
}
static int free_frame(struct frame_allocator *base,struct mpp_frame *frame)
{
    lv_aic_player_allocator_t *a=(void *)base;
    if(!frame) return -1;
    lock(a);
    for(unsigned i=0;i<FRAME_COUNT;i++) if(matches(&a->frames[i],frame)) {
        struct allocation *r=&a->frames[i];
        if(r->retired) { unlock(a); return -1; }
        if(r->ticket) r->retired=true; else dispose(a,r);
        unlock(a); return 0;
    }
    /* SDK creation failure calls free on the failed, unallocated slot too. */
    bool empty=!frame->buf.phy_addr[0] && !frame->buf.phy_addr[1] && !frame->buf.phy_addr[2];
    unlock(a); return empty?0:-1;
}
static int close_allocator(struct frame_allocator *base)
{
    /* SDK closes at decoder stop, but the player retains this pointer for
     * restart. Owner alone destroys context after SDK detachment and pins. */
    (void)base; return 0;
}
static struct alloc_ops operations={allocate,free_frame,close_allocator};
lv_aic_player_allocator_t *lv_aic_player_allocator_create(size_t budget)
{
    if(!budget) return NULL;
    lv_aic_player_allocator_t *a=calloc(1,sizeof(*a));
    if(!a) return NULL;
    a->mutex=aicos_mutex_create();
    if(!a->mutex) { free(a); return NULL; }
    a->base.ops=&operations; a->budget=budget; return a;
}
struct frame_allocator *lv_aic_player_allocator_sdk(lv_aic_player_allocator_t *a)
{ return a?&a->base:NULL; }
bool lv_aic_player_allocator_destroy(lv_aic_player_allocator_t *a)
{
    if(!a) return true;
    lock(a);
    if(a->used) { unlock(a); return false; }
    unlock(a); aicos_mutex_delete(a->mutex); free(a); return true;
}
bool lv_aic_player_allocator_acquire(lv_aic_player_allocator_t *a,
    const struct mpp_frame *frame,size_t capacities[3],uint64_t *ticket)
{
    if(!a || !frame || !capacities || !ticket) return false;
    lock(a);
    for(unsigned i=0;i<FRAME_COUNT;i++) {
        struct allocation *r=&a->frames[i];
        if(!matches(r,frame) || r->retired || r->ticket || a->next_ticket==UINT64_MAX) continue;
        struct allocation view={0};
        view.buf.size.width=frame->buf.size.width;
        if(frame->buf.size.width>r->buf.size.width || frame->buf.size.height>r->buf.size.height ||
           frame->buf.stride[0]>INT32_MAX ||
           !layout(&view,(int)frame->buf.stride[0],frame->buf.size.height,frame->buf.format) ||
           memcmp(view.buf.stride,frame->buf.stride,sizeof(view.buf.stride))) continue;
        bool bounded=true;
        for(unsigned j=0;j<3;j++) if(view.capacity[j]>r->capacity[j]) bounded=false;
        if(!bounded) continue;
        r->ticket=++a->next_ticket;
        /* Decoder DMA is complete for an acquired SDK frame. Invalidate only;
         * cleaning here could overwrite freshly decoded pixels. */
        for(unsigned j=0;j<3;j++) if(r->capacity[j])
            aicos_dcache_invalid_range((unsigned long *)(uintptr_t)r->buf.phy_addr[j],
                                      (unsigned long)r->capacity[j]);
        memcpy(capacities,r->capacity,sizeof(r->capacity)); *ticket=r->ticket;
        unlock(a); return true;
    }
    unlock(a); return false;
}
bool lv_aic_player_allocator_release(lv_aic_player_allocator_t *a,uint64_t ticket)
{
    if(!a || !ticket) return false;
    lock(a);
    for(unsigned i=0;i<FRAME_COUNT;i++) if(a->frames[i].ticket==ticket) {
        struct allocation *r=&a->frames[i]; r->ticket=0;
        if(r->retired) dispose(a,r);
        unlock(a); return true;
    }
    unlock(a); return false;
}
#endif
