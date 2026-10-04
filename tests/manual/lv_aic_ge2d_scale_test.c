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

static int scale_probe(uint16_t sx, uint16_t sy, unsigned source_kind, bool clipped,
                       bool pivoted, bool expect_engine, bool tiled, uint16_t rotation)
{
    bool argb = source_kind != 0, premult = source_kind >= 2;
    uint8_t *source = aicos_malloc_align(MEM_CMA, SRC_STRIDE * SRC_W, 32);
    uint8_t *output = aicos_malloc_align(MEM_CMA, DST_STRIDE * DST_W, 32);
    lv_draw_buf_t src, dst;
    lv_draw_image_dsc_t d;
    lv_layer_t layer = {0};
    lv_draw_task_t task = {0};
    lv_draw_aic_ge2d_outcome_t outcome;
    lv_area_t drawn;
    int result = -1, worst = 0, checked = 0;
    bool submitted = false, unsafe_dma = false;
    if (!source || !output) goto done;
    lv_memzero(source, SRC_STRIDE * SRC_W);
    for (int y = 0; y < SRC_W; y++) {
        for (int x = 0; x < SRC_W; x++) {
            uint8_t *p = source + y * SRC_STRIDE + x * (argb ? 4 : 3);
            p[0] = 40 + 2*x + 2*y; p[1] = 30 + 5*y; p[2] = 20 + 6*x;
            if (argb) p[3] = 128;
            if (premult) for(unsigned c=0;c<3;c++) p[c]=(p[c]*128U+127U)/255U;
        }
    }
    for (int i = 0; i < DST_W * DST_W; i++) {
        output[i*4] = 240; output[i*4+1] = 160;
        output[i*4+2] = 16; output[i*4+3] = 255;
    }
    if (lv_draw_buf_init(&src, SRC_W, SRC_W,
                         source_kind==2 ? LV_COLOR_FORMAT_ARGB8888_PREMULTIPLIED :
                         argb ? LV_COLOR_FORMAT_ARGB8888 : LV_COLOR_FORMAT_RGB888,
                         SRC_STRIDE, source, SRC_STRIDE * SRC_W) != LV_RESULT_OK ||
        lv_draw_buf_init(&dst, DST_W, DST_W, LV_COLOR_FORMAT_ARGB8888,
                         DST_STRIDE, output, DST_STRIDE * DST_W) != LV_RESULT_OK) goto done;
    if (source_kind==3) src.header.flags |= LV_IMAGE_FLAGS_PREMULTIPLIED;
    lv_draw_image_dsc_init(&d);
    d.src = &src; d.scale_x = sx; d.scale_y = sy;
    d.header = src.header;
    d.tile = tiled; d.rotation = rotation;
    d.pivot = (lv_point_t){pivoted ? 7 : 0, pivoted ? 9 : 0};
    d.opa = argb ? 128 : LV_OPA_COVER;
    /* Nonzero layer origin catches accidental display-vs-buffer coordinates. */
    layer.draw_buf = &dst;
    layer.color_format = LV_COLOR_FORMAT_ARGB8888;
    layer.buf_area = (lv_area_t){100, 200, 100 + DST_W - 1, 200 + DST_W - 1};
    task.target_layer = &layer; task.type = LV_DRAW_TASK_TYPE_IMAGE;
    task.draw_dsc = &d; task.area = (lv_area_t){124, 224, 155, 255};
    task.clip_area = clipped ? (lv_area_t){125, 227, 144, 239} : layer.buf_area;
    if (tiled) {
        if (sx != 256 || sy != 256 || rotation) d.pivot = (lv_point_t){16,16};
        d.image_area = task.area;
        task.area = (lv_area_t){124, 224, 187, 287};
        task.clip_area = (lv_area_t){125, 227, 180, 280};
    }
    aicos_dcache_clean_range((unsigned long *)source, SRC_STRIDE * SRC_W);
    aicos_dcache_clean_invalid_range((unsigned long *)output, DST_STRIDE * DST_W);
    submitted = true;
    if (lv_draw_aic_ge2d_image(&task, &outcome) != LV_RESULT_OK) {
        /* A failed submission/emit/sync cannot prove hardware quiescence. */
        unsafe_dma=true; goto done;
    }
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
    lv_image_buf_get_transformed_area(&drawn, SRC_W, SRC_W, rotation, sx, sy, &d.pivot);
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
            int local_x = tiled ? (ax-d.image_area.x1)%SRC_W : ax-124;
            int local_y = tiled ? (ay-d.image_area.y1)%SRC_W : ay-224;
            int dx=local_x-d.pivot.x, dy=local_y-d.pivot.y;
            int inverse_x=rotation==900 ? dy : rotation==1800 ? -dx : rotation==2700 ? -dy : dx;
            int inverse_y=rotation==900 ? -dx : rotation==1800 ? -dy : rotation==2700 ? dx : dy;
            u=((int64_t)inverse_x*256*65536)/sx+(int64_t)d.pivot.x*65536;
            v=((int64_t)inverse_y*256*65536)/sy+(int64_t)d.pivot.y*65536;
            if (!tiled && (u < 3*65536 || v < 3*65536 || u > 28*65536 || v > 28*65536)) continue;
            if (p[3] != 255) {
                AIC_TEST_E("FAIL destination alpha x=%d y=%d value=%u",x,y,(unsigned)p[3]);
                goto done;
            }
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
    if (unsafe_dma) {
        AIC_TEST_E("FAIL GE scale execution; retaining CMA buffers until reboot");
        return -1;
    }
    if (source) aicos_free_align(MEM_CMA, source);
    if (output) aicos_free_align(MEM_CMA, output);
    if (result != 0) AIC_TEST_E("FAIL scale sx=%u sy=%u argb=%d clip=%d pivot=%d",
                          (unsigned)sx, (unsigned)sy, argb, clipped, pivoted);
    return result;
}

