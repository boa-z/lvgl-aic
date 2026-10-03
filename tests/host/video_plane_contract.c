/* SPDX-License-Identifier: Apache-2.0 */
#include "lv_aic_video_plane.h"
#include "lv_aic_rgb_image.h"
#include "lv_aic_yuv_image.h"
#include <mpp_fb.h>
#include <mpp_ge.h>
#include <aic_osal.h>
#include <stdlib.h>
#include <assert.h>
#include <string.h>
static unsigned opens,closes,updates,waits,retains,releases,cache;
static unsigned fail_update,fail_wait,busy,fail_query;
static struct aicfb_layer_data submitted;
static unsigned fail_alpha_get,fail_alpha_set,alpha_updates;
static enum mpp_pixel_format ui_format=MPP_FMT_ARGB_8888;
static struct aicfb_alpha_config alpha={.layer_id=AICFB_LAYER_TYPE_UI,.enable=1,
    .mode=AICFB_MIXDER_ALPHA_MODE,.value=173};
struct mpp_fb { int unused; };static struct mpp_fb fb;
struct mpp_fb *mpp_fb_open(void) { opens++;return &fb; }
void mpp_fb_close(struct mpp_fb *p) { assert(p==&fb);closes++; }
int mpp_fb_ioctl(struct mpp_fb *p,int cmd,void *arg)
{
    assert(p==&fb);
    if(cmd==AICFB_GET_SCREENINFO) {
        if(fail_query) return -1;
        *(struct aicfb_screeninfo *)arg=(struct aicfb_screeninfo){.format=ui_format,.width=800,.height=480};return 0;
    }
    if(cmd==AICFB_GET_LAYER_CONFIG) {
        struct aicfb_layer_data *l=arg;assert(l->layer_id==AICFB_LAYER_TYPE_VIDEO);l->enable=busy;return 0;
    }
    if(cmd==AICFB_GET_ALPHA_CONFIG) {
        assert(((struct aicfb_alpha_config *)arg)->layer_id==AICFB_LAYER_TYPE_UI);
        if(fail_alpha_get) return -1;
        *(struct aicfb_alpha_config *)arg=alpha;return 0;
    }
    if(cmd==AICFB_UPDATE_ALPHA_CONFIG) {
        alpha=*(struct aicfb_alpha_config *)arg;alpha_updates++;
        assert(alpha.layer_id==AICFB_LAYER_TYPE_UI);
        return fail_alpha_set?-1:0; /* Exercise partial mutation even on failure. */
    }
    if(cmd==AICFB_UPDATE_LAYER_CONFIG) {
        submitted=*(struct aicfb_layer_data *)arg;updates++;
        assert(submitted.layer_id==AICFB_LAYER_TYPE_VIDEO && submitted.rect_id==0);
        return fail_update?-1:0;
    }
    assert(cmd==AICFB_WAIT_FOR_VSYNC);waits++;return fail_wait && waits==fail_wait?-1:0;
}
void aicos_dcache_clean_invalid_range(unsigned long *p,unsigned long size)
{ assert((uintptr_t)p>=0x40000000 && size);cache++; }
struct mpp_ge { int unused; };static struct mpp_ge ge;
static unsigned ge_opened,ge_closed,ge_calls,ge_failure,fail_alloc;
static size_t cma_live,cma_peak;
static struct { void *ptr;size_t size; } allocations[64];
static unsigned allocated;
static struct ge_bitblt rotation;
struct mpp_ge *mpp_ge_open(void) { ge_opened++;return &ge; }
void mpp_ge_close(struct mpp_ge *p) { assert(p==&ge);ge_closed++; }
int mpp_ge_bitblt(struct mpp_ge *p,struct ge_bitblt *b)
{ assert(p==&ge);rotation=*b;ge_calls++;return ge_failure==1?-1:0; }
int mpp_ge_emit(struct mpp_ge *p) { assert(p==&ge);return ge_failure==2?-1:0; }
int mpp_ge_sync(struct mpp_ge *p) { assert(p==&ge);return ge_failure==3?-1:0; }
void *aicos_malloc_align(unsigned int type,size_t bytes,size_t align)
{
    assert(type==MEM_CMA && align==64 && allocated<64);
    if(fail_alloc) return NULL;
    void *ptr=(void *)(uintptr_t)(0x48000000+allocated*0x10000);
    allocations[allocated].ptr=ptr;allocations[allocated++].size=bytes;
    cma_live+=bytes;if(cma_live>cma_peak) cma_peak=cma_live;return ptr;
}
void aicos_free_align(unsigned int type,void *ptr)
{
    assert(type==MEM_CMA);
    for(unsigned i=0;i<allocated;i++) if(allocations[i].ptr==ptr) {
        assert(allocations[i].size && cma_live>=allocations[i].size);
        cma_live-=allocations[i].size;allocations[i].size=0;return;
    }
    assert(!"unknown copy allocation");
}
static bool retain(void *p) { assert(p);retains++;return true; }
static void release(void *p) { assert(p);releases++; }
static lv_aic_rgb_image_t *rgb(unsigned n)
{
    lv_aic_rgb_frame_t f={LV_COLOR_FORMAT_RGB888,8,8,24,(const uint8_t *)(uintptr_t)(0x42000000+n*0x1000),192};
    lv_aic_rgb_image_t *r=lv_aic_rgb_image_create(&f,retain,release,&fb);assert(r);return r;
}
int main(int argc,char **argv)
{
    (void)argc;(void)argv;
    lv_init();assert(lv_aic_rgb_image_decoder_init() && lv_aic_yuv_image_decoder_init());
    fail_query=1;assert(!lv_aic_video_plane_open());fail_query=0;
    busy=1;assert(!lv_aic_video_plane_open());busy=0;assert(opens==closes);
    lv_aic_video_plane_t *p=lv_aic_video_plane_open();assert(p && !lv_aic_video_plane_open());
    lv_aic_rgb_image_t *a=rgb(0),*b=rgb(1);
    assert(!lv_aic_video_plane_present(p,lv_aic_rgb_image_source(a),-1,0,8,8));
    assert(!lv_aic_video_plane_present(p,lv_aic_rgb_image_source(a),795,0,8,8));assert(!updates && !cache);
    assert(lv_aic_video_plane_present(p,lv_aic_rgb_image_source(a),20,30,16,16));
    assert(submitted.buf.phy_addr[0]==0x42000000 && submitted.buf.format==MPP_FMT_RGB_888);
    assert(submitted.pos.x==20 && submitted.scale_size.width==16 && waits==2);
    lv_aic_rgb_image_destroy(a);assert(!releases);
    assert(lv_aic_video_plane_present(p,lv_aic_rgb_image_source(b),0,0,8,8));assert(releases==1);
    lv_aic_rgb_image_destroy(b);assert(releases==1);
    assert(lv_aic_video_plane_hide(p));assert(releases==2 && !submitted.enable);
    /* Either update or either synchronization failure retains both readers. */
    for(unsigned mode=0;mode<3;mode++) {
        a=rgb(2);b=rgb(3);
        assert(lv_aic_video_plane_present(p,lv_aic_rgb_image_source(a),0,0,8,8));
        lv_aic_rgb_image_destroy(a);unsigned before=releases;
        if(!mode) fail_update=1;else fail_wait=waits+mode;
        assert(!lv_aic_video_plane_present(p,lv_aic_rgb_image_source(b),0,0,8,8));
        lv_aic_rgb_image_destroy(b);assert(releases==before && lv_aic_video_plane_faulted(p));
        a=rgb(4);unsigned u=updates;
        assert(!lv_aic_video_plane_present(p,lv_aic_rgb_image_source(a),0,0,8,8));assert(updates==u);
        lv_aic_rgb_image_destroy(a);before++;
        fail_update=1;assert(!lv_aic_video_plane_close(p));assert(releases==before && !lv_aic_video_plane_open());
        fail_update=0;fail_wait=0;assert(lv_aic_video_plane_hide(p));assert(releases==before+2);
    }
    lv_aic_yuv_frame_t f={.format=LV_COLOR_FORMAT_NV12,.width=8,.height=8,
        .color_space=LV_AIC_YUV_BT709_LIMITED,.planes={
        {(const uint8_t *)0x43000000,8,64},{(const uint8_t *)0x43001000,8,32}}};
    lv_aic_yuv_image_t *y=lv_aic_yuv_image_create(&f,retain,release,&fb);assert(y);
    assert(lv_aic_video_plane_present(p,lv_aic_yuv_image_source(y),0,0,32,32));
    assert(submitted.buf.format==MPP_FMT_NV12 && submitted.buf.phy_addr[1]==0x43001000);
    lv_aic_yuv_image_destroy(y);assert(lv_aic_video_plane_close(p));
    assert(retains==releases && opens==closes && !alpha_updates);
    struct aicfb_alpha_config original=alpha;
    ui_format=MPP_FMT_RGB_565;p=lv_aic_video_plane_open();assert(p);
    assert(!lv_aic_video_plane_enable_ui_alpha(p) && !alpha_updates);
    assert(lv_aic_video_plane_close(p));ui_format=MPP_FMT_ARGB_8888;
    p=lv_aic_video_plane_open();assert(p);fail_alpha_get=1;
    assert(!lv_aic_video_plane_enable_ui_alpha(p) && !alpha_updates);
    fail_alpha_get=0;assert(lv_aic_video_plane_close(p));
    for(unsigned mode=0;mode<7;mode++) {
        p=lv_aic_video_plane_open();assert(p);
        if(mode==0) fail_alpha_set=1;
        if(mode==1 || mode==2) fail_wait=waits+mode;
        bool applied=lv_aic_video_plane_enable_ui_alpha(p);
        assert(applied==(mode>=3));
        assert(alpha.enable && alpha.mode==AICFB_PIXEL_ALPHA_MODE && alpha.value==255);
        if(applied) {
            unsigned u=alpha_updates;assert(lv_aic_video_plane_enable_ui_alpha(p) && alpha_updates==u);
            a=rgb(9);assert(lv_aic_video_plane_present(p,lv_aic_rgb_image_source(a),0,0,8,8));
            lv_aic_rgb_image_destroy(a);assert(lv_aic_video_plane_hide(p));
            assert(alpha.mode==AICFB_PIXEL_ALPHA_MODE); /* hide retains alpha ownership */
        } else {
            assert(lv_aic_video_plane_faulted(p));assert(lv_aic_video_plane_hide(p));
            assert(lv_aic_video_plane_faulted(p));
            a=rgb(10);assert(!lv_aic_video_plane_present(p,lv_aic_rgb_image_source(a),0,0,8,8));
            lv_aic_rgb_image_destroy(a);
        }
        fail_alpha_set=0;fail_wait=0;
        if(mode==3) fail_alpha_set=1;
        if(mode==4 || mode==5) fail_wait=waits+mode-3;
        if(mode>=3 && mode<=5) {
            unsigned closed=closes;assert(!lv_aic_video_plane_close(p));
            assert(closes==closed && lv_aic_video_plane_faulted(p) && !lv_aic_video_plane_open());
            fail_alpha_set=0;fail_wait=0;
        }
        assert(lv_aic_video_plane_close(p));assert(!memcmp(&alpha,&original,sizeof(alpha)));
    }
    assert(retains==releases && opens==closes);
#if AIC_LVGL_USE_GE2D
    /* Rotation copies have their own explicit budget, independent of decoder CMA. */
    p=lv_aic_video_plane_open();assert(p);
    lv_aic_rgb_frame_t native={LV_COLOR_FORMAT_RGB565,16,8,32,(const uint8_t *)0x44000000,256};
    a=lv_aic_rgb_image_create(&native,retain,release,&fb);assert(a);
    const void *src=lv_aic_rgb_image_source(a);unsigned u=updates;
    assert(!lv_aic_video_plane_present_rotated(p,src,0,0,8,16,45,2048));
    assert(!lv_aic_video_plane_present_rotated(p,src,0,0,8,16,90,1023));assert(updates==u && !cma_live);
    fail_alloc=1;assert(!lv_aic_video_plane_present_rotated(p,src,0,0,8,16,90,2048));
    fail_alloc=0;assert(!cma_live && !lv_aic_video_plane_faulted(p));
    assert(lv_aic_video_plane_present_rotated(p,src,0,0,8,16,90,1024));
    assert(cma_live==1024 && rotation.ctrl.flags==MPP_ROTATION_90 && !rotation.ctrl.alpha_en);
    assert(rotation.dst_buf.size.width==8 && rotation.dst_buf.size.height==16 && rotation.dst_buf.stride[0]==64);
    assert(submitted.buf.format==MPP_FMT_ARGB_8888 && submitted.buf.phy_addr[0]==rotation.dst_buf.phy_addr[0]);
    u=updates;unsigned calls=ge_calls;
    assert(!lv_aic_video_plane_present_rotated(p,src,0,0,8,16,270,1024));assert(updates==u && ge_calls==calls);
    assert(lv_aic_video_plane_present_rotated(p,src,0,0,8,16,270,2048));assert(cma_live==1024 && cma_peak==2048);
    assert(rotation.ctrl.flags==MPP_ROTATION_270);
    assert(lv_aic_video_plane_present_rotated(p,src,0,0,16,8,180,1536));
    assert(cma_live==512 && rotation.ctrl.flags==MPP_ROTATION_180 && rotation.dst_buf.size.width==16);
    fail_update=1;assert(!lv_aic_video_plane_present_rotated(p,src,0,0,8,16,90,1536));
    assert(cma_live==1536);lv_aic_rgb_image_destroy(a);
    assert(retains==releases); /* GE completed: source can retire even if DE failed. */
    fail_update=0;assert(lv_aic_video_plane_hide(p));assert(!cma_live);
    y=lv_aic_yuv_image_create(&f,retain,release,&fb);assert(y);
    assert(lv_aic_video_plane_present_rotated(p,lv_aic_yuv_image_source(y),0,0,16,16,90,512));
    assert(rotation.src_buf.format==MPP_FMT_NV12 && rotation.dst_buf.format==MPP_FMT_ARGB_8888);
    lv_aic_yuv_image_destroy(y);assert(lv_aic_video_plane_close(p));
    assert(!cma_live && ge_opened==ge_closed && retains==releases && opens==closes);
    p=lv_aic_video_plane_open();assert(p);
    native.data=(const uint8_t *)(uintptr_t)(0x48000000+allocated*0x10000);
    a=lv_aic_rgb_image_create(&native,retain,release,&fb);assert(a);calls=ge_calls;
    assert(!lv_aic_video_plane_present_rotated(p,lv_aic_rgb_image_source(a),0,0,8,16,90,1024));
    assert(ge_calls==calls && !cma_live && !lv_aic_video_plane_faulted(p));
    lv_aic_rgb_image_destroy(a);assert(lv_aic_video_plane_close(p));
    if(argc==2) {
        p=lv_aic_video_plane_open();assert(p);a=rgb(12);src=lv_aic_rgb_image_source(a);
        assert(lv_aic_video_plane_present_rotated(p,src,0,0,8,8,90,1024));
        ge_failure=(unsigned)atoi(argv[1]);assert(ge_failure>=1 && ge_failure<=3);
        assert(!lv_aic_video_plane_present_rotated(p,src,0,0,8,8,180,1024));
        lv_aic_rgb_image_destroy(a);assert(cma_live==1024 && retains==releases+1);
        assert(lv_aic_video_plane_faulted(p));unsigned closed=closes;
        ge_failure=0;assert(!lv_aic_video_plane_hide(p) && !lv_aic_video_plane_close(p));
        assert(closes==closed && cma_live==1024 && !lv_aic_video_plane_open());
        /* Process exit models reboot. No force-release of uncertain GE readers. */
        return 0;
    }
#else
    p=lv_aic_video_plane_open();assert(p);a=rgb(13);
    assert(!lv_aic_video_plane_present_rotated(p,lv_aic_rgb_image_source(a),0,0,8,8,90,1024));
    assert(!ge_calls && !cma_live && !lv_aic_video_plane_faulted(p));
    assert(lv_aic_video_plane_present(p,lv_aic_rgb_image_source(a),0,0,8,8));
    lv_aic_rgb_image_destroy(a);assert(lv_aic_video_plane_close(p));
    assert(retains==releases && opens==closes);
#endif
    assert(lv_aic_rgb_image_decoder_deinit() && lv_aic_yuv_image_decoder_deinit());lv_deinit();return 0;
}
