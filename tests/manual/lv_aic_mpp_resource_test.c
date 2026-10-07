/* SPDX-License-Identifier: Apache-2.0
 * Resource-stage board probes: file/memory pixel parity and cache ownership.
 * Only the serialized LVGL owner may run these finite checks. */
#include "lv_aic_manual_test.h"
#if AIC_LVGL_USE_MPP_DEC && AIC_LVGL_BSP_RTTHREAD
#define AIC_LVGL_USE_PRIVATE_API 1
#define LOG_TAG "lvgl.mpp.resource"
#define LOG_LVL LOG_LVL_INFO
#include "lvgl_aic_private.h"
#include "lv_aic_mpp_decoder.h"
#include "lv_aic_mpp_stream.h"
#include "lv_aic_test_log.h"

static uint32_t pixel_hash(const lv_draw_buf_t *b)
{
    uint32_t hash = 2166136261U;
    uint32_t row_bytes = b->header.w * lv_color_format_get_size(b->header.cf);
    for (uint32_t y=0; y<b->header.h; y++) {
        const uint8_t *p = b->data + y*b->header.stride;
        for (uint32_t x=0; x<row_bytes; x++) hash = (hash ^ p[x]) * 16777619U;
    }
    return hash;
}

static int resource_probe(const char *path, bool stress)
{
    lv_aic_mpp_stream_t stream = {0};
    uint8_t *encoded = NULL;
    uint32_t length, read_bytes, expected_hash, width, height, cf;
    uint32_t alloc_before, hit_before;
    lv_image_dsc_t image = {0};
    lv_image_decoder_dsc_t file = {0}, a = {0}, b = {0};
    lv_image_decoder_args_t args = {0};
    bool file_open = false, a_open = false, b_open = false;
    int result = -1;
    if (lv_aic_mpp_stream_open_file(&stream,path) != LV_FS_RES_OK) goto done;
    length = lv_aic_mpp_stream_size(&stream);
    if (!length || length > 8U*1024U*1024U) goto done;
    encoded = lv_malloc(length);
    if (!encoded || lv_aic_mpp_stream_read(&stream,encoded,length,&read_bytes) != LV_FS_RES_OK ||
        read_bytes != length) goto done;
    lv_aic_mpp_stream_close(&stream);
    args.no_cache = true;
    if (lv_image_decoder_open(&file,path,&args) != LV_RESULT_OK) goto done;
    file_open = true;
    if (file.decoder != lv_aic_get_mpp_decoder() || !file.decoded) goto done;
    width=file.decoded->header.w; height=file.decoded->header.h; cf=file.decoded->header.cf;
    expected_hash=pixel_hash(file.decoded);
    lv_image_decoder_close(&file); file_open=false;

    image.header.magic=LV_IMAGE_HEADER_MAGIC; image.header.cf=LV_COLOR_FORMAT_RAW;
    image.data=encoded; image.data_size=length;
    args.no_cache=false;
    if (lv_image_decoder_open(&a,&image,&args) != LV_RESULT_OK) goto done;
    a_open=true;
    if (a.decoder != lv_aic_get_mpp_decoder() || !a.decoded ||
        a.decoded->header.w != width || a.decoded->header.h != height ||
        a.decoded->header.cf != cf || pixel_hash(a.decoded) != expected_hash) goto done;
    alloc_before=lv_aic_mpp_cma_stats()->alloc_count;
    hit_before=lv_aic_mpp_cache_stats()->hits;
    if (lv_image_decoder_open(&b,&image,&args) != LV_RESULT_OK) goto done;
    b_open=true;
    if (a.decoded != b.decoded || lv_aic_mpp_cache_stats()->hits != hit_before+1 ||
        lv_aic_mpp_cma_stats()->alloc_count != alloc_before) goto done;
    lv_aic_mpp_cache_drop(&image);
    lv_image_decoder_close(&a); a_open=false;
    if (pixel_hash(b.decoded) != expected_hash || !lv_aic_mpp_cma_stats()->current_cma_bytes) goto done;
    lv_image_decoder_close(&b); b_open=false;
    if (lv_aic_mpp_cache_stats()->entries || lv_aic_mpp_cma_stats()->current_cma_bytes) goto done;
    AIC_TEST_I("PASS file-memory parity %s", path);
    AIC_TEST_I("size=%ux%u cf=%u hash=%08x active-drop=OK",
          (unsigned)width,(unsigned)height,(unsigned)cf,(unsigned)expected_hash);

    if (stress) {
        if (lv_image_decoder_open(&a,&image,&args) != LV_RESULT_OK) goto done;
        a_open=true; lv_image_decoder_close(&a); a_open=false;
        alloc_before=lv_aic_mpp_cma_stats()->alloc_count;
        hit_before=lv_aic_mpp_cache_stats()->hits;
        for (unsigned i=0;i<100;i++) {
            if (lv_image_decoder_open(&a,&image,&args) != LV_RESULT_OK) goto done;
            a_open=true;
            if (a.decoder != lv_aic_get_mpp_decoder() || pixel_hash(a.decoded) != expected_hash) goto done;
            lv_image_decoder_close(&a); a_open=false;
        }
        if (lv_aic_mpp_cache_stats()->hits != hit_before+100 ||
            lv_aic_mpp_cma_stats()->alloc_count != alloc_before) goto done;
        AIC_TEST_I("PASS cache 100 hits, no new CMA allocation, pixel hashes stable");
    }
    result=0;
done:
    if (file_open) lv_image_decoder_close(&file);
    if (a_open) lv_image_decoder_close(&a);
    if (b_open) lv_image_decoder_close(&b);
    lv_aic_mpp_cache_drop(&image);
    lv_aic_mpp_stream_close(&stream);
    lv_free(encoded);
    if (result) AIC_TEST_E("FAIL resource probe %s",path);
    return result;
}