/* Compare real GE output with native SW from the same premultiplied bytes.
 * Includes alpha zero, partial and opaque pixels, clip guards and global alpha. */
static int premult_probe(bool flagged, lv_opa_t opacity)
{
    const unsigned bytes=16*16*4;
    uint32_t *source=aicos_malloc_align(MEM_CMA,bytes,32);
    uint32_t *output=aicos_malloc_align(MEM_CMA,bytes,32);
    uint32_t *reference=aicos_malloc_align(MEM_CMA,bytes,32);
    lv_draw_buf_t src,dst,ref;
    lv_layer_t layer={0},ref_layer={0};
    lv_draw_task_t task={0},ref_task={0};
    lv_draw_image_dsc_t d;
    bool submitted=false;
    int result=-1,worst=0;
    if(!source || !output || !reference) goto done;
    const unsigned alphas[]={0,64,128,255};
    for(unsigned i=0;i<256;i++) {
        unsigned a=alphas[i%4];
        source[i]=(a<<24)|(a<<16)|((a/2)<<8)|(a/4);
        output[i]=reference[i]=0xff103050;
    }
    if(lv_draw_buf_init(&src,16,16,flagged?LV_COLOR_FORMAT_ARGB8888:
        LV_COLOR_FORMAT_ARGB8888_PREMULTIPLIED,64,source,bytes)!=LV_RESULT_OK ||
       lv_draw_buf_init(&dst,16,16,LV_COLOR_FORMAT_ARGB8888,64,output,bytes)!=LV_RESULT_OK ||
       lv_draw_buf_init(&ref,16,16,LV_COLOR_FORMAT_ARGB8888,64,reference,bytes)!=LV_RESULT_OK) goto done;
    if(flagged) src.header.flags|=LV_IMAGE_FLAGS_PREMULTIPLIED;
    layer.draw_buf=&dst;layer.color_format=LV_COLOR_FORMAT_ARGB8888;
    layer.buf_area=(lv_area_t){0,0,15,15};
    lv_draw_image_dsc_init(&d);d.src=&src;d.header=src.header;
    d.image_area=layer.buf_area;d.opa=opacity;
    task.type=LV_DRAW_TASK_TYPE_IMAGE;task.draw_dsc=&d;task.target_layer=&layer;
    task.area=layer.buf_area;task.clip_area=(lv_area_t){3,2,12,13};
    aicos_dcache_clean_range((unsigned long *)source,bytes);
    aicos_dcache_clean_invalid_range((unsigned long *)output,bytes);
    submitted=true;
    lv_draw_aic_ge2d_outcome_t outcome;
    if(lv_draw_aic_ge2d_image(&task,&outcome)!=LV_RESULT_OK) {
        lv_image_cache_drop(&src);
        AIC_TEST_E("FAIL premult DMA status; retaining probe storage until reboot");
        return -1;
    }
    if(outcome!=LV_DRAW_AIC_GE2D_OUTCOME_ENGINE) goto done;
    aicos_dcache_invalid_range((unsigned long *)output,bytes);
    ref_layer=layer;ref_layer.draw_buf=&ref;
    ref_task=task;ref_task.target_layer=&ref_layer;
    lv_draw_sw_image(&ref_task,&d,&ref_task.area);
    for(unsigned y=0;y<16;y++) for(unsigned x=0;x<16;x++) {
        if(x<3 || x>12 || y<2 || y>13) {
            if(output[y*16+x]!=0xff103050) goto done;
        }
        for(unsigned c=0;c<4;c++) {
            int delta=((const uint8_t *)output)[y*64+x*4+c]-((const uint8_t *)reference)[y*64+x*4+c];
            if(delta<0) delta=-delta;
            if(delta>worst) worst=delta;
        }
    }
    if(worst>3) goto done;
    result=0;
done:
    if(submitted) lv_image_cache_drop(&src);
    if(source) aicos_free_align(MEM_CMA,source);
    if(output) aicos_free_align(MEM_CMA,output);
    if(reference) aicos_free_align(MEM_CMA,reference);
    if(result==0) AIC_TEST_I("PASS premult flagged=%d opa=%u max_error=%d guards=OK engine=1",flagged,(unsigned)opacity,worst);
    else AIC_TEST_E("FAIL premult flagged=%d opa=%u max_error=%d",flagged,(unsigned)opacity,worst);
    return result;
}

