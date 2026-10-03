/* SPDX-License-Identifier: Apache-2.0 */
#include "canvas_image.h"
#include <aic_osal.h>
#include <assert.h>
#include <stdint.h>
static bool fault,fail;
static unsigned allocs,frees,cleans;
static uintptr_t next=0x40001000,last_freed,last_cleaned;
static size_t last_size;
bool lv_draw_aic_ge2d_faulted(void) { return fault; }
void *aicos_malloc_align(unsigned int type,size_t size,size_t align)
{
    assert(type==MEM_CMA && align==64 && size && !(size&63));
    if(fail) return NULL;
    allocs++; return (void *)next;
}
void aicos_free_align(unsigned int type,void *p)
{ assert(type==MEM_CMA);frees++;last_freed=(uintptr_t)p; }
void aicos_dcache_clean_invalid_range(unsigned long *p,unsigned long bytes)
{ cleans++;last_cleaned=(uintptr_t)p;last_size=bytes; }
int main(void)
{
    lv_init();
    assert(!lv_mpp_image_alloc(0,1,MPP_FMT_RGB_565));
    assert(!lv_mpp_image_alloc(4097,1,MPP_FMT_RGB_565));
    assert(!lv_mpp_image_alloc(1,-1,MPP_FMT_RGB_565));
    assert(!lv_mpp_image_alloc(1,1,MPP_FMT_RGB_888));
    assert(!lv_aic_mpp_image_alloc_bounded(17,3,MPP_FMT_RGB_565,191));
    assert(allocs==0);
    fail=true;assert(!lv_mpp_image_alloc(17,3,MPP_FMT_RGB_565));fail=false;
    const uintptr_t bad[]={0x40001001,0xffffffc0,UINTPTR_MAX};
    for(unsigned i=0;i<3;i++) {
        next=bad[i];assert(!lv_mpp_image_alloc(17,3,MPP_FMT_RGB_565));
        assert(last_freed==next && allocs==frees);
    }
    next=0x40001000;
    for(unsigned i=0;i<100;i++) {
        struct lv_mpp_buf *image=lv_aic_mpp_image_alloc_bounded(17,3,MPP_FMT_RGB_565,192);
        assert(image && image->size==192 && image->buf.stride[0]==64);
        assert(image->buf.size.width==17 && image->buf.size.height==3);
        assert(image->buf.phy_addr[0]==next && (uintptr_t)image->data==next);
        image->data=(void *)(uintptr_t)0xdeadbeef;image->size=1;
        image->buf.phy_addr[0]=0;image->buf.stride[0]=1;
        unsigned before=cleans;
        lv_mpp_image_flush_cache(image);
        assert(cleans==before+1 && last_cleaned==next && last_size==192);
        fault=true;lv_mpp_image_flush_cache(image);lv_mpp_image_free(image);
        assert(cleans==before+1 && allocs==frees+1);
        assert(!lv_mpp_image_alloc(1,1,MPP_FMT_RGB_565));
        /* Only mocks may remove faults: real uncertain DMA requires reboot. */
        fault=false;lv_mpp_image_free(image);assert(allocs==frees && last_freed==next);
    }
    struct lv_mpp_buf *argb=lv_mpp_image_alloc(17,3,MPP_FMT_ARGB_8888);
    assert(argb && argb->size==384 && argb->buf.stride[0]==128);
    lv_mpp_image_free(argb);assert(allocs==frees);
    assert(!lv_mpp_image_alloc(1025,1024,MPP_FMT_ARGB_8888));
    struct lv_mpp_buf *large=lv_mpp_image_alloc(1024,1024,MPP_FMT_ARGB_8888);
    assert(large && large->size==4*1024*1024);
    next=0x41000000;
    struct lv_mpp_buf *second=lv_mpp_image_alloc(8,8,MPP_FMT_RGB_565);assert(second);
    lv_mpp_image_free(large); /* Remove a non-head owner. */
    lv_mpp_image_flush_cache(second);assert(last_cleaned==next && last_size==512);
    lv_mpp_image_free(second);assert(allocs==frees);
    struct lv_mpp_buf foreign={0};lv_mpp_image_flush_cache(&foreign);lv_mpp_image_free(&foreign);
    lv_mpp_image_flush_cache(NULL);lv_mpp_image_free(NULL);
    assert(allocs==frees);lv_deinit();return 0;
}
