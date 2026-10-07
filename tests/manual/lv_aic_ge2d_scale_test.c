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

static int scale_probe(uint16_t sx, uint16_t sy, unsigned source_kind, unsigned clipped,
                       bool pivoted, bool expect_engine, bool tiled, uint16_t rotation)
{
    bool argb = source_kind != 0, premult = source_kind >= 2;
    uint8_t *source = aicos_malloc_align(MEM_CMA, SRC_STRIDE * SRC_W, 32);
    uint8_t *output = aicos_malloc_align(MEM_CMA, DST_STRIDE * DST_W, 32);
    lv_draw_buf_t src, dst;
    lv_draw_image_dsc_t d;
    lv_layer_t layer = {0};
    lv_draw_task_t task = {0};
    lv_draw_aic_ge2d_outcome_t outcome = LV_DRAW_AIC_GE2D_OUTCOME_NOTHING;
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
    if(clipped==2) {
        /* Full valid 32-sample scaler interval: two balanced commands, while
         * discarding fractional outer samples that independently need SW. */
        d.pivot=(lv_point_t){16,16};
        const lv_area_t stripes[]={{124,224,155,255},{125,224,156,255},
                                   {125,225,156,256},{124,225,155,256}};
        task.clip_area=stripes[rotation/900];
    }
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
        if (outcome != LV_DRAW_AIC_GE2D_OUTCOME_SOFTWARE) {
            AIC_TEST_E("FAIL expected software fallback, got outcome=%d", (int)outcome);
            goto done;
        }
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
            if(rotation % 900) {
                /* Independent Q15 inverse oracle for integral-degree board
                 * probes; production resampling uses explicit Q16 phases. */
                int64_t sn=lv_trigo_sin(rotation/10),cs=lv_trigo_sin(rotation/10+90);
                u=((dx*cs+dy*sn)*512)/sx+(int64_t)d.pivot.x*65536;
                v=((-dx*sn+dy*cs)*512)/sy+(int64_t)d.pivot.y*65536;
            }
            if ((!tiled || rotation % 900) &&
                (u < 4*65536 || v < 4*65536 || u > 27*65536 || v > 27*65536)) continue;
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
    if (result != 0) AIC_TEST_E("FAIL scale sx=%u sy=%u argb=%d clip=%u pivot=%d outcome=%d",
                          (unsigned)sx, (unsigned)sy, argb, clipped, pivoted, (int)outcome);
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

/* Native SW oracle for CPU recolor followed by actual GE work. Constant
 * quadrants cover native pixels; transformed comparisons avoid filter edges. */
static int recolor_probe(unsigned kind,unsigned transform,bool alpha_target)
{
    const lv_color_format_t formats[]={LV_COLOR_FORMAT_RGB565,LV_COLOR_FORMAT_RGB888,
        LV_COLOR_FORMAT_XRGB8888,LV_COLOR_FORMAT_ARGB8888,
        LV_COLOR_FORMAT_ARGB8888_PREMULTIPLIED,LV_COLOR_FORMAT_ARGB8888};
    uint8_t *source=aicos_malloc_align(MEM_CMA,SRC_STRIDE*SRC_W,64);
    uint32_t *output=aicos_malloc_align(MEM_CMA,DST_STRIDE*DST_W,64);
    uint32_t *reference=lv_malloc(DST_STRIDE*DST_W);
    lv_draw_buf_t src,dst,ref;lv_layer_t layer={0},ref_layer={0};
    lv_draw_image_dsc_t d;lv_draw_task_t task={0};
    bool submitted=false,unsafe_dma=false;int result=-1,worst=0,checked=0;
    if(!source || !output || !reference) goto done;
    lv_memset(source,0xa5,SRC_STRIDE*SRC_W);
    const uint32_t colors[]={0x00ff00ff,0x40d04020,0x8020b050,0xff9050c0};
    unsigned bpp=lv_color_format_get_size(formats[kind]);
    for(unsigned y=0;y<SRC_W;y++) for(unsigned x=0;x<SRC_W;x++) {
        uint32_t color=colors[(x>=16)+2*(y>=16)];unsigned a=kind>=3?color>>24:255;
        uint8_t *pixel=source+y*SRC_STRIDE+x*bpp;
        if(bpp==2) {
            uint16_t v=(((color>>16)&255)>>3)<<11|(((color>>8)&255)>>2)<<5|(color&255)>>3;
            lv_memcpy(pixel,&v,2);
        } else {
            for(unsigned c=0;c<3;c++) pixel[c]=kind>=4?(((color>>(8*c))&255)*a+127)/255:(color>>(8*c))&255;
            if(bpp==4) pixel[3]=a;
        }
    }
    const uint32_t backgrounds[]={0x00102030,0x40205090,0x80306020,0xff103050};
    for(unsigned i=0;i<DST_W*DST_W;i++)
        output[i]=reference[i]=alpha_target?backgrounds[(i%DST_W)/24]:0xff102030;
    lv_color_format_t target_cf=alpha_target?LV_COLOR_FORMAT_ARGB8888:LV_COLOR_FORMAT_XRGB8888;
    if(lv_draw_buf_init(&src,SRC_W,SRC_W,formats[kind],SRC_STRIDE,source,SRC_STRIDE*SRC_W)!=LV_RESULT_OK ||
       lv_draw_buf_init(&dst,DST_W,DST_W,target_cf,DST_STRIDE,output,DST_STRIDE*DST_W)!=LV_RESULT_OK ||
       lv_draw_buf_init(&ref,DST_W,DST_W,target_cf,DST_STRIDE,reference,DST_STRIDE*DST_W)!=LV_RESULT_OK) goto done;
    if(kind==5) src.header.flags|=LV_IMAGE_FLAGS_PREMULTIPLIED;
    lv_draw_image_dsc_init(&d);d.src=&src;d.header=src.header;
    d.recolor=lv_color_make(220,45,150);d.recolor_opa=128;d.opa=128;d.pivot=(lv_point_t){7,9};
    if(transform) { d.scale_x=384;d.scale_y=320;d.rotation=transform==2?330:0; }
    layer.draw_buf=&dst;layer.color_format=target_cf;
    layer.buf_area=(lv_area_t){90,190,185,285};
    task.type=LV_DRAW_TASK_TYPE_IMAGE;task.draw_dsc=&d;task.target_layer=&layer;
    task.area=(lv_area_t){120,220,151,251};task.clip_area=transform==1?task.area:layer.buf_area;
    aicos_dcache_clean_range((unsigned long *)source,SRC_STRIDE*SRC_W);
    aicos_dcache_clean_invalid_range((unsigned long *)output,DST_STRIDE*DST_W);
    submitted=true;lv_draw_aic_ge2d_outcome_t outcome;
    if(lv_draw_aic_ge2d_image(&task,&outcome)!=LV_RESULT_OK) { unsafe_dma=true;goto done; }
    if(outcome!=LV_DRAW_AIC_GE2D_OUTCOME_ENGINE) goto done;
    aicos_dcache_invalid_range((unsigned long *)output,DST_STRIDE*DST_W);
    ref_layer=layer;ref_layer.draw_buf=&ref;
    lv_draw_task_t sw=task;sw.target_layer=&ref_layer;lv_draw_sw_image(&sw,&d,&sw.area);
    int64_t sn=lv_trigo_sin(d.rotation/10),cs=lv_trigo_sin(d.rotation/10+90);
    for(unsigned y=0;y<DST_W;y++) for(unsigned x=0;x<DST_W;x++) {
        int ax=(int)x+90,ay=(int)y+190;
        if((ax<task.clip_area.x1 || ax>task.clip_area.x2 || ay<task.clip_area.y1 || ay>task.clip_area.y2) &&
           output[y*DST_W+x]!=(alpha_target?backgrounds[x/24]:0xff102030)) goto done;
        int dx=ax-120-d.pivot.x,dy=ay-220-d.pivot.y;
        int64_t u=(dx*cs+dy*sn)*512/d.scale_x+d.pivot.x*65536;
        int64_t v=(-dx*sn+dy*cs)*512/d.scale_y+d.pivot.y*65536;
        bool interior=((u>=4*65536 && u<=11*65536)||(u>=20*65536 && u<=27*65536)) &&
                      ((v>=4*65536 && v<=11*65536)||(v>=20*65536 && v<=27*65536));
        if(transform && !interior) continue;
        for(unsigned c=0;c<(alpha_target?4U:3U);c++) {
            int delta=(int)((output[y*DST_W+x]>>(8*c))&255)-(int)((reference[y*DST_W+x]>>(8*c))&255);
            if(delta<0) delta=-delta;
            if(delta>worst) worst=delta;
        }
        checked++;
    }
    if(checked>=32 && worst<=(alpha_target && transform?5:3)) result=0;
done:
    if(submitted) lv_image_cache_drop(&src);
    if(reference) lv_free(reference);
    if(unsafe_dma) {
        AIC_TEST_E("FAIL recolor DMA; retaining source/target until reboot");return -1;
    }
    if(source) aicos_free_align(MEM_CMA,source);
    if(output) aicos_free_align(MEM_CMA,output);
    if(result==0) AIC_TEST_I("PASS recolor kind=%u transform=%u argb_dst=%d pixels=%d error=%d",kind,transform,alpha_target,checked,worst);
    else AIC_TEST_E("FAIL recolor kind=%u transform=%u argb_dst=%d pixels=%d error=%d",kind,transform,alpha_target,checked,worst);
    return result;
}

/* Native masking is applied to a private CMA copy before actual GE geometry.
 * The reference uses a separate child buffer because native SW masks in place. */
static int mask_probe(unsigned encoding,unsigned luminance,unsigned transform,bool alpha_target)
{
    uint8_t *source=aicos_malloc_align(MEM_CMA,SRC_STRIDE*SRC_W,64);
    uint32_t *output=aicos_malloc_align(MEM_CMA,DST_STRIDE*DST_W,64);
    uint32_t *reference=lv_malloc(DST_STRIDE*DST_W);
    uint8_t *reference_source=lv_malloc(SRC_STRIDE*SRC_W);
    uint8_t coverage[20*20];lv_memset(coverage,128,sizeof coverage);
    lv_image_dsc_t mask={.header={.magic=LV_IMAGE_HEADER_MAGIC,.w=20,.h=20,.stride=20,
        .cf=luminance?LV_COLOR_FORMAT_L8:LV_COLOR_FORMAT_A8},.data=coverage,.data_size=sizeof coverage};
    lv_draw_buf_t src,dst,ref,ref_src;
    lv_layer_t layer={0},ref_layer={0},child={0},ref_child={0};
    lv_draw_image_dsc_t d;lv_draw_task_t task={0};
    bool opened=false,unsafe_dma=false;int result=-1,worst=0,checked=0;
    if(!source || !output || !reference || !reference_source) goto done;
    for(unsigned y=0;y<SRC_W;y++) for(unsigned x=0;x<SRC_W;x++) {
        uint8_t *pixel=source+y*SRC_STRIDE+x*4;
        const unsigned channels[]={48,128,224};
        for(unsigned c=0;c<3;c++) pixel[c]=encoding?(channels[c]*192+127)/255:channels[c];
        pixel[3]=192;
    }
    lv_memcpy(reference_source,source,SRC_STRIDE*SRC_W);
    const uint32_t backgrounds[]={0x00102030,0x40205090,0x80306020,0xff103050};
    for(unsigned i=0;i<DST_W*DST_W;i++)
        output[i]=reference[i]=alpha_target?backgrounds[(i%DST_W)/24]:0xff102030;
    lv_color_format_t source_cf=encoding==1?LV_COLOR_FORMAT_ARGB8888_PREMULTIPLIED:LV_COLOR_FORMAT_ARGB8888;
    lv_color_format_t target_cf=alpha_target?LV_COLOR_FORMAT_ARGB8888:LV_COLOR_FORMAT_XRGB8888;
    if(lv_draw_buf_init(&src,SRC_W,SRC_W,source_cf,SRC_STRIDE,source,SRC_STRIDE*SRC_W)!=LV_RESULT_OK ||
       lv_draw_buf_init(&dst,DST_W,DST_W,target_cf,DST_STRIDE,output,DST_STRIDE*DST_W)!=LV_RESULT_OK ||
       lv_draw_buf_init(&ref,DST_W,DST_W,target_cf,DST_STRIDE,reference,DST_STRIDE*DST_W)!=LV_RESULT_OK) goto done;
    if(encoding==2) src.header.flags|=LV_IMAGE_FLAGS_PREMULTIPLIED;
    child.draw_buf=&src;child.color_format=source_cf;child.buf_area=(lv_area_t){120,220,151,251};
    lv_draw_image_dsc_init(&d);d.src=&child;d.header=src.header;d.image_area=child.buf_area;
    d.bitmap_mask_src=&mask;d.opa=128;d.pivot=(lv_point_t){16,16};
    if(transform) { d.scale_x=384;d.scale_y=320;d.rotation=transform==2?330:0; }
    layer.draw_buf=&dst;layer.color_format=target_cf;layer.buf_area=(lv_area_t){90,190,185,285};
    task.type=LV_DRAW_TASK_TYPE_LAYER;task.draw_dsc=&d;task.target_layer=&layer;
    task.area=child.buf_area;task.clip_area=(lv_area_t){108,209,164,268};
    aicos_dcache_clean_range((unsigned long *)source,SRC_STRIDE*SRC_W);
    aicos_dcache_clean_invalid_range((unsigned long *)output,DST_STRIDE*DST_W);
    opened=true;lv_draw_aic_ge2d_outcome_t outcome;
    if(lv_draw_aic_ge2d_image(&task,&outcome)!=LV_RESULT_OK) { unsafe_dma=true;goto done; }
    if(outcome!=LV_DRAW_AIC_GE2D_OUTCOME_ENGINE) goto done;
    aicos_dcache_invalid_range((unsigned long *)output,DST_STRIDE*DST_W);
    if(lv_memcmp(source,reference_source,SRC_STRIDE*SRC_W)) goto done;
    ref_src=src;ref_src.data=reference_source;ref_child=child;ref_child.draw_buf=&ref_src;
    ref_layer=layer;ref_layer.draw_buf=&ref;
    lv_draw_task_t sw=task;sw.target_layer=&ref_layer;
    lv_draw_image_dsc_t sw_d=d;sw_d.src=&ref_child;sw.draw_dsc=&sw_d;
    lv_draw_sw_layer(&sw,&sw_d,&sw.area);lv_image_cache_drop(&ref_src);
    int64_t sn=lv_trigo_sin(d.rotation/10),cs=lv_trigo_sin(d.rotation/10+90);
    for(unsigned y=0;y<DST_W;y++) for(unsigned x=0;x<DST_W;x++) {
        int ax=(int)x+90,ay=(int)y+190;
        if((ax<task.clip_area.x1 || ax>task.clip_area.x2 || ay<task.clip_area.y1 || ay>task.clip_area.y2) &&
           output[y*DST_W+x]!=(alpha_target?backgrounds[x/24]:0xff102030)) goto done;
        int dx=ax-120-d.pivot.x,dy=ay-220-d.pivot.y;
        int64_t u=(dx*cs+dy*sn)*512/d.scale_x+d.pivot.x*65536;
        int64_t v=(-dx*sn+dy*cs)*512/d.scale_y+d.pivot.y*65536;
        if(transform && !(u>=12*65536 && u<=19*65536 && v>=12*65536 && v<=19*65536)) continue;
        for(unsigned c=0;c<(alpha_target?4U:3U);c++) {
            int delta=(int)((output[y*DST_W+x]>>(8*c))&255)-(int)((reference[y*DST_W+x]>>(8*c))&255);
            if(delta<0) delta=-delta;
            if(delta>worst) worst=delta;
        }
        checked++;
    }
    if(checked>=32 && worst<=(transform?5:3)) result=0;
done:
    if(opened) {
        if(!unsafe_dma) lv_image_cache_drop(&src);
        lv_image_cache_drop(&mask); /* CPU-only decoder is already closed. */
    }
    if(reference) lv_free(reference);
    if(reference_source) lv_free(reference_source);
    if(unsafe_dma) { AIC_TEST_E("FAIL layer mask DMA; retaining source/target until reboot");return -1; }
    if(source) aicos_free_align(MEM_CMA,source);
    if(output) aicos_free_align(MEM_CMA,output);
    if(result==0) AIC_TEST_I("PASS mask enc=%u L8=%u geom=%u argb=%d pixels=%d error=%d",encoding,luminance,transform,alpha_target,checked,worst);
    else AIC_TEST_E("FAIL mask enc=%u L8=%u geom=%u argb=%d pixels=%d error=%d",encoding,luminance,transform,alpha_target,checked,worst);
    return result;
}

/* Ordinary IMAGE masks use a private ARGB staging copy rather than the child
 * layer path above. Keep this probe deliberately native-size: it validates the
 * decoded source-format conversion, mask coverage, cache/clip guards and the
 * independent software result without conflating scaler boundary arithmetic. */
static int image_mask_probe(unsigned format,bool alpha_target)
{
    static const lv_color_format_t formats[] = {
        LV_COLOR_FORMAT_RGB565, LV_COLOR_FORMAT_RGB888,
        LV_COLOR_FORMAT_XRGB8888, LV_COLOR_FORMAT_ARGB8888,
        LV_COLOR_FORMAT_ARGB8888_PREMULTIPLIED,
    };
    const lv_color_format_t source_cf=formats[format];
    const unsigned bpp=lv_color_format_get_size(source_cf);
    uint8_t *source=aicos_malloc_align(MEM_CMA,SRC_STRIDE*SRC_W,64);
    uint8_t *output=aicos_malloc_align(MEM_CMA,DST_STRIDE*DST_W,64);
    uint32_t *reference=lv_malloc(DST_STRIDE*DST_W);
    uint8_t *masked_reference=lv_malloc(SRC_STRIDE*SRC_W);
    uint8_t *original=lv_malloc(SRC_STRIDE*SRC_W);
    uint8_t coverage[20*20];
    lv_image_dsc_t mask={.header={.magic=LV_IMAGE_HEADER_MAGIC,.w=20,.h=20,
        .stride=20,.cf=LV_COLOR_FORMAT_A8},.data=coverage,.data_size=sizeof coverage};
    lv_draw_buf_t src,dst,ref,masked;
    lv_layer_t layer={0},ref_layer={0}; lv_draw_task_t task={0};
    lv_draw_image_dsc_t d; bool opened=false,unsafe_dma=false; int result=-1,worst=0;
    if(!source || !output || !reference || !masked_reference || !original) goto done;
    lv_memset(coverage,128,sizeof coverage);
    for(unsigned y=0;y<SRC_W;y++) for(unsigned x=0;x<SRC_W;x++) {
        unsigned a=source_cf>=LV_COLOR_FORMAT_ARGB8888?64+5*x:255;
        unsigned r=32+4*x,g=40+3*y,b=96+2*x; uint8_t *p=source+y*SRC_STRIDE+x*bpp;
        if(source_cf==LV_COLOR_FORMAT_RGB565) {
            uint16_t v=(uint16_t)((r>>3)<<11)|((uint16_t)(g>>2)<<5)|(b>>3); lv_memcpy(p,&v,2);
        }
        else {
            bool premult=source_cf==LV_COLOR_FORMAT_ARGB8888_PREMULTIPLIED;
            p[0]=premult?(b*a+127)/255:b; p[1]=premult?(g*a+127)/255:g;
            p[2]=premult?(r*a+127)/255:r; if(bpp==4) p[3]=a;
        }
    }
    lv_memcpy(original,source,SRC_STRIDE*SRC_W);
    const uint32_t backgrounds[]={0x00102030,0x40205090,0x80306020,0xff103050};
    for(unsigned i=0;i<DST_W*DST_W;i++) {
        ((uint32_t *)output)[i]=alpha_target?backgrounds[(i%DST_W)/24]:0xff102030;
        reference[i]=((uint32_t *)output)[i];
    }
    if(lv_draw_buf_init(&src,SRC_W,SRC_W,source_cf,SRC_STRIDE,source,SRC_STRIDE*SRC_W)!=LV_RESULT_OK ||
       lv_draw_buf_init(&dst,DST_W,DST_W,alpha_target?LV_COLOR_FORMAT_ARGB8888:LV_COLOR_FORMAT_XRGB8888,
                        DST_STRIDE,output,DST_STRIDE*DST_W)!=LV_RESULT_OK ||
       lv_draw_buf_init(&ref,DST_W,DST_W,alpha_target?LV_COLOR_FORMAT_ARGB8888:LV_COLOR_FORMAT_XRGB8888,
                        DST_STRIDE,reference,DST_STRIDE*DST_W)!=LV_RESULT_OK) goto done;
    if(source_cf==LV_COLOR_FORMAT_ARGB8888_PREMULTIPLIED) src.header.flags|=LV_IMAGE_FLAGS_PREMULTIPLIED;
    lv_draw_image_dsc_init(&d); d.src=&src; d.header=src.header; d.image_area=(lv_area_t){120,220,151,251};
    d.bitmap_mask_src=&mask; d.opa=128; d.pivot=(lv_point_t){16,16};
    layer.draw_buf=&dst; layer.color_format=dst.header.cf; layer.buf_area=(lv_area_t){90,190,185,285};
    task.type=LV_DRAW_TASK_TYPE_IMAGE; task.draw_dsc=&d; task.target_layer=&layer;
    task.area=d.image_area; task.clip_area=(lv_area_t){108,209,164,268};
    aicos_dcache_clean_range((unsigned long *)source,SRC_STRIDE*SRC_W);
    aicos_dcache_clean_invalid_range((unsigned long *)output,DST_STRIDE*DST_W);
    opened=true; lv_draw_aic_ge2d_outcome_t outcome;
    if(lv_draw_aic_ge2d_image(&task,&outcome)!=LV_RESULT_OK) { unsafe_dma=true; goto done; }
    if(outcome!=LV_DRAW_AIC_GE2D_OUTCOME_ENGINE) goto done;
    aicos_dcache_invalid_range((unsigned long *)output,DST_STRIDE*DST_W);
    if(lv_memcmp(source,original,SRC_STRIDE*SRC_W)) goto done;
    if(lv_draw_buf_init(&masked,SRC_W,SRC_W,
                        source_cf==LV_COLOR_FORMAT_ARGB8888_PREMULTIPLIED?
                        LV_COLOR_FORMAT_ARGB8888_PREMULTIPLIED:LV_COLOR_FORMAT_ARGB8888,
                        SRC_STRIDE,masked_reference,SRC_STRIDE*SRC_W)!=LV_RESULT_OK) goto done;
    if(source_cf==LV_COLOR_FORMAT_ARGB8888_PREMULTIPLIED) masked.header.flags|=LV_IMAGE_FLAGS_PREMULTIPLIED;
    if(lv_aic_sw_image_mask_copy(&d,&task.area,&src,&masked)!=1) goto done;
    ref_layer=layer; ref_layer.draw_buf=&ref;
    lv_draw_image_dsc_t sw_d=d; sw_d.src=&masked; sw_d.bitmap_mask_src=NULL;
    lv_draw_task_t sw=task; sw.target_layer=&ref_layer; sw.draw_dsc=&sw_d;
    lv_draw_sw_image(&sw,&sw_d,&sw.area);
    for(unsigned y=0;y<DST_W;y++) for(unsigned x=0;x<DST_W;x++) {
        int ax=(int)x+90,ay=(int)y+190;
        if((ax<task.clip_area.x1 || ax>task.clip_area.x2 || ay<task.clip_area.y1 || ay>task.clip_area.y2) &&
           ((uint32_t *)output)[y*DST_W+x]!=((uint32_t *)reference)[y*DST_W+x]) goto done;
        for(unsigned c=0;c<(alpha_target?4U:3U);c++) {
            int delta=(int)(((uint32_t *)output)[y*DST_W+x]>>(8*c)&255)-
                      (int)(((uint32_t *)reference)[y*DST_W+x]>>(8*c)&255); if(delta<0) delta=-delta;
            if(delta>worst) worst=delta;
        }
    }
    result=worst<=(source_cf==LV_COLOR_FORMAT_ARGB8888_PREMULTIPLIED?8:3)?0:-1;
done:
    if(opened) { if(!unsafe_dma) lv_image_cache_drop(&src); lv_image_cache_drop(&mask); }
    if(unsafe_dma) { AIC_TEST_E("FAIL IMAGE mask DMA; retaining source/target until reboot"); return -1; }
    if(reference) lv_free(reference); if(masked_reference) lv_free(masked_reference); if(original) lv_free(original);
    if(source) aicos_free_align(MEM_CMA,source); if(output) aicos_free_align(MEM_CMA,output);
    if(result==0) AIC_TEST_I("PASS image-mask fmt=%u argb=%u error=%d",format,alpha_target,worst);
    else AIC_TEST_E("FAIL image-mask fmt=%u argb=%u error=%d",format,alpha_target,worst);
    return result;
}

#if AIC_LVGL_USE_SPI_SDK
#include "lv_aic_spi_ge2d.h"
/* Tests only the owned GE conversion stage: no SPI bus or panel is opened. */
static unsigned spi_channel(uint16_t p,unsigned c)
{ return c==0?p&31:c==1?(p>>5)&63:p>>11; }
static int spi_stripe_probe(unsigned rotation,bool swap)
{
    enum { SOURCE_BYTES=63*8*2,OUTPUT_BYTES=64*8*2+2 };
    uint8_t *storage=lv_malloc(SOURCE_BYTES+OUTPUT_BYTES);
    if(!storage) return -1;
    uint16_t *source=(uint16_t *)storage;
    uint8_t *output=storage+SOURCE_BYTES;
    for(unsigned y=0;y<8;y++) for(unsigned x=0;x<63;x++)
        source[y*63+x]=((x/3)<<11)|((y*5+x/4)<<5)|(x/4+y);
    lv_aic_spi_rgb565_frame_t frame={.data=(const uint8_t *)source,.capacity=SOURCE_BYTES,
        .width=63,.height=8,.stride=126};
    unsigned w=rotation&1?8:64,h=rotation&1?64:8;
    lv_aic_spi_ge2d_t *g=lv_aic_spi_ge2d_create(63,8,w,h,8192);
    if(!g) { lv_free(storage);return -1; }
    lv_memset(output,0xa5,OUTPUT_BYTES);
    lv_aic_spi_result_t converted=lv_aic_spi_ge2d_convert(g,&frame,output+1,OUTPUT_BYTES-2,rotation*90,swap);
    if(converted!=LV_AIC_SPI_OK) {
        /* Converter retains its own DMA storage on FAULT; borrowed CPU
         * source/output are reusable after any synchronous return. */
        (void)lv_aic_spi_ge2d_close(g);lv_free(storage);
        AIC_TEST_E("FAIL SPI GE stripe conversion result=%d",(int)converted);return -1;
    }
    int worst=0;
    for(unsigned y=0;y<h;y++) for(unsigned x=0;x<w;x++) {
        unsigned u=rotation==0?x:rotation==1?y:rotation==2?w-1-x:h-1-y;
        unsigned v=rotation==0?y:rotation==1?w-1-x:rotation==2?h-1-y:x;
        /* SDK 63-to-64 step=64512, initial half-step=32256. */
        unsigned q=32256+u*64512,a=LV_MIN(q/65536,62U),b=LV_MIN(a+1,62U),f=q%65536;
        unsigned pos=1+2*(y*w+x);
        uint16_t actual=output[pos+(swap?1:0)]+256U*output[pos+(swap?0:1)];
        for(unsigned c=0;c<3;c++) {
            unsigned want=(spi_channel(source[v*63+a],c)*(65536-f)+
                           spi_channel(source[v*63+b],c)*f+32768)/65536;
            int delta=(int)spi_channel(actual,c)-(int)want;
            if(delta<0) delta=-delta;
            if(delta>worst) worst=delta;
        }
    }
    bool guards=output[0]==0xa5 && output[OUTPUT_BYTES-1]==0xa5;
    lv_free(storage);
    if(lv_aic_spi_ge2d_close(g)!=LV_AIC_SPI_OK || !guards || worst>1) {
        AIC_TEST_E("FAIL SPI GE stripes rot=%u swap=%d max_native_error=%d guards=%d",rotation*90,swap,worst,guards);
        return -1;
    }
    AIC_TEST_I("PASS SPI GE stripes rot=%u swap=%d pixels=512 max_native_error=%d",rotation*90,swap,worst);
    return 0;
}
#endif

int lv_aic_ge2d_scale_test_run(void)
{
    for(unsigned alpha_target=0;alpha_target<2;alpha_target++)
    for(unsigned format=0;format<5;format++) {
        AIC_TEST_I("BEGIN image-mask format=%u argb=%u",format,alpha_target);
        if(image_mask_probe(format,alpha_target!=0)) return -1;
    }
    for(unsigned alpha_target=0;alpha_target<2;alpha_target++)
    for(unsigned encoding=0;encoding<3;encoding++)
    for(unsigned luminance=0;luminance<2;luminance++) for(unsigned transform=0;transform<3;transform++) {
        if(mask_probe(encoding,luminance,transform,alpha_target!=0)) return -1;
    }
    for(unsigned alpha_target=0;alpha_target<2;alpha_target++)
    for(unsigned kind=0;kind<6;kind++) for(unsigned transform=0;transform<3;transform++) {
        AIC_TEST_I("BEGIN recolor kind=%u transform=%u argb_dst=%u",kind,transform,alpha_target);
        if(recolor_probe(kind,transform,alpha_target!=0)) return -1;
    }
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
    /* Near-unity unclipped RGB scaling (32-sample interval, 32-wide output)
     * runs through the preflighted stripe path: two balanced commands keep
     * the fractional phases inside the source, so no whole-task SW. */
    if (scale_probe(264,256,false,false,false,true,false,0)) return -1;
    const uint16_t stripe_scales[]={264,281};
    for(unsigned kind=0;kind<4;kind++) for(unsigned zoom=0;zoom<2;zoom++) {
        for(unsigned angle=0;angle<3600;angle+=900) {
            AIC_TEST_I("BEGIN RGB stripes sx=%u rot=%u kind=%u",(unsigned)stripe_scales[zoom],angle/10,kind);
            if(scale_probe(stripe_scales[zoom],256,kind,2,true,true,false,(uint16_t)angle)) return -1;
        }
        AIC_TEST_I("BEGIN multipass stripes sx=%u rot=33 kind=%u",(unsigned)stripe_scales[zoom],kind);
        if(scale_probe(stripe_scales[zoom],384,kind,false,true,true,false,330)) return -1;
    }
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
    for(unsigned kind=0;kind<4;kind++) {
        AIC_TEST_I("BEGIN multipass sx=384 sy=192 rot=33 kind=%u",kind);
        if(scale_probe(384,192,kind,false,true,true,false,330)) return -1;
        AIC_TEST_I("BEGIN multipass tiled sx=512 sy=512 rot=45 kind=%u",kind);
        if(scale_probe(512,512,kind,true,true,true,true,450)) return -1;
    }
#if AIC_LVGL_USE_SPI_SDK
    for(unsigned rotation=0;rotation<4;rotation++) for(unsigned swap=0;swap<2;swap++)
        if(spi_stripe_probe(rotation,swap!=0)) return -1;
#endif
    AIC_TEST_I("PASS 3C2 numeric probes; panel edges/touch still require confirmation");
    return 0;
}
#endif
