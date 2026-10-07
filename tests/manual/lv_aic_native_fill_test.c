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

/* GE v1.1 fillrect admits RGB and YUV400 destinations only; the SDK gate is
 * packages/artinchip/mpp/ge/cmdq_ops.c (ge_fillrect). lv_ge_fill mirrors the
 * gate locally so unsupported layouts return INVALID without submission or
 * quarantine. YUV400 fills are a measured raw path: the engine writes the
 * source red byte, not a CSC2 luma (board-2026-10-06-yuv400-csc). */
static bool fill_format_admitted(enum mpp_pixel_format fmt)
{
#if defined(AIC_GE_DRV_V11)
    return (fmt>=MPP_FMT_ARGB_8888 && fmt<=MPP_FMT_BGRA_4444) || fmt==MPP_FMT_YUV400;
#else
    (void)fmt;
    return true;
#endif
}

static void pixel_rgb(const uint8_t *p,enum mpp_pixel_format fmt,int rgb[3])
{
    if(fmt==MPP_FMT_RGB_565) {
        unsigned v=p[0]|((unsigned)p[1]<<8);
        rgb[0]=((v>>11)&31)*255/31;rgb[1]=((v>>5)&63)*255/63;rgb[2]=(v&31)*255/31;
    }
    else { rgb[0]=p[2];rgb[1]=p[1];rgb[2]=p[0]; }
}
static int gradient_probe(enum mpp_pixel_format fmt,int direction,int blend,int reverse,int alpha_gradient)
{
    struct lv_mpp_buf *owner=lv_mpp_image_alloc(16,16,fmt==MPP_FMT_RGB_565?fmt:MPP_FMT_ARGB_8888);
    if(!owner) return -1;
    struct mpp_buf dst=owner->buf;
    dst.format=fmt;dst.crop_en=1;dst.crop=(struct mpp_rect){4,4,8,8};
    unsigned bpp=fmt==MPP_FMT_RGB_565?2:fmt==MPP_FMT_RGB_888?3:4;
    const int first[3]={32,64,96},last[3]={144,232,152};
    const int *start=reverse?last:first,*end=reverse?first:last;
    uint32_t alpha=alpha_gradient?(reverse?224:32):blend?128:255;
    uint32_t end_alpha=alpha_gradient?(reverse?32:224):alpha;
    uint32_t start_color=(alpha<<24)|(start[0]<<16)|(start[1]<<8)|start[2];
    uint32_t end_color=(end_alpha<<24)|(end[0]<<16)|(end[1]<<8)|end[2];
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
    unsigned alpha_min=255,alpha_max=0,alpha_bad=0;
    for(unsigned y=0;y<16;y++) for(unsigned byte=0;byte<dst.stride[0];byte++) {
        bool active=y>=4 && y<12 && byte>=4*bpp && byte<12*bpp;
        if(!active && owner->data[y*dst.stride[0]+byte]!=0xa5) {
            AIC_TEST_E("FAIL gradient guard fmt=%u y=%u byte=%u",(unsigned)fmt,y,byte);goto done;
        }
    }
    for(unsigned y=4;y<12;y++) for(unsigned x=4;x<12;x++) {
        int actual[3];pixel_rgb(owner->data+y*dst.stride[0]+x*bpp,fmt,actual);
        int pos=direction==GE_H_LINEAR_GRADIENT?(int)x-4:(int)y-4;
        int pixel_alpha=(int)alpha+((int)end_alpha-(int)alpha)*pos/7;
        for(unsigned channel=0;channel<3;channel++) {
            int source=start[channel]+(end[channel]-start[channel])*pos/7;
            int expected=blend?(source*pixel_alpha+bg[channel]*(255-pixel_alpha)+127)/255:source;
            int error=actual[channel]-expected;if(error<0) error=-error;
            if(error>worst) worst=error;
        }
        if(bpp==4) {
            unsigned dst_alpha=owner->data[y*dst.stride[0]+x*bpp+3];
            if(!blend) {
                if(dst_alpha!=255) {
                    AIC_TEST_E("FAIL gradient opaque-background alpha fmt=%u y=%u x=%u got=%u",
                               (unsigned)fmt,y,x,dst_alpha);
                    goto done;
                }
            }
            else {
                /* Board-measured GE v1.1 blend-unit destination alpha: with an
                 * opaque 255 background the engine applies its straight-alpha
                 * OVER result to the alpha channel itself,
                 *   A_out = (A*A + 255*(255-A) + 127)/255,
                 * with A the interpolated per-pixel alpha. The D50T-2-Lite
                 * log board-2026-10-06-native-alpha-probe matched this exactly
                 * (constant A=128 -> 191; ramp 32..224 -> 228/210/198/192).
                 * Production ARGB8888 partial opacity still uses the staged
                 * path in draw/ge2d/lv_draw_aic_ge2d_fill.c; this probe pins
                 * the raw engine semantics. */
                int expected_alpha=(pixel_alpha*pixel_alpha+255*(255-pixel_alpha)+127)/255;
                int diff=(int)dst_alpha-expected_alpha;if(diff<0) diff=-diff;
                if(dst_alpha<alpha_min) alpha_min=dst_alpha;
                if(dst_alpha>alpha_max) alpha_max=dst_alpha;
                if(diff>1) alpha_bad++;
            }
        }
    }
    /* Alpha ramps can reach the far edge of a 5-bit truncation bin:
     * allow one RGB565 step plus integer interpolation rounding. */
    if(worst>(bpp==2?(alpha_gradient?9:5):2)) {
        AIC_TEST_E("FAIL gradient fmt=%u dir=%d blend=%d reverse=%d alpha_grad=%d max_error=%d",
                   (unsigned)fmt,direction,blend,reverse,alpha_gradient,worst);goto done;
    }
    if(alpha_bad) {
        AIC_TEST_E("FAIL gradient dst-alpha fmt=%u dir=%d blend=%d rev=%d ag=%d min=%u max=%u bad=%u/64",
                   (unsigned)fmt,direction,blend,reverse,alpha_gradient,
                   alpha_min,alpha_max,alpha_bad);
        goto done;
    }
    AIC_TEST_I("PASS gradient fmt=%u dir=%d blend=%d reverse=%d alpha_grad=%d error=%d guards=OK",
               (unsigned)fmt,direction,blend,reverse,alpha_gradient,worst);
    result=0;
done:
    lv_mpp_image_free(owner);return result;
}
/* RGB->YUV coefficients mirror the reviewed SDK CSC2 tables (8-bit fixed
 * point). Solid colors avoid chroma resampling phase ambiguity. */
