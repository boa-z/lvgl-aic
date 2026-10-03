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
#include "lv_aic_test_log.h"
#include <string.h>

static bool retain_probe(void *context) { (*(int *)context)++; return true; }
static void release_probe(void *context) { (*(int *)context)--; }
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
    return 0;
fail:
    AIC_TEST_E("FAIL YUV frame/CPU conversion contract");
    return -1;
}
#endif