int lv_aic_ge2d_scale_test_run(void)
{
    const lv_opa_t opacities[]={64,128,255};
    for(unsigned flagged=0;flagged<2;flagged++) for(unsigned i=0;i<3;i++) {
        AIC_TEST_I("BEGIN premult flagged=%u opa=%u",flagged,(unsigned)opacities[i]);
        if(premult_probe(flagged!=0,opacities[i])) return -1;
    }
    const uint16_t ratios[] = {128,384,512};
    for (int argb = 0; argb < 4; argb++) {
        for (unsigned i = 0; i < 3; i++) {
            AIC_TEST_I("BEGIN sx=%u sy=%u source_kind=%d", (unsigned)ratios[i], (unsigned)ratios[i], argb);
            if (scale_probe(ratios[i], ratios[i], argb, false, false, true, false,0)) return -1;
        }
    }
    AIC_TEST_I("BEGIN nonuniform sx=384 sy=192 RGB");
    if (scale_probe(384,192,false,false,false,true,false,0)) return -1;
    AIC_TEST_I("BEGIN nonuniform clipped pivot RGB");
    if (scale_probe(384,192,false,true,true,true,false,0)) return -1;
    AIC_TEST_I("BEGIN nonuniform clipped pivot ARGB");
    if (scale_probe(384,192,true,true,true,true,false,0)) return -1;
    if (scale_probe(15,256,false,false,false,false,false,0)) return -1;
    if (scale_probe(4097,256,false,true,false,false,false,0)) return -1;
    if (scale_probe(264,256,false,false,false,false,false,0)) return -1;
    for (int argb = 0; argb < 4; argb++) {
        AIC_TEST_I("BEGIN native tile source_kind=%d", argb);
        if (scale_probe(256,256,argb,true,false,true,true,0)) return -1;
    }
    for (int argb=0;argb<4;argb++) {
        for (unsigned angle=0;angle<3600;angle+=900) {
            AIC_TEST_I("BEGIN tile sx=384 sy=512 rot=%u source_kind=%d",angle/10,argb);
            if (scale_probe(384,512,argb,true,true,true,true,(uint16_t)angle)) return -1;
        }
    }
    AIC_TEST_I("PASS 3C2 numeric probes; panel edges/touch still require confirmation");
    return 0;
}
#endif
