/* SPDX-License-Identifier: Apache-2.0 */
#include "lv_aic_player_allocator.h"
#include <aic_osal.h>
#include "lv_aic_yuv_mpp.h"
#include <assert.h>
#include <string.h>
static int locked, mutex_live, allocs, frees, cleans, invalidates, fail_at, attempts;
static uintptr_t next_address=0x40001000;
static size_t sizes[256];
static uintptr_t addresses[256];
static bool mutex_fail;
aicos_mutex_t aicos_mutex_create(void)
{ if(mutex_fail) return NULL; assert(!mutex_live); mutex_live=1; return &mutex_live; }
void aicos_mutex_delete(aicos_mutex_t mutex) { assert(mutex==&mutex_live && !locked); mutex_live=0; }
int aicos_mutex_take(aicos_mutex_t mutex,uint32_t timeout)
{ assert(mutex==&mutex_live && timeout==AICOS_WAIT_FOREVER && !locked); locked=1; return 0; }
int aicos_mutex_give(aicos_mutex_t mutex) { assert(mutex==&mutex_live && locked); locked=0; return 0; }
void *aicos_malloc_align(unsigned int type,size_t size,size_t align)
{
    assert(locked && type==MEM_CMA && align==32 && size && !(size&31));
    if(++attempts==fail_at) return NULL;
    assert(allocs<256); addresses[allocs]=next_address; sizes[allocs++]=size;
    next_address+=0x10000; return (void *)addresses[allocs-1];
}
static unsigned find(uintptr_t address)
{ unsigned i=0; while(i<(unsigned)allocs && addresses[i]!=address) i++; assert(i<(unsigned)allocs && sizes[i]); return i; }
void aicos_free_align(unsigned int type,void *ptr)
{ assert(locked && type==MEM_CMA); sizes[find((uintptr_t)ptr)]=0; frees++; }
void aicos_dcache_clean_invalid_range(unsigned long *addr,unsigned long size)
{ assert(locked && sizes[find((uintptr_t)addr)]==size); cleans++; }
void aicos_dcache_invalid_range(unsigned long *addr,unsigned long size)
{ assert(locked && sizes[find((uintptr_t)addr)]==size); invalidates++; }
static void reject(lv_aic_player_allocator_t *a,const struct mpp_frame *frame)
{
    size_t cap[3]={11,22,33}; uint64_t ticket=99;
    assert(!lv_aic_player_allocator_acquire(a,frame,cap,&ticket));
    assert(cap[0]==11 && cap[1]==22 && cap[2]==33 && ticket==99);
}
int main(void)
{
    assert(!lv_aic_player_allocator_create(0));
    mutex_fail=true; assert(!lv_aic_player_allocator_create(4096)); mutex_fail=false;
    lv_aic_player_allocator_t *a=lv_aic_player_allocator_create(4096);
    assert(a); struct frame_allocator *sdk=lv_aic_player_allocator_sdk(a);
    struct mpp_frame f={.id=7,.pts=123,.buf={.size={32,16}}}, saved=f;
    for(int plane=1;plane<=3;plane++) {
        fail_at=attempts+plane;
        assert(sdk->ops->alloc_frame_buffer(sdk,&f,32,16,MPP_FMT_YUV420P)<0);
        assert(!memcmp(&f,&saved,sizeof(f)) && allocs==frees);
        assert(!sdk->ops->free_frame_buffer(sdk,&f));
    }
    fail_at=0;
    assert(sdk->ops->alloc_frame_buffer(sdk,&f,31,16,MPP_FMT_YUV420P)<0);
    assert(sdk->ops->alloc_frame_buffer(sdk,&f,32,15,MPP_FMT_YUV420P)<0);
    assert(sdk->ops->alloc_frame_buffer(sdk,&f,32,16,MPP_FMT_NV16)<0);
    assert(!sdk->ops->alloc_frame_buffer(sdk,&f,32,16,MPP_FMT_YUV420P));
    assert(f.id==7 && f.pts==123 && f.buf.stride[1]==16);
    assert(!lv_aic_player_allocator_destroy(a));
    size_t cap[3]; uint64_t ticket;
    assert(lv_aic_player_allocator_acquire(a,&f,cap,&ticket));
    assert(cap[0]==512 && cap[1]==128 && cap[2]==128 && invalidates==3);
    lv_aic_yuv_frame_t view;
    f.buf.crop_en=1; f.buf.crop=(struct mpp_rect){2,2,30,14};
    assert(lv_aic_yuv_from_mpp(&f.buf,cap,LV_AIC_YUV_BT601_LIMITED,&view));
    assert(view.planes[0].capacity==446 && view.planes[1].capacity==111);
    assert(view.width==30 && view.height==14);
    reject(a,&f); int old_frees=frees;
    assert(!sdk->ops->free_frame_buffer(sdk,&f) && frees==old_frees);
    assert(sdk->ops->free_frame_buffer(sdk,&f)<0);
    assert(!sdk->ops->close_allocator(sdk));
    reject(a,&f); assert(!lv_aic_player_allocator_destroy(a));
    assert(lv_aic_player_allocator_release(a,ticket) && frees==old_frees+3);
    assert(!lv_aic_player_allocator_release(a,ticket));
    f=saved; assert(!sdk->ops->alloc_frame_buffer(sdk,&f,32,16,MPP_FMT_YUV420P));
    struct mpp_frame bad=f; bad.buf.phy_addr[1]++; reject(a,&bad);
    bad=f; bad.buf.stride[1]++; reject(a,&bad);
    bad=f; bad.buf.size.height=18; reject(a,&bad);
    bad=f; bad.buf.stride[0]=UINT32_MAX; reject(a,&bad);
    /* H264 maximum-size mode reuses allocations with a smaller frame pitch. */
    f.buf.size.width=16; f.buf.size.height=8; f.buf.stride[0]=16;
    f.buf.stride[1]=f.buf.stride[2]=8;
    uint64_t second;
    assert(lv_aic_player_allocator_acquire(a,&f,cap,&second) && second>ticket);
    assert(!lv_aic_player_allocator_release(a,ticket));
    assert(lv_aic_player_allocator_release(a,second));
    assert(!sdk->ops->free_frame_buffer(sdk,&f));
    const enum mpp_pixel_format formats[]={MPP_FMT_YUV420P,MPP_FMT_YUV422P,MPP_FMT_YUV444P,
        MPP_FMT_NV12,MPP_FMT_NV21,MPP_FMT_YUV400,MPP_FMT_RGB_565,MPP_FMT_BGR_565,
        MPP_FMT_RGB_888,MPP_FMT_BGR_888,MPP_FMT_ARGB_8888,MPP_FMT_ABGR_8888,
        MPP_FMT_RGBA_8888,MPP_FMT_BGRA_8888};
    for(unsigned i=0;i<sizeof(formats)/sizeof(formats[0]);i++) {
        f=saved;
        assert(!sdk->ops->alloc_frame_buffer(sdk,&f,128,8,formats[i]));
        assert(lv_aic_player_allocator_acquire(a,&f,cap,&ticket));
        assert(lv_aic_player_allocator_release(a,ticket));
        assert(!sdk->ops->free_frame_buffer(sdk,&f));
    }
    struct mpp_frame many[32];
    for(unsigned i=0;i<32;i++) {
        many[i]=saved; many[i].buf.size.width=1;
        assert(!sdk->ops->alloc_frame_buffer(sdk,&many[i],1,1,MPP_FMT_YUV400));
    }
    f=saved; f.buf.size.width=1;
    assert(sdk->ops->alloc_frame_buffer(sdk,&f,1,1,MPP_FMT_YUV400)<0);
    for(unsigned i=0;i<32;i++) assert(!sdk->ops->free_frame_buffer(sdk,&many[i]));
    assert(lv_aic_player_allocator_destroy(a));
    a=lv_aic_player_allocator_create(767); sdk=lv_aic_player_allocator_sdk(a); f=saved;
    assert(sdk->ops->alloc_frame_buffer(sdk,&f,32,16,MPP_FMT_YUV420P)<0);
    assert(lv_aic_player_allocator_destroy(a));
    a=lv_aic_player_allocator_create(768); sdk=lv_aic_player_allocator_sdk(a); f=saved;
    assert(!sdk->ops->alloc_frame_buffer(sdk,&f,32,16,MPP_FMT_YUV420P));
    assert(lv_aic_player_allocator_acquire(a,&f,cap,&ticket));
    assert(!sdk->ops->free_frame_buffer(sdk,&f));
    f=saved; /* Retired but pinned memory still consumes the budget. */
    assert(sdk->ops->alloc_frame_buffer(sdk,&f,32,16,MPP_FMT_YUV420P)<0);
    assert(lv_aic_player_allocator_release(a,ticket));
    assert(!sdk->ops->alloc_frame_buffer(sdk,&f,32,16,MPP_FMT_YUV420P));
    assert(!sdk->ops->free_frame_buffer(sdk,&f));
    next_address=0xffffffe0;
    f=saved; assert(sdk->ops->alloc_frame_buffer(sdk,&f,32,16,MPP_FMT_YUV420P)<0);
    next_address=0x40000001;
    assert(sdk->ops->alloc_frame_buffer(sdk,&f,32,16,MPP_FMT_YUV420P)<0);
#if UINTPTR_MAX > UINT32_MAX
    next_address=(uintptr_t)UINT32_MAX+1;
    assert(sdk->ops->alloc_frame_buffer(sdk,&f,32,16,MPP_FMT_YUV420P)<0);
#endif
    assert(lv_aic_player_allocator_destroy(a));
    assert(allocs==frees && !mutex_live && cleans>0);
    return 0;
}
