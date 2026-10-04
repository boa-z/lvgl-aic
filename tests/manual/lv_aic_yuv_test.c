/* SPDX-License-Identifier: Apache-2.0 */
#define AIC_LVGL_USE_PRIVATE_API 1
#define LOG_TAG "lvgl.yuv"
#define LOG_LVL LOG_LVL_INFO
#include "lv_aic_manual_test.h"
#if AIC_LVGL_USE_GE2D && AIC_LVGL_BSP_RTTHREAD
#include "lv_aic_yuv.h"
#include "lv_aic_yuv_mpp.h"
#include "lv_aic_yuv_image.h"
#include "lvgl_aic_private.h"
#include "lv_draw_aic_ge2d.h"
#include "lv_draw_aic_ge2d_yuv.h"
#include <aic_core.h>
#include <aic_osal.h>
#include "lv_aic_test_log.h"
#include <string.h>

static bool retain_probe(void *context) { (*(int *)context)++; return true; }
static void release_probe(void *context) { (*(int *)context)--; }
static int ge_probe(void)
{
    if (lv_draw_aic_ge2d_yuv_faulted()) return -1;
    uint8_t *source=aicos_malloc_align(MEM_CMA,1024,32);
    uint8_t *output=aicos_malloc_align(MEM_CMA,64*64*3,32);
    uint8_t *reference=lv_malloc(32*16*3);
    bool registered=false;
    lv_aic_yuv_image_t *image=NULL;
    int result=-1;
    /* The callback context must also outlive a quarantined source lease. */
    static int refs;
    lv_aic_yuv_frame_t frame={0};
    if (!source || !output || !reference) goto done;
    frame.format=LV_COLOR_FORMAT_I420; frame.width=32; frame.height=16;
    frame.color_space=LV_AIC_YUV_BT601_LIMITED;
    frame.planes[0]=(lv_aic_yuv_plane_t){source,32,512};
    frame.planes[1]=(lv_aic_yuv_plane_t){source+512,16,256};
    frame.planes[2]=(lv_aic_yuv_plane_t){source+768,16,256};
    for (unsigned y=0;y<16;y++)
        for (unsigned x=0;x<32;x++) source[y*32+x]=16+5*x+3*y;
    memset(source+512,90,256); memset(source+768,240,256);
    if (!lv_aic_yuv_to_rgb888(&frame,reference,96,32*16*3)) goto done;
    if (!lv_aic_yuv_image_decoder_init()) goto done;
    registered=true;
    image=lv_aic_yuv_image_create(&frame,retain_probe,release_probe,&refs);
    if (!image) goto done;
    lv_draw_buf_t dst;
    if (lv_draw_buf_init(&dst,64,64,LV_COLOR_FORMAT_RGB888,192,output,64*64*3)!=LV_RESULT_OK) goto done;
    lv_layer_t layer={0};
    layer.draw_buf=&dst; layer.color_format=LV_COLOR_FORMAT_RGB888;
    layer.buf_area=(lv_area_t){0,0,63,63};
    lv_draw_image_dsc_t d;
    lv_draw_image_dsc_init(&d); d.src=lv_aic_yuv_image_source(image);
    if (lv_image_decoder_get_info(d.src,&d.header)!=LV_RESULT_OK) goto done;
    d.pivot=(lv_point_t){0,0};
    lv_draw_task_t task={0};
    task.type=LV_DRAW_TASK_TYPE_IMAGE; task.draw_dsc=&d; task.target_layer=&layer;
    task.area=(lv_area_t){32,32,63,47}; task.clip_area=layer.buf_area;
    for (int angle=0;angle<4;angle++) {
        int worst=0, checked=0;
        d.rotation=angle*900;
        memset(output,0xa5,64*64*3);
        aicos_dcache_clean_invalid_range((unsigned long *)output,64*64*3);
        lv_draw_aic_ge2d_outcome_t outcome;
        if (lv_draw_aic_ge2d_image(&task,&outcome)!=LV_RESULT_OK) goto done;
        if (outcome!=LV_DRAW_AIC_GE2D_OUTCOME_ENGINE) goto done;
        aicos_dcache_invalid_range((unsigned long *)output,64*64*3);
        for (int y=0;y<64;y++) {
            for (int x=0;x<64;x++) {
                int dx=x-32, dy=y-32;
                int sx=angle==0 ? dx : angle==1 ? dy : angle==2 ? -dx : -dy;
                int sy=angle==0 ? dy : angle==1 ? -dx : angle==2 ? -dy : dx;
                bool inside=sx>=0 && sx<32 && sy>=0 && sy<16;
                for (int c=0;c<3;c++) {
                    int got=output[(y*64+x)*3+c];
                    if (!inside) { if (got!=0xa5) goto done; }
                    else {
                        int error=got-reference[(sy*32+sx)*3+c];
                        if (error<0) error=-error;
                        if (error>worst) worst=error;
                    }
                }
                if (inside) checked++;
            }
        }
        if (checked!=512 || worst>3) {
            AIC_TEST_E("FAIL GE I420 rotation=%d pixels=%d max_error=%d",angle*90,checked,worst);
            goto done;
        }
        AIC_TEST_I("PASS GE I420 rotation=%d pixels=512 max_error=%d guards=OK",angle*90,worst);
    }
    /* Publish a new immutable frame: neutral chroma makes the expected output
     * an analytic luma ramp, independent of the production RGB converter and
     * scale geometry helper. Fractional clips test phase rather than only size. */
    lv_aic_yuv_image_destroy(image); image=NULL;
    memset(source+512,128,512);
    image=lv_aic_yuv_image_create(&frame,retain_probe,release_probe,&refs);
    if (!image) goto done;
    d.src=lv_aic_yuv_image_source(image); d.rotation=0;
    task.area=(lv_area_t){8,8,39,23};
    const unsigned scales[]={128,384,512,384,384,384,384};
    for (unsigned probe=0;probe<7;probe++) {
        int scale=scales[probe], offset=probe==3 ? 4 : 0;
        int width=probe==0 ? 16 : 24, height=probe==0 ? 8 : 12;
        int x0=8+offset, y0=8+offset, worst=0, checked=0;
        d.scale_x=d.scale_y=scale;
        task.clip_area=(lv_area_t){x0,y0,x0+width-1,y0+height-1};
        if (probe>=4) {
            lv_point_t p[4]={{2,2},{26,2},{2,12},{26,12}};
            d.rotation=(probe-3)*900; d.scale_y=512; d.pivot=(lv_point_t){16,8};
            task.area=(lv_area_t){16,16,47,31};
            lv_point_array_transform(p,4,d.rotation,d.scale_x,d.scale_y,&d.pivot,true);
            task.clip_area=(lv_area_t){p[0].x,p[0].y,p[0].x,p[0].y};
            for(unsigned i=1;i<4;i++) {
                if(p[i].x<task.clip_area.x1) task.clip_area.x1=p[i].x;
                if(p[i].x>task.clip_area.x2) task.clip_area.x2=p[i].x;
                if(p[i].y<task.clip_area.y1) task.clip_area.y1=p[i].y;
                if(p[i].y>task.clip_area.y2) task.clip_area.y2=p[i].y;
            }
            lv_area_move(&task.clip_area,16,16);
            x0=task.clip_area.x1; y0=task.clip_area.y1;
            width=lv_area_get_width(&task.clip_area); height=lv_area_get_height(&task.clip_area);
        }
        memset(output,0xa5,64*64*3);
        aicos_dcache_clean_invalid_range((unsigned long *)output,64*64*3);
        lv_draw_aic_ge2d_outcome_t outcome;
        AIC_TEST_I("BEGIN GE I420 sx=%d sy=%d rotation=%d",scale,d.scale_y,d.rotation/10);
        if (lv_draw_aic_ge2d_image(&task,&outcome)!=LV_RESULT_OK ||
            outcome!=LV_DRAW_AIC_GE2D_OUTCOME_ENGINE) goto done;
        aicos_dcache_invalid_range((unsigned long *)output,64*64*3);
        for (int y=0;y<64;y++) {
            for (int x=0;x<64;x++) {
                bool inside=x>=x0 && x<x0+width && y>=y0 && y<y0+height;
                /* BT.601 limited neutral chroma: RGB = (Y - 16) * 255/219.
                 * Y is affine, so bilinear filtering has the same exact ramp. */
                int expected=0;
                if (inside) {
                    int64_t numerator=(int64_t)(5*(x-8)+3*(y-8))*256*255;
                    int denominator=scale*219;
                    if(probe>=4) {
                        int dx=x-32, dy=y-24;
                        int u=d.rotation==900 ? dy : d.rotation==1800 ? -dx : -dy;
                        int v=d.rotation==900 ? -dx : d.rotation==1800 ? -dy : dx;
                        /* Inverse rotate, then source-axis scales 3/2 and 2.
                         * Six times (5*sx + 3*sy) = 624 + 20*u + 9*v. */
                        numerator=(int64_t)(624+20*u+9*v)*255;
                        denominator=6*219;
                    }
                    expected=(int)((numerator+denominator/2)/denominator);
                    if (expected>255) expected=255;
                    checked++;
                }
                for (int c=0;c<3;c++) {
                    int got=output[(y*64+x)*3+c];
                    if (!inside) { if (got!=0xa5) goto done; }
                    else {
                        int error=got-expected;
                        if (error<0) error=-error;
                        if (error>worst) worst=error;
                    }
                }
            }
        }
        if (checked!=width*height || worst>3) {
            AIC_TEST_E("FAIL GE I420 scale=%d pixels=%d max_error=%d",scale,checked,worst);
            goto done;
        }
        AIC_TEST_I("PASS GE I420 scale=%d pixels=%d max_error=%d guards=OK",scale,checked,worst);
    }
    d.rotation=0; d.scale_x=d.scale_y=LV_SCALE_NONE;
    d.pivot=(lv_point_t){0,0}; d.tile=1;
    task.area=(lv_area_t){0,16,63,47}; task.clip_area=(lv_area_t){2,18,61,45};
    memset(output,0xa5,64*64*3);
    aicos_dcache_clean_invalid_range((unsigned long *)output,64*64*3);
    lv_draw_aic_ge2d_outcome_t tiled_outcome;
    AIC_TEST_I("BEGIN GE I420 tiled clipped 2x2");
    if (lv_draw_aic_ge2d_image(&task,&tiled_outcome)!=LV_RESULT_OK ||
        tiled_outcome!=LV_DRAW_AIC_GE2D_OUTCOME_ENGINE) goto done;
    aicos_dcache_invalid_range((unsigned long *)output,64*64*3);
    int tiled_worst=0;
    for(int y=0;y<64;y++) {
        for(int x=0;x<64;x++) {
            bool inside=x>=2 && x<=61 && y>=18 && y<=45;
            int expected=inside ? ((5*(x%32)+3*((y-16)%16))*255+109)/219 : 0;
            for(int c=0;c<3;c++) {
                int got=output[(y*64+x)*3+c];
                if(!inside) { if(got!=0xa5) goto done; }
                else {
                    int error=got-expected;
                    if(error<0) error=-error;
                    if(error>tiled_worst) tiled_worst=error;
                }
            }
        }
    }
    if(tiled_worst>3) {
        AIC_TEST_E("FAIL GE I420 tiled max_error=%d",tiled_worst);
        goto done;
    }
    AIC_TEST_I("PASS GE I420 tiled pixels=1680 max_error=%d guards=OK",tiled_worst);
    /* Source Y=16+5*x+3*y, neutral chroma. Independent rational inverse
     * mapping checks transformed cells, including seams and untouched gaps. */
    for (unsigned probe=0;probe<2;probe++) {
        d.scale_x=d.scale_y=512; d.rotation=probe ? 900 : 0;
        d.pivot=probe ? (lv_point_t){16,8} : (lv_point_t){0,0};
        task.clip_area=task.area;
        memset(output,0xa5,64*64*3);
        aicos_dcache_clean_invalid_range((unsigned long *)output,64*64*3);
        AIC_TEST_I("BEGIN GE I420 tile scale=512 rotation=%u",probe*90);
        if (lv_draw_aic_ge2d_image(&task,&tiled_outcome)!=LV_RESULT_OK ||
            tiled_outcome!=LV_DRAW_AIC_GE2D_OUTCOME_ENGINE) goto done;
        aicos_dcache_invalid_range((unsigned long *)output,64*64*3);
        tiled_worst=0;
        int checked=0;
        for (int y=0;y<64;y++) {
            for (int x=0;x<64;x++) {
                int rx=x%32, ry=(y-16)%16;
                bool inside=y>=16 && y<=47 && (!probe || rx>=2);
                int expected=0;
                if(inside) {
                    int twice_luma=probe ? 216+5*ry-3*rx : 5*rx+3*ry;
                    expected=(twice_luma*255+219)/438;
                    checked++;
                }
                for (int c=0;c<3;c++) {
                    int got=output[(y*64+x)*3+c];
                    if(!inside) {
                        if(got!=0xa5) {
                            AIC_TEST_E("FAIL tile guard x=%d y=%d rot=%u",x,y,probe*90);
                            goto done;
                        }
                    }
                    else {
                        int error=got-expected;
                        if(error<0) error=-error;
                        if(error>tiled_worst) tiled_worst=error;
                    }
                }
            }
        }
        if(checked!=(probe ? 1920 : 2048) || tiled_worst>3) {
            AIC_TEST_E("FAIL I420 tile rot=%u pixels=%d max_error=%d",probe*90,checked,tiled_worst);
            goto done;
        }
        AIC_TEST_I("PASS I420 tile rot=%u pixels=%d max_error=%d guards=OK",probe*90,checked,tiled_worst);
    }
    /* Colored chroma exposes phase/crop seams hidden by neutral UV. Run
     * whole and split refresh with the same independent BT.601 oracle. */
    d.tile=0;d.pivot=(lv_point_t){0,0};d.scale_y=256;
    task.area=(lv_area_t){32,32,63,47};
    for(unsigned y=0;y<16;y++) for(unsigned x=0;x<32;x++) source[y*32+x]=80+x+2*y;
    for(unsigned y=0;y<8;y++) for(unsigned x=0;x<16;x++) {
        source[512+y*16+x]=80+2*x+y;source[768+y*16+x]=150+x-2*y;
    }
    const lv_area_t stripe_clips[]={{32,32,63,47},{17,32,32,63},{1,17,32,32},{32,1,47,32}};
    for(unsigned zoom=0;zoom<2;zoom++) for(unsigned angle=0;angle<4;angle++)
    for(unsigned opacity=0;opacity<2;opacity++) for(unsigned partial=0;partial<2;partial++) {
        d.scale_x=zoom?281:264;d.rotation=angle*900;d.opa=opacity?128:255;
        task.clip_area=stripe_clips[angle];
        memset(output,0xa5,64*64*3);
        aicos_dcache_clean_invalid_range((unsigned long *)output,64*64*3);
        for(unsigned pass=0;pass<(partial?2U:1U);pass++) {
            task.clip_area=stripe_clips[angle];
            if(partial) {
                if(angle&1) {
                    int middle=(task.clip_area.x1+task.clip_area.x2)/2;
                    if(pass) task.clip_area.x1=middle+1;else task.clip_area.x2=middle;
                } else {
                    int middle=(task.clip_area.y1+task.clip_area.y2)/2;
                    if(pass) task.clip_area.y1=middle+1;else task.clip_area.y2=middle;
                }
            }
            lv_draw_aic_ge2d_outcome_t outcome;
            if(lv_draw_aic_ge2d_image(&task,&outcome)!=LV_RESULT_OK || outcome!=LV_DRAW_AIC_GE2D_OUTCOME_ENGINE) goto done;
        }
        aicos_dcache_invalid_range((unsigned long *)output,64*64*3);
        int worst=0,checked=0;
        const lv_area_t *clip=&stripe_clips[angle];
        for(int y=0;y<64;y++) for(int x=0;x<64;x++) {
            bool inside=x>=clip->x1 && x<=clip->x2 && y>=clip->y1 && y<=clip->y2;
            if(!inside) {
                for(unsigned c=0;c<3;c++) if(output[(y*64+x)*3+c]!=0xa5) goto done;
                continue;
            }
            int dx=x-32,dy=y-32;
            int64_t u=(int64_t)(angle==0?dx:angle==1?dy:angle==2?-dx:-dy)*256*65536/d.scale_x;
            int64_t v=(int64_t)(angle==0?dy:angle==1?-dx:angle==2?-dy:dx)*65536;
            int64_t cx=LV_MIN(u/2,15*INT64_C(65536)),cy=LV_MIN(v/2,7*INT64_C(65536));
            int64_t yy=64*INT64_C(65536)+u+2*v; /* limited-range Y minus 16 */
            int64_t uu=-48*INT64_C(65536)+2*cx+cy,vv=22*INT64_C(65536)+cx-2*cy;
            const int64_t rgb[]={yy*76284+uu*132251,yy*76284-uu*25675-vv*53279,yy*76284+vv*104597};
            for(unsigned c=0;c<3;c++) {
                int want=(int)((rgb[c]+INT64_C(2147483648))/INT64_C(4294967296));
                want=LV_CLAMP(0,want,255);want=(want*d.opa+165*(255-d.opa)+127)/255;
                int delta=output[(y*64+x)*3+c]-want;
                if(delta<0) delta=-delta;
                if(delta>worst) worst=delta;
            }
            checked++;
        }
        if(checked!=512 || worst>4) {
            AIC_TEST_E("FAIL YUV stripes sx=%u rot=%u partial=%u error=%d",(unsigned)d.scale_x,angle*90,partial,worst);goto done;
        }
        AIC_TEST_I("PASS YUV stripes sx=%u rot=%u opa=%u partial=%u pixels=512 error=%d",(unsigned)d.scale_x,angle*90,(unsigned)d.opa,partial,worst);
    }
    /* Packed 4:2:2 source formats absent from LVGL's native enum. */
    lv_aic_yuv_image_destroy(image);image=NULL;
    const lv_aic_yuv_format_t packed_formats[]={LV_AIC_YUV_YVYU,LV_AIC_YUV_VYUY};
    for(unsigned f=0;f<2;f++) for(unsigned space=0;space<4;space++) {
        frame.format=packed_formats[f];frame.color_space=space;
        frame.planes[0]=(lv_aic_yuv_plane_t){source,64,1024};
        for(unsigned y=0;y<16;y++) for(unsigned x=0;x<32;x+=2) {
            uint8_t *p=source+y*64+x*2;
            uint8_t y0=40+3*x+y,y1=y0+3;
            if(!f) { p[0]=y0;p[1]=180;p[2]=y1;p[3]=90; }
            else { p[0]=180;p[1]=y0;p[2]=90;p[3]=y1; }
        }
        if(!lv_aic_yuv_to_rgb888(&frame,reference,96,32*16*3)) goto done;
        image=lv_aic_yuv_image_create(&frame,retain_probe,release_probe,&refs);
        if(!image) goto done;
        lv_draw_image_dsc_init(&d);d.src=lv_aic_yuv_image_source(image);d.pivot=(lv_point_t){0,0};
        if(lv_image_decoder_get_info(d.src,&d.header)!=LV_RESULT_OK) goto done;
        task.area=(lv_area_t){8,8,39,23};task.clip_area=(lv_area_t){10,10,37,21};
        memset(output,0xa5,64*64*3);
        aicos_dcache_clean_invalid_range((unsigned long *)output,64*64*3);
        lv_draw_aic_ge2d_outcome_t outcome;
        if(lv_draw_aic_ge2d_image(&task,&outcome)!=LV_RESULT_OK ||
           outcome!=LV_DRAW_AIC_GE2D_OUTCOME_ENGINE) goto done;
        aicos_dcache_invalid_range((unsigned long *)output,64*64*3);
        int worst=0;
        for(int y=0;y<64;y++) for(int x=0;x<64;x++) for(int c=0;c<3;c++) {
            bool inside=x>=10 && x<=37 && y>=10 && y<=21;
            int got=output[(y*64+x)*3+c];
            if(!inside) { if(got!=0xa5) goto done; }
            else {
                int error=got-reference[((y-8)*32+x-8)*3+c];
                if(error<0) error=-error;
                if(error>worst) worst=error;
            }
        }
        if(worst>3) {
            AIC_TEST_E("FAIL packed YUV fmt=%u space=%u error=%d",(unsigned)frame.format,space,worst);goto done;
        }
        AIC_TEST_I("PASS packed YUV fmt=%u space=%u error=%d guards=OK",(unsigned)frame.format,space,worst);
        lv_aic_yuv_image_destroy(image);image=NULL;
    }
    result=0;
done:
    if (image) lv_aic_yuv_image_destroy(image);
    if (lv_draw_aic_ge2d_yuv_faulted()) {
        /* Both DMA allocations and producer callback storage survive until
         * reboot. No free/decoder teardown can establish hardware quiescence. */
        lv_free(reference);
        AIC_TEST_E("FAIL YUV DMA; retaining source/destination until reboot");
        return -1;
    }
    if (registered && !lv_aic_yuv_image_decoder_deinit()) result=-1;
    if (refs) result=-1;
    if (source) aicos_free_align(MEM_CMA,source);
    if (output) aicos_free_align(MEM_CMA,output);
    lv_free(reference);
    if (result) AIC_TEST_E("FAIL GE I420 probe");
    return result;
}
static int image_probe(const lv_aic_yuv_frame_t *frame)
{
    int refs=0, result=-1;
    lv_image_decoder_dsc_t reader={0};
    bool opened=false;
    lv_aic_yuv_image_t *image=NULL;
    if (!lv_aic_yuv_image_decoder_init()) return -1;
    image=lv_aic_yuv_image_create(frame,retain_probe,release_probe,&refs);
    if (!image || refs!=1) goto done;
    if (lv_image_decoder_open(&reader,lv_aic_yuv_image_source(image),NULL)!=LV_RESULT_OK) goto done;
    opened=true;
    lv_aic_yuv_image_destroy(image); image=NULL;
    if (refs!=1 || lv_aic_yuv_image_decoder_deinit()) goto done;
    /* Source is Y=81,U=90,V=240 in BT.709 full range. */
    if (!reader.decoded || reader.decoded->data[0]!=10 ||
        reader.decoded->data[1]!=36 || reader.decoded->data[2]!=255) goto done;
    result=0;
done:
    if (image) lv_aic_yuv_image_destroy(image);
    if (opened) lv_image_decoder_close(&reader);
    if (refs!=0 || !lv_aic_yuv_image_decoder_deinit()) result=-1;
    if (!result) AIC_TEST_I("PASS YUV image decoder pixels and deferred producer release");
    return result;
}

