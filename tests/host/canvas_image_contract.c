/* SPDX-License-Identifier: Apache-2.0 */
#include "canvas_image.h"
#include <aic_osal.h>
#include <assert.h>
#include <stdint.h>
#include <string.h>
static bool fault,fail,device_missing;
static unsigned submits,emits,syncs,invalidates;
static int fail_step;
static struct ge_fillrect captured;
void lv_draw_aic_ge2d_quarantine(void) { fault=true; }
struct mpp_ge *lv_draw_aic_ge2d_device(void)
{ return device_missing || fault ? NULL : (struct mpp_ge *)(uintptr_t)1; }
int mpp_ge_fillrect(struct mpp_ge *ge,struct ge_fillrect *fill)
{ assert(ge);captured=*fill;submits++;fill->dst_buf.crop.x=99;return fail_step==1?-1:0; }
int mpp_ge_emit(struct mpp_ge *ge) { assert(ge);emits++;return fail_step==2?-1:0; }
int mpp_ge_sync(struct mpp_ge *ge) { assert(ge);syncs++;return fail_step==3?-1:0; }
void aicos_dcache_invalid_range(unsigned long *p,unsigned long size)
{ assert((uintptr_t)p && size);invalidates++; }

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
static void fill_contract(void)
{
    struct lv_mpp_buf *image=lv_mpp_image_alloc(16,16,MPP_FMT_ARGB_8888);assert(image);
    struct mpp_buf buf=image->buf,saved;
    buf.crop_en=1;buf.crop=(struct mpp_rect){2,3,8,9};saved=buf;
    for(int type=0;type<=2;type++) for(int blend=0;blend<=1;blend++) {
        unsigned old=submits,old_clean=cleans,old_invalid=invalidates;
        assert(lv_ge_fill(&buf,type,0x40123456,0xc0abcdef,blend)==LV_RESULT_OK);
        assert(submits==old+1 && cleans==old_clean+1 && invalidates==old_invalid+1);
        assert(!memcmp(&buf,&saved,sizeof(buf)));
        assert(captured.type==(enum ge_fillrect_type)type && captured.ctrl.alpha_en==(unsigned)blend);
        assert(captured.ctrl.alpha_rules==GE_PD_NONE && captured.ctrl.src_alpha_mode==0);
        assert(captured.start_color==0x40123456 && captured.end_color==0xc0abcdef);
        assert(!memcmp(&captured.dst_buf,&saved,sizeof(saved)));
    }
    for(int kind=0;kind<8;kind++) {
        struct mpp_buf bad=buf;
        if(kind==0) bad.crop.x=-1;
        if(kind==1) bad.crop.width=99;
        if(kind==2) bad.stride[0]=1;
        if(kind==3) bad.size.height=99; /* Owned allocation overrun. */
        if(kind==4) bad.phy_addr[0]=0xffffffc0;
        if(kind==5) bad.format=MPP_FMT_YUV420P;
        if(kind==6) bad.size.width=0;
        if(kind==7) bad.crop.height=0;
        unsigned old=submits;
        assert(lv_ge_fill(&bad,GE_NO_GRADIENT,0,0,0)==LV_RESULT_INVALID && submits==old && !fault);
    }
    device_missing=true;assert(lv_ge_fill(&buf,GE_NO_GRADIENT,0,0,0)==LV_RESULT_INVALID && !fault);
    device_missing=false;
    assert(lv_ge_fill(&buf,3,0,0,0)==LV_RESULT_INVALID && !fault);
    for(fail_step=1;fail_step<=3;fail_step++) {
        unsigned old=submits,old_emit=emits,old_sync=syncs,old_free=frees,old_invalid=invalidates;
        assert(lv_ge_fill(&buf,GE_H_LINEAR_GRADIENT,0,~0U,1)==LV_RESULT_INVALID && fault);
        assert(submits==old+1 && emits==old_emit+(fail_step>=2) && syncs==old_sync+(fail_step>=3));
        assert(invalidates==old_invalid);
        assert(lv_ge_fill(&buf,GE_NO_GRADIENT,0,0,0)==LV_RESULT_INVALID && submits==old+1);
        lv_mpp_image_free(image);assert(frees==old_free);
        fault=false; /* Synchronous mock only; hardware requires reboot. */
    }
    fail_step=0;
    lv_mpp_image_free(image);assert(allocs==frees);
    /* Caller-owned packed RGB destinations do not trigger guessed cache spans. */
    for(int format=MPP_FMT_ARGB_8888;format<=MPP_FMT_BGRA_4444;format++) {
        buf=saved;buf.format=format;buf.crop_en=0;
        unsigned old_clean=cleans,old_invalid=invalidates;
        assert(lv_ge_fill(&buf,GE_V_LINEAR_GRADIENT,0,~0U,0)==LV_RESULT_OK);
        assert(cleans==old_clean && invalidates==old_invalid);
    }
}
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
    assert(allocs==frees);fill_contract();lv_deinit();return 0;
}
