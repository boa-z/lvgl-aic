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