int lv_aic_yuv_test_run(void)
{
    uint8_t y[24], u[8], v[8], rgb[48];
    /* Reference values from the four explicit standard matrices, BGR order. */
    const uint8_t expected[4][3] = {{0,0,254},{0,24,255},{14,14,238},{10,36,255}};
    lv_aic_yuv_frame_t frame = {0};
    frame.format=LV_COLOR_FORMAT_I420; frame.width=3; frame.height=3;
    frame.planes[0]=(lv_aic_yuv_plane_t){y,8,sizeof(y)};
    frame.planes[1]=(lv_aic_yuv_plane_t){u,4,sizeof(u)};
    frame.planes[2]=(lv_aic_yuv_plane_t){v,4,sizeof(v)};
    memset(y,81,sizeof(y)); memset(u,90,sizeof(u)); memset(v,240,sizeof(v));
    for (unsigned space=0;space<4;space++) {
        frame.color_space=space;
        memset(rgb,0xa5,sizeof(rgb));
        if (!lv_aic_yuv_to_rgb888(&frame,rgb,16,sizeof(rgb))) goto fail;
        for (unsigned row=0;row<3;row++) {
            for (unsigned byte=0;byte<16;byte++) {
                if (rgb[row*16+byte] != (byte<9 ? expected[space][byte%3] : 0xa5)) goto fail;
            }
        }
        struct mpp_buf buf;
        if (lv_aic_yuv_to_mpp(&frame,0,&buf)) goto fail; /* HAL would truncate odd sizes. */
        frame.width=2; frame.height=2;
        if (!lv_aic_yuv_to_mpp(&frame,0,&buf) || buf.format!=MPP_FMT_YUV420P ||
            buf.phy_addr[1]!=(uint32_t)(uintptr_t)u || buf.stride[1]!=4) goto fail;
        frame.width=3; frame.height=3;
    }
    AIC_TEST_I("PASS CPU YUV matrices=4 odd I420 pixels=36 guards=OK; GE DMA not tested");
    if (image_probe(&frame)) goto fail;
    if (ge_probe()) goto fail;
    return 0;
fail:
    AIC_TEST_E("FAIL YUV frame/CPU conversion contract");
    return -1;
}
#endif
