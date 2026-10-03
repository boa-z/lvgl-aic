/* SPDX-License-Identifier: Apache-2.0
 * Phase 3C2: real executor + LVGL decode/helper, offscreen CMA output.
 * No global allocator changes. Pixel checks exclude filter boundary taps;
 * the full guard area outside the clip must remain byte-for-byte unchanged. */
#define AIC_LVGL_USE_PRIVATE_API 1
#define LOG_TAG "lvgl.ge2d.scale"
#define LOG_LVL LOG_LVL_INFO
#include "lv_aic_manual_test.h"
#if AIC_LVGL_USE_GE2D && AIC_LVGL_BSP_RTTHREAD
#include "lvgl_aic_private.h"
#include "lv_draw_aic_ge2d.h"
#include <aic_core.h>
#include <aic_osal.h>
#include "lv_aic_test_log.h"

#define SRC_W 32
#define DST_W 96
#define SRC_STRIDE 128
#define DST_STRIDE (DST_W * 4)

static int scale_probe(uint16_t sx, uint16_t sy, bool argb, bool clipped,
                       bool pivoted, bool expect_engine, bool tiled)
{
    uint8_t *source = aicos_malloc_align(MEM_CMA, SRC_STRIDE * SRC_W, 32);
    uint8_t *output = aicos_malloc_align(MEM_CMA, DST_STRIDE * DST_W, 32);
    lv_draw_buf_t src, dst;
    lv_draw_image_dsc_t d;
    lv_layer_t layer = {0};
    lv_draw_task_t task = {0};
    lv_draw_aic_ge2d_outcome_t outcome;
    lv_area_t drawn;
    int result = -1, worst = 0, checked = 0;
    bool submitted = false;
    if (!source || !output) goto done;
    lv_memzero(source, SRC_STRIDE * SRC_W);
    for (int y = 0; y < SRC_W; y++) {
        for (int x = 0; x < SRC_W; x++) {
            uint8_t *p = source + y * SRC_STRIDE + x * (argb ? 4 : 3);
            p[0] = 40 + 2*x + 2*y; p[1] = 30 + 5*y; p[2] = 20 + 6*x;
            if (argb) p[3] = 128;
        }
    }
    for (int i = 0; i < DST_W * DST_W; i++) {
        output[i*4] = 240; output[i*4+1] = 160;
        output[i*4+2] = 16; output[i*4+3] = 255;
    }
    if (lv_draw_buf_init(&src, SRC_W, SRC_W,
                         argb ? LV_COLOR_FORMAT_ARGB8888 : LV_COLOR_FORMAT_RGB888,
                         SRC_STRIDE, source, SRC_STRIDE * SRC_W) != LV_RESULT_OK ||
        lv_draw_buf_init(&dst, DST_W, DST_W, LV_COLOR_FORMAT_ARGB8888,
                         DST_STRIDE, output, DST_STRIDE * DST_W) != LV_RESULT_OK) goto done;
    lv_draw_image_dsc_init(&d);
    d.src = &src; d.scale_x = sx; d.scale_y = sy;
    d.header = src.header;
    d.tile = tiled;
    d.pivot = (lv_point_t){pivoted ? 7 : 0, pivoted ? 9 : 0};
    d.opa = argb ? 128 : LV_OPA_COVER;
    /* Nonzero layer origin catches accidental display-vs-buffer coordinates. */
    layer.draw_buf = &dst;
    layer.buf_area = (lv_area_t){100, 200, 100 + DST_W - 1, 200 + DST_W - 1};
    task.target_layer = &layer; task.type = LV_DRAW_TASK_TYPE_IMAGE;
    task.draw_dsc = &d; task.area = (lv_area_t){124, 224, 155, 255};
    task.clip_area = clipped ? (lv_area_t){125, 227, 144, 239} : layer.buf_area;
    if (tiled) {
        d.image_area = task.area;
        task.area = (lv_area_t){124, 224, 187, 287};
        task.clip_area = (lv_area_t){125, 227, 180, 280};
    }
    aicos_dcache_clean_range((unsigned long *)source, SRC_STRIDE * SRC_W);
    aicos_dcache_clean_invalid_range((unsigned long *)output, DST_STRIDE * DST_W);
    submitted = true;
    if (lv_draw_aic_ge2d_image(&task, &outcome) != LV_RESULT_OK) goto done;
    if (!expect_engine) {
        if (outcome != LV_DRAW_AIC_GE2D_OUTCOME_SOFTWARE) goto done;
        AIC_TEST_I("PASS scale fallback sx=%u sy=%u", (unsigned)sx, (unsigned)sy);
        result = 0;
        goto done;
    }
    if (outcome != LV_DRAW_AIC_GE2D_OUTCOME_ENGINE) {
        AIC_TEST_E("FAIL expected engine, got outcome=%d", (int)outcome);
        goto done;
    }
    aicos_dcache_invalid_range((unsigned long *)output, DST_STRIDE * DST_W);
    lv_image_buf_get_transformed_area(&drawn, SRC_W, SRC_W, 0, sx, sy, &d.pivot);
    lv_area_move(&drawn, task.area.x1, task.area.y1);
    if (tiled) drawn = task.area;
    if (!lv_area_intersect(&drawn, &drawn, &task.clip_area)) goto done;
    for (int y = 0; y < DST_W; y++) {
        for (int x = 0; x < DST_W; x++) {
            const uint8_t *p = output + y * DST_STRIDE + x * 4;
            int ax = x + 100, ay = y + 200;
            int64_t u, v;
            int expected[3], alpha = argb ? LV_OPA_MIX2(128,128) : 255;
            const int background[3] = {240,160,16};
            if (ax < drawn.x1 || ax > drawn.x2 || ay < drawn.y1 || ay > drawn.y2) {
                if (p[0] != 240 || p[1] != 160 || p[2] != 16 || p[3] != 255) {
                    AIC_TEST_E("FAIL clip guard x=%d y=%d", x, y);
                    goto done;
                }
                continue;
            }
            /* Independent rational mapping, not the production phase helper.
             * Linear colour ramps test both axes and fractional clip origins. */
            u = ((int64_t)(ax-124-d.pivot.x)*256*65536)/sx + (int64_t)d.pivot.x*65536;
            v = ((int64_t)(ay-224-d.pivot.y)*256*65536)/sy + (int64_t)d.pivot.y*65536;
            if (tiled) {
                u = ((ax - d.image_area.x1) % SRC_W) * 65536;
                v = ((ay - d.image_area.y1) % SRC_W) * 65536;
            }
            else if (u < 3*65536 || v < 3*65536 || u > 28*65536 || v > 28*65536) continue;
            expected[0] = 40 + (int)((2*u+2*v+32768)/65536);
            expected[1] = 30 + (int)((5*v+32768)/65536);
            expected[2] = 20 + (int)((6*u+32768)/65536);
            for (int c = 0; c < 3; c++) {
                int want = (expected[c]*alpha + background[c]*(255-alpha)+127)/255;
                int error = (int)p[c] - want;
                if (error < 0) error = -error;
                if (error > worst) worst = error;
            }
            checked++;
        }
    }
    if (checked < 16 || worst > 3) {
        AIC_TEST_E("FAIL scale pixels checked=%d max_error=%d", checked, worst);
        goto done;
    }
    AIC_TEST_I("PASS pixels=%d max_error=%d clip_guard=OK engine=1", checked, worst);
    result = 0;
done:
    if (submitted) lv_image_cache_drop(&src);
    if (source) aicos_free_align(MEM_CMA, source);
    if (output) aicos_free_align(MEM_CMA, output);
    if (result != 0) AIC_TEST_E("FAIL scale sx=%u sy=%u argb=%d clip=%d pivot=%d",
                          (unsigned)sx, (unsigned)sy, argb, clipped, pivoted);
    return result;
}