int lv_aic_mpp_resource_test_run(void)
{
    uint32_t previous_limit=lv_aic_mpp_cache_stats()->limit_bytes;
    int result=-1;
    lv_aic_mpp_cache_drop(NULL);
    lv_aic_mpp_cache_set_limit(4U*1024U*1024U);
    AIC_TEST_I("BEGIN resource stage: memory JPEG/PNG and bounded cache");
    if (resource_probe("L:/data/mpp_test/aic_801x479.jpg",false) ||
        resource_probe("L:/data/mpp_test/b.png",true) ||
        resource_probe("L:/data/mpp_test/c.png",true)) goto done;
    AIC_TEST_I("BEGIN BMP resource probes");
    if (resource_probe("L:/data/mpp_test/probe-16.bmp",true) ||
        resource_probe("L:/data/mpp_test/probe-rgb565.bmp",true) ||
        resource_probe("L:/data/mpp_test/probe-24.bmp",true) ||
        resource_probe("L:/data/mpp_test/probe-32.bmp",true)) goto done;
    AIC_TEST_I("PASS BMP file-memory parity and cache lifecycle");
#ifdef AIC_MPP_AICP_DEC_ENABLE
    AIC_TEST_I("BEGIN AICP resource probes");
    if (resource_probe("L:/data/mpp_test/bird.aicp",true)) goto done;
#ifdef AIC_VE_DRV_V31
    if (resource_probe("L:/data/mpp_test/flower.aicp",true)) goto done;
#else
    AIC_TEST_I("SKIP AICP alpha fixture: requires V31");
#endif
    AIC_TEST_I("PASS AICP file-memory parity and cache lifecycle");
#endif
    if (lv_aic_mpp_cache_stats()->entries || lv_aic_mpp_cma_stats()->current_cma_bytes) goto done;
    AIC_TEST_I("PASS resource stage; file/memory parity and cache lifetime balanced");
    result=0;
done:
    lv_aic_mpp_cache_drop(NULL);
    lv_aic_mpp_cache_set_limit(previous_limit);
    return result;
}
#endif
