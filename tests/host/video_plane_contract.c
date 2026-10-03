/* SPDX-License-Identifier: Apache-2.0 */
#include "lv_aic_video_plane.h"
#include "lv_aic_rgb_image.h"
#include "lv_aic_yuv_image.h"
#include <mpp_fb.h>
#include <assert.h>
#include <string.h>
static unsigned opens,closes,updates,waits,retains,releases,cache;
static unsigned fail_update,fail_wait,busy,fail_query;
static struct aicfb_layer_data submitted;
struct mpp_fb { int unused; };static struct mpp_fb fb;
struct mpp_fb *mpp_fb_open(void) { opens++;return &fb; }
void mpp_fb_close(struct mpp_fb *p) { assert(p==&fb);closes++; }
int mpp_fb_ioctl(struct mpp_fb *p,int cmd,void *arg)
{
    assert(p==&fb);
    if(cmd==AICFB_GET_SCREENINFO) {
        if(fail_query) return -1;
        *(struct aicfb_screeninfo *)arg=(struct aicfb_screeninfo){.width=800,.height=480};return 0;
    }
    if(cmd==AICFB_GET_LAYER_CONFIG) {
        struct aicfb_layer_data *l=arg;assert(l->layer_id==AICFB_LAYER_TYPE_VIDEO);l->enable=busy;return 0;
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
static bool retain(void *p) { assert(p);retains++;return true; }
static void release(void *p) { assert(p);releases++; }
static lv_aic_rgb_image_t *rgb(unsigned n)
{
    lv_aic_rgb_frame_t f={LV_COLOR_FORMAT_RGB888,8,8,24,(const uint8_t *)(uintptr_t)(0x42000000+n*0x1000),192};
    lv_aic_rgb_image_t *r=lv_aic_rgb_image_create(&f,retain,release,&fb);assert(r);return r;
}
int main(void)
{
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
    assert(retains==releases && opens==closes);
    assert(lv_aic_rgb_image_decoder_deinit() && lv_aic_yuv_image_decoder_deinit());lv_deinit();return 0;
}