int lv_aic_ge2d_scale_test_run(void)
{
    const uint16_t ratios[] = {128,384,512};
    for (int argb = 0; argb < 2; argb++) {
        for (unsigned i = 0; i < 3; i++) {
            AIC_TEST_I("BEGIN sx=%u sy=%u argb=%d", (unsigned)ratios[i], (unsigned)ratios[i], argb);
            if (scale_probe(ratios[i], ratios[i], argb, false, false, true, false)) return -1;
        }
    }
    AIC_TEST_I("BEGIN nonuniform sx=384 sy=192 RGB");
    if (scale_probe(384,192,false,false,false,true,false)) return -1;
    AIC_TEST_I("BEGIN nonuniform clipped pivot RGB");
    if (scale_probe(384,192,false,true,true,true,false)) return -1;
    AIC_TEST_I("BEGIN nonuniform clipped pivot ARGB");
    if (scale_probe(384,192,true,true,true,true,false)) return -1;
    if (scale_probe(15,256,false,false,false,false,false)) return -1;
    if (scale_probe(4097,256,false,true,false,false,false)) return -1;
    if (scale_probe(264,256,false,false,false,false,false)) return -1;
    for (int argb = 0; argb < 2; argb++) {
        AIC_TEST_I("BEGIN native tile argb=%d", argb);
        if (scale_probe(256,256,argb,true,false,true,true)) return -1;
    }
    AIC_TEST_I("PASS 3C2 numeric probes; panel edges/touch still require confirmation");
    return 0;
}
#endif