static int yuv_probe(enum mpp_pixel_format fmt,unsigned space,unsigned color)
{
    static const int coefficients[4][12]={
        {66,129,25,16,-38,-74,112,128,112,-94,-18,128},
        {47,157,16,16,-26,-87,112,128,112,-102,-10,128},
        {77,150,29,0,-42,-84,128,128,128,-106,-20,128},
        {54,183,18,0,-28,-98,128,128,128,-115,-11,128}
    };
    const unsigned colors[]={0xff000000,0xffffffff,0xffff0000,0xff00ff00,0xff0000ff};
    struct lv_mpp_buf *owner=lv_mpp_image_alloc(16,64,MPP_FMT_ARGB_8888);
    if(!owner) return -1;
    struct mpp_buf dst=owner->buf;
    dst.size.height=16;dst.format=fmt;dst.flags=space;
    dst.crop_en=1;dst.crop=(struct mpp_rect){4,4,8,8};
    unsigned planes=1,sx=1,sy=1,packed=0,vu=0;
    switch(fmt) {
    case MPP_FMT_YUV420P: planes=3;sx=2;sy=2;break;
    case MPP_FMT_YUV422P: planes=3;sx=2;break;
    case MPP_FMT_YUV444P: planes=3;break;
    case MPP_FMT_NV12: case MPP_FMT_NV21: planes=2;sx=2;sy=2;vu=fmt==MPP_FMT_NV21;break;
    case MPP_FMT_NV16: case MPP_FMT_NV61: planes=2;sx=2;vu=fmt==MPP_FMT_NV61;break;
    case MPP_FMT_YUYV: case MPP_FMT_YVYU: case MPP_FMT_UYVY: case MPP_FMT_VYUY: packed=1;break;
    default: break; /* YUV400 */
    }
    for(unsigned i=1;i<planes;i++) {
        dst.stride[i]=64;dst.phy_addr[i]=dst.phy_addr[0]+1024*i;
    }
    memset(owner->data,0xa5,owner->size);
    int result=-1,worst=0,expected[3];
    int worst_got=0,worst_want=0;
    int active_min=256,active_max=-1;
    if(!fill_format_admitted(fmt)) {
        /* Unsupported YUV layouts must never reach the GE command queue:
         * admission is local, so expect INVALID, no quarantine and an
         * untouched buffer (every guard byte still 0xa5). */
        if(lv_ge_fill(&dst,GE_NO_GRADIENT,colors[color],colors[color],0)!=LV_RESULT_INVALID ||
           lv_draw_aic_ge2d_faulted()) {
            AIC_TEST_E("FAIL YUV admission fmt=%u space=%u color=%u fault=%u",
                       (unsigned)fmt,space,color,(unsigned)lv_draw_aic_ge2d_faulted());
            goto done;
        }
        for(unsigned offset=0;offset<(unsigned)owner->size;offset++)
            if(owner->data[offset]!=0xa5) {
                AIC_TEST_E("FAIL YUV rejection wrote fmt=%u space=%u color=%u offset=%u",
                           (unsigned)fmt,space,color,offset);
                goto done;
            }
        result=0;goto done;
    }
    for(unsigned c=0;c<3;c++) {
        const int *k=coefficients[space]+c*4;
        int sum=k[0]*(int)((colors[color]>>16)&255)+k[1]*(int)((colors[color]>>8)&255)+k[2]*(int)(colors[color]&255);
        expected[c]=(sum+128)/256+k[3];
        if(expected[c]<0) expected[c]=0;
        if(expected[c]>255) expected[c]=255;
    }
    /* Board-measured GE v1.1 YUV400 fill (D50T-2-Lite 2026-10-06, log
     * board-2026-10-06-yuv400-csc): the written byte is the raw red byte
     * of the ARGB8888 fill color - got==R for all 20 solids in all four
     * color spaces - so the CSC2 tables do not apply on this path. Model the
     * measured byte; do not treat YUV400 fill as a luma conversion. */
    if(fmt==MPP_FMT_YUV400)
        expected[0]=(int)((colors[color]>>16)&255);
    if(lv_ge_fill(&dst,GE_NO_GRADIENT,colors[color],colors[color],0)!=LV_RESULT_OK) {
        AIC_TEST_E("FAIL YUV submission fmt=%u space=%u fault=%u",(unsigned)fmt,space,
                   (unsigned)lv_draw_aic_ge2d_faulted());goto done;
    }
    /* Every allocated byte is checked, including unused plane capacity. */
    for(unsigned offset=0;offset<(unsigned)owner->size;offset++) {
        unsigned plane=offset/1024,row=(offset%1024)/64,byte=offset%64;
        unsigned x0=4,x1=12,y0=4,y1=12,channel=0;
        if(plane && plane<planes) {
            y0/=sy;y1/=sy;
            if(planes==3) { x0/=sx;x1/=sx;channel=plane; }
            else channel=1+((byte&1)^vu);
        }
        if(packed) {
            x0*=2;x1*=2;
            static const unsigned order[4][4]={{0,1,0,2},{0,2,0,1},{1,0,2,0},{2,0,1,0}};
            unsigned index=fmt==MPP_FMT_YUYV?0:fmt==MPP_FMT_YVYU?1:fmt==MPP_FMT_UYVY?2:3;
            channel=order[index][byte&3];
        }
        bool active=plane<planes && row>=y0 && row<y1 && byte>=x0 && byte<x1;
        int error=(int)owner->data[offset]-(active?expected[channel]:0xa5);
        if(error<0) error=-error;
        if(!active && error) {
            AIC_TEST_E("FAIL YUV guard fmt=%u offset=%u",(unsigned)fmt,offset);goto done;
        }
        if(active) {
            if(owner->data[offset]<active_min) active_min=owner->data[offset];
            if(owner->data[offset]>active_max) active_max=owner->data[offset];
            if(error>worst) {
                worst=error;
                worst_got=owner->data[offset];worst_want=expected[channel];
            }
        }
    }
    if(worst>2) {
        AIC_TEST_E("FAIL YUV pixel fmt=%u space=%u color=%u error=%d got=%d want=%d range=%d..%d",
            (unsigned)fmt,space,color,worst,worst_got,worst_want,active_min,active_max);
        goto done;
    }
    result=0;
done:
    lv_mpp_image_free(owner);return result;
}
/* Full-resolution YUV avoids an assumed hardware chroma sampling phase.
 * Subsampled gradients require separate phase-aware board characterization. */
