/* SPDX-License-Identifier: Apache-2.0 */
#define LOG_TAG "lvgl.native.fill"
#define LOG_LVL LOG_LVL_INFO
#include "lv_aic_manual_test.h"
#if AIC_LVGL_USE_CANVAS && AIC_LVGL_USE_GE2D && AIC_LVGL_BSP_RTTHREAD
#define AIC_LVGL_USE_PRIVATE_API 1
#include "lv_draw_aic_ge2d.h"
#include "canvas_image.h"
#include "lv_aic_test_log.h"
#include <string.h>

static void pixel_rgb(const uint8_t *p,enum mpp_pixel_format fmt,int rgb[3])
{
    if(fmt==MPP_FMT_RGB_565) {
        unsigned v=p[0]|((unsigned)p[1]<<8);
        rgb[0]=((v>>11)&31)*255/31;rgb[1]=((v>>5)&63)*255/63;rgb[2]=(v&31)*255/31;
    }
    else { rgb[0]=p[2];rgb[1]=p[1];rgb[2]=p[0]; }
}
static int gradient_probe(enum mpp_pixel_format fmt,int direction,int blend,int reverse)
{
    struct lv_mpp_buf *owner=lv_mpp_image_alloc(16,16,fmt==MPP_FMT_RGB_565?fmt:MPP_FMT_ARGB_8888);
    if(!owner) return -1;
    struct mpp_buf dst=owner->buf;
    dst.format=fmt;dst.crop_en=1;dst.crop=(struct mpp_rect){4,4,8,8};
    unsigned bpp=fmt==MPP_FMT_RGB_565?2:fmt==MPP_FMT_RGB_888?3:4;
    const int first[3]={32,64,96},last[3]={144,232,152};
    const int *start=reverse?last:first,*end=reverse?first:last;
    uint32_t alpha=blend?128:255;
    uint32_t start_color=(alpha<<24)|(start[0]<<16)|(start[1]<<8)|start[2];
    uint32_t end_color=(alpha<<24)|(end[0]<<16)|(end[1]<<8)|end[2];
    uint8_t background[4]={80,48,16,255};
    if(bpp==2) {
        unsigned packed=((16>>3)<<11)|((48>>2)<<5)|(80>>3);
        background[0]=packed; background[1]=packed>>8;
    }
    int bg[3];pixel_rgb(background,fmt,bg);
    memset(owner->data,0xa5,owner->size);
    for(unsigned y=4;y<12;y++) for(unsigned x=4;x<12;x++)
        memcpy(owner->data+y*dst.stride[0]+x*bpp,background,bpp);
    if(lv_ge_fill(&dst,direction,start_color,end_color,blend)!=LV_RESULT_OK) {
        AIC_TEST_E("FAIL gradient submission; fault=%u storage retained if faulted",
                   (unsigned)lv_draw_aic_ge2d_faulted());
        lv_mpp_image_free(owner);return -1;
    }
    int worst=0,result=-1;
    for(unsigned y=0;y<16;y++) for(unsigned byte=0;byte<dst.stride[0];byte++) {
        bool active=y>=4 && y<12 && byte>=4*bpp && byte<12*bpp;
        if(!active && owner->data[y*dst.stride[0]+byte]!=0xa5) {
            AIC_TEST_E("FAIL gradient guard fmt=%u y=%u byte=%u",(unsigned)fmt,y,byte);goto done;
        }
    }
    for(unsigned y=4;y<12;y++) for(unsigned x=4;x<12;x++) {
        int actual[3];pixel_rgb(owner->data+y*dst.stride[0]+x*bpp,fmt,actual);
        int pos=direction==GE_H_LINEAR_GRADIENT?(int)x-4:(int)y-4;
        for(unsigned channel=0;channel<3;channel++) {
            int source=start[channel]+(end[channel]-start[channel])*pos/7;
            int expected=blend?(source*128+bg[channel]*127+127)/255:source;
            int error=actual[channel]-expected;if(error<0) error=-error;
            if(error>worst) worst=error;
        }
        if(!blend && bpp==4 && owner->data[y*dst.stride[0]+x*bpp+3]!=255) {
            AIC_TEST_E("FAIL gradient replacement alpha");goto done;
        }
    }
    if(worst>(bpp==2?5:2)) {
        AIC_TEST_E("FAIL gradient fmt=%u dir=%d blend=%d reverse=%d max_error=%d",
                   (unsigned)fmt,direction,blend,reverse,worst);goto done;
    }
    AIC_TEST_I("PASS gradient fmt=%u dir=%d blend=%d reverse=%d error=%d guards=OK",
               (unsigned)fmt,direction,blend,reverse,worst);
    result=0;
done:
    lv_mpp_image_free(owner);return result;
}
int lv_aic_native_fill_test_run(void)
{
    if(lv_draw_aic_ge2d_faulted()) return -1;
    const enum mpp_pixel_format formats[]={MPP_FMT_ARGB_8888,MPP_FMT_RGB_888,MPP_FMT_RGB_565};
    for(unsigned f=0;f<3;f++) for(int direction=1;direction<=2;direction++)
        for(int blend=0;blend<=1;blend++) for(int reverse=0;reverse<=1;reverse++)
            if(gradient_probe(formats[f],direction,blend,reverse)) return -1;
    AIC_TEST_I("PASS 24 native gradient probes; YUV CSC acceptance remains separate");
    return 0;
}
#endif