static int yuv_gradient_probe(enum mpp_pixel_format fmt,unsigned space,int direction,int reverse)
{
    static const int k[4][12]={
        {66,129,25,16,-38,-74,112,128,112,-94,-18,128},
        {47,157,16,16,-26,-87,112,128,112,-102,-10,128},
        {77,150,29,0,-42,-84,128,128,128,-106,-20,128},
        {54,183,18,0,-28,-98,128,128,128,-115,-11,128}};
    struct lv_mpp_buf *owner=lv_mpp_image_alloc(16,64,MPP_FMT_ARGB_8888);
    if(!owner) return -1;
    struct mpp_buf dst=owner->buf;
    dst.size.height=16;dst.format=fmt;dst.flags=space;
    dst.crop_en=1;dst.crop=(struct mpp_rect){4,4,8,8};
    unsigned planes=fmt==MPP_FMT_YUV444P?3:1;
    for(unsigned c=1;c<planes;c++) { dst.stride[c]=64;dst.phy_addr[c]=dst.phy_addr[0]+1024*c; }
    memset(owner->data,0xa5,owner->size);
    unsigned first=reverse?0xff90e898:0xff204060,last=reverse?0xff204060:0xff90e898;
    int result=-1,worst=0;
    if(!fill_format_admitted(fmt)) {
        if(lv_ge_fill(&dst,direction,first,last,0)!=LV_RESULT_INVALID ||
           lv_draw_aic_ge2d_faulted()) {
            AIC_TEST_E("FAIL YUV gradient admission fmt=%u space=%u dir=%d rev=%d fault=%u",
                       (unsigned)fmt,space,direction,reverse,(unsigned)lv_draw_aic_ge2d_faulted());
            goto done;
        }
        for(unsigned offset=0;offset<(unsigned)owner->size;offset++)
            if(owner->data[offset]!=0xa5) {
                AIC_TEST_E("FAIL YUV gradient rejection wrote fmt=%u space=%u offset=%u",
                           (unsigned)fmt,space,offset);
                goto done;
            }
        result=0;goto done;
    }
    if(lv_ge_fill(&dst,direction,first,last,0)!=LV_RESULT_OK) goto done;
    for(unsigned offset=0;offset<(unsigned)owner->size;offset++) {
        unsigned c=offset/1024,y=(offset%1024)/64,x=offset%64;
        bool active=c<planes && y>=4 && y<12 && x>=4 && x<12;
        int expected=0xa5;
        if(active) {
            int position=direction==GE_H_LINEAR_GRADIENT?(int)x-4:(int)y-4;
            if(fmt==MPP_FMT_YUV400) {
                /* Same measured raw path as the solids: the fill writes the
                 * red byte, so the ramp interpolates R, not a CSC2 luma. */
                int a=(first>>16)&255,b=(last>>16)&255;
                expected=a+(b-a)*position/7;
            }
            else {
                int sum=0;
                for(unsigned channel=0;channel<3;channel++) {
                    unsigned shift=16-channel*8;
                    int a=(first>>shift)&255,b=(last>>shift)&255;
                    sum+=k[space][c*4+channel]*(a+(b-a)*position/7);
                }
                expected=(sum+128)/256+k[space][c*4+3];
                if(expected<0) expected=0;
                if(expected>255) expected=255;
            }
        }
        int error=(int)owner->data[offset]-expected;if(error<0) error=-error;
        if((!active && error) || (active && error>2)) {
            AIC_TEST_E("FAIL YUV gradient fmt=%u space=%u dir=%d rev=%d error=%d offset=%u got=%u want=%d",
                (unsigned)fmt,space,direction,reverse,error,offset,owner->data[offset],expected);goto done;
        }
        if(error>worst) worst=error;
    }
    AIC_TEST_I("PASS YUV gradient fmt=%u space=%u dir=%d rev=%d error=%d guards=OK",
        (unsigned)fmt,space,direction,reverse,worst);
    result=0;
done:
    lv_mpp_image_free(owner);return result;
}
static int yuv_probes(void)
{
    const enum mpp_pixel_format formats[]={MPP_FMT_YUV420P,MPP_FMT_YUV422P,MPP_FMT_YUV444P,
        MPP_FMT_NV12,MPP_FMT_NV21,MPP_FMT_NV16,MPP_FMT_NV61,
        MPP_FMT_YUYV,MPP_FMT_YVYU,MPP_FMT_UYVY,MPP_FMT_VYUY,MPP_FMT_YUV400};
    int yuv400_failures=0;
    for(unsigned f=0;f<12;f++) {
        for(unsigned space=0;space<4;space++) for(unsigned color=0;color<5;color++) {
            if(yuv_probe(formats[f],space,color)) {
                if(formats[f]!=MPP_FMT_YUV400) return -1;
                yuv400_failures++;
            }
        }
        if(formats[f]==MPP_FMT_YUV400) {
            if(yuv400_failures) {
                AIC_TEST_E("FAIL YUV400 solids %d of 20 probes mismatch; got/want above",
                           yuv400_failures);
                return -1;
            }
            AIC_TEST_I("PASS YUV400 20 raw red-byte solid probes guards=OK");
        }
        else if(fill_format_admitted(formats[f]))
            AIC_TEST_I("PASS YUV fmt=%u 20 CSC solid probes guards=OK",(unsigned)formats[f]);
        else
            AIC_TEST_I("SKIP YUV fmt=%u 20 CSC solid probes; GE v1.1 fillrect admits RGB/YUV400 only",
                       (unsigned)formats[f]);
    }
    return 0;
}
int lv_aic_native_fill_test_run(void)
{
    if(lv_draw_aic_ge2d_faulted()) return -1;
    const enum mpp_pixel_format formats[]={MPP_FMT_ARGB_8888,MPP_FMT_RGB_888,MPP_FMT_RGB_565};
    for(unsigned f=0;f<3;f++) for(int direction=1;direction<=2;direction++)
        for(int blend=0;blend<=1;blend++) for(int reverse=0;reverse<=1;reverse++)
            if(gradient_probe(formats[f],direction,blend,reverse,0)) return -1;
    for(unsigned f=0;f<3;f++) for(int direction=1;direction<=2;direction++)
        for(int reverse=0;reverse<=1;reverse++)
            if(gradient_probe(formats[f],direction,1,reverse,1)) return -1;
    if(yuv_probes()) return -1;
    for(unsigned space=0;space<4;space++) for(int direction=1;direction<=2;direction++)
        for(int reverse=0;reverse<=1;reverse++) {
            if(yuv_gradient_probe(MPP_FMT_YUV400,space,direction,reverse)) return -1;
            if(yuv_gradient_probe(MPP_FMT_YUV444P,space,direction,reverse)) return -1;
        }
    if(!fill_format_admitted(MPP_FMT_YUV444P))
        AIC_TEST_I("SKIP YUV444P 16 CSC gradient probes; GE v1.1 fillrect admits RGB/YUV400 only");
    if(fill_format_admitted(MPP_FMT_YUV444P))
        AIC_TEST_I("PASS 36 RGB gradient, 240 YUV solid, 32 YUV gradient probes; panel separate");
    else
        AIC_TEST_I("PASS 36 RGB, 20 YUV400 raw-R solid, 16 raw-R gradient; 236 other-YUV rejected");
    return 0;
}
#endif
