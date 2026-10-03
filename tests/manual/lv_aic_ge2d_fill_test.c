/* SPDX-License-Identifier: Apache-2.0
 * Real fill executor; offscreen CMA output and independent blend arithmetic.
 * Board evidence is required: host contracts verify descriptors, not pixels. */
#define AIC_LVGL_USE_PRIVATE_API 1
#define LOG_TAG "lvgl.ge2d.fill"
#define LOG_LVL LOG_LVL_INFO
#include "lv_aic_manual_test.h"
#if AIC_LVGL_USE_GE2D && AIC_LVGL_BSP_RTTHREAD
#include "lvgl_aic_private.h"
#include "lv_draw_aic_ge2d.h"
#include <aic_core.h>
#include <aic_osal.h>
#include "lv_aic_test_log.h"
#include <string.h>

#define FILL_W 16
#define FILL_H 16
#define FILL_STRIDE 96
#define FILL_BYTES (FILL_H * FILL_STRIDE)
static bool fill_probe_poisoned;

static void read_rgb(const uint8_t *p, lv_color_format_t cf, int rgb[3])
{
    if (cf == LV_COLOR_FORMAT_RGB565) {
        uint16_t v = (uint16_t)p[0] | ((uint16_t)p[1] << 8);
        rgb[0] = ((v >> 11) & 31) * 255 / 31;
        rgb[1] = ((v >> 5) & 63) * 255 / 63;
        rgb[2] = (v & 31) * 255 / 31;
    }
    else { rgb[0] = p[2]; rgb[1] = p[1]; rgb[2] = p[0]; }
}

static int fill_probe(lv_color_format_t cf, uint8_t opacity)
{
    uint8_t *output;
    uint8_t background[4] = {240,160,16,255};
    const int source[3] = {200,40,80};
    int bg[3], worst = 0, checked = 0, result = -1;
    uint32_t bpp = lv_color_format_get_size(cf);
    lv_draw_buf_t dst;
    lv_draw_fill_dsc_t d;
    lv_layer_t layer = {0};
    lv_draw_task_t task = {0};
    if (fill_probe_poisoned) {
        AIC_TEST_E("FAIL prior DMA failure; reboot before retrying fill probes");
        return -1;
    }
    output = aicos_malloc_align(MEM_CMA, FILL_BYTES, 32);
    if (!output) return -1;
    if (cf == LV_COLOR_FORMAT_RGB565) {
        uint16_t v = ((16 >> 3) << 11) | ((160 >> 2) << 5) | (240 >> 3);
        background[0] = v & 255; background[1] = v >> 8;
    }
    memset(output, 0xa5, FILL_BYTES);
    for (int y = 0; y < FILL_H; y++) {
        for (int x = 0; x < FILL_W; x++)
            memcpy(output + y * FILL_STRIDE + x * bpp, background, bpp);
    }
    read_rgb(background, cf, bg);
    if (lv_draw_buf_init(&dst,FILL_W,FILL_H,cf,FILL_STRIDE,output,FILL_BYTES) != LV_RESULT_OK)
        goto done;
    lv_draw_fill_dsc_init(&d); d.color = lv_color_make(200,40,80); d.opa = opacity;
    layer.draw_buf = &dst; layer.buf_area = (lv_area_t){100,200,115,215};
    task.type = LV_DRAW_TASK_TYPE_FILL; task.draw_dsc = &d; task.target_layer = &layer;
    /* Both task and clip extend outside the layer. Result is x=0..9,y=3..11. */
    task.area = (lv_area_t){97,198,112,211};
    task.clip_area = (lv_area_t){95,203,109,219};
    aicos_dcache_clean_invalid_range((unsigned long *)output, FILL_BYTES);
    if (lv_draw_aic_ge2d_fill(&task) != LV_RESULT_OK) {
        /* A failed sync does not establish that DMA has stopped. Keep this
         * allocation alive and block retries until reset, rather than free it. */
        fill_probe_poisoned = true;
        AIC_TEST_E("FAIL fill DMA; retaining %u CMA bytes until reboot", (unsigned)FILL_BYTES);
        return -1;
    }
    aicos_dcache_invalid_range((unsigned long *)output, FILL_BYTES);
    for (int y = 0; y < FILL_H; y++) {
        for (int x = 0; x < FILL_W; x++) {
            const uint8_t *p = output + y * FILL_STRIDE + x * bpp;
            int rgb[3];
            if (x > 9 || y < 3 || y > 11) {
                if (memcmp(p,background,bpp) != 0) {
                    AIC_TEST_E("FAIL fill clip guard x=%d y=%d",x,y); goto done;
                }
                continue;
            }
            read_rgb(p,cf,rgb);
            for (int c = 0; c < 3; c++) {
                int want = (source[c]*opacity + bg[c]*(255-opacity)+127)/255;
                int error = rgb[c] - want;
                int tolerance = cf == LV_COLOR_FORMAT_RGB565 ? (c == 1 ? 5 : 9) : 2;
                if (error < 0) error = -error;
                if (error > worst) worst = error;
                if (error > tolerance) {
                    AIC_TEST_E("FAIL fill pixel x=%d y=%d c=%d got=%d want=%d",x,y,c,rgb[c],want);
                    goto done;
                }
            }
            checked++;
        }
        for (unsigned byte = FILL_W*bpp; byte < FILL_STRIDE; byte++) {
            if (output[y*FILL_STRIDE+byte] != 0xa5) {
                AIC_TEST_E("FAIL fill stride padding y=%d byte=%u",y,byte); goto done;
            }
        }
    }
    if (checked != 90) goto done;
    AIC_TEST_I("PASS fill cf=%d opa=%u pixels=%d max_error=%d guards=OK",(int)cf,opacity,checked,worst);
    result = 0;
done:
    aicos_free_align(MEM_CMA, output);
    return result;
}

static int fake_probe(uint8_t alpha)
{
    if (fill_probe_poisoned) return -1;
    uint8_t *output = aicos_malloc_align(MEM_CMA, FILL_BYTES, 32);
    if (!output) return -1;
    int result = -1;
    char path[64];
    lv_draw_buf_t dst;
    lv_draw_image_dsc_t image;
    lv_layer_t layer = {0};
    lv_draw_task_t task = {0};
    lv_draw_aic_ge2d_outcome_t outcome;
    lv_snprintf(path, sizeof(path), "L:/16x16_0_%08x.fake", ((unsigned)alpha << 24) | 0x123456);
    memset(output, 0xa5, FILL_BYTES);
    if (lv_draw_buf_init(&dst,FILL_W,FILL_H,LV_COLOR_FORMAT_ARGB8888,
                         FILL_STRIDE,output,FILL_BYTES) != LV_RESULT_OK) goto done;
    lv_draw_image_dsc_init(&image);
    image.src = path;
    if (lv_image_decoder_get_info(path, &image.header) != LV_RESULT_OK) goto done;
    layer.draw_buf = &dst; layer.color_format = LV_COLOR_FORMAT_ARGB8888;
    layer.buf_area = (lv_area_t){100,200,115,215};
    task.type = LV_DRAW_TASK_TYPE_IMAGE; task.draw_dsc = &image; task.target_layer = &layer;
    task.area = (lv_area_t){97,198,112,211};
    task.clip_area = (lv_area_t){95,203,109,219};
    aicos_dcache_clean_invalid_range((unsigned long *)output, FILL_BYTES);
    if (lv_draw_aic_ge2d_image(&task, &outcome) != LV_RESULT_OK) {
        fill_probe_poisoned = true;
        AIC_TEST_E("FAIL fake DMA; retaining %u CMA bytes until reboot", (unsigned)FILL_BYTES);
        return -1;
    }
    if (outcome != LV_DRAW_AIC_GE2D_OUTCOME_ENGINE) goto done;
    aicos_dcache_invalid_range((unsigned long *)output, FILL_BYTES);
    for (int y = 0; y < FILL_H; y++) {
        for (int byte = 0; byte < FILL_STRIDE; byte++) {
            const uint8_t argb[] = {0x56,0x34,0x12,alpha};
            uint8_t expected = y >= 3 && y <= 11 && byte < 40 ? argb[byte % 4] : 0xa5;
            if (output[y*FILL_STRIDE + byte] != expected) {
                AIC_TEST_E("FAIL fake alpha=%u y=%d byte=%d", alpha,y,byte);
                goto done;
            }
        }
    }
    AIC_TEST_I("PASS fake replace alpha=%u pixels=90 guards=OK", alpha);
    result = 0;
done:
    lv_image_header_cache_drop(path);
    aicos_free_align(MEM_CMA, output);
    if (result) AIC_TEST_E("FAIL fake replacement alpha=%u", alpha);
    return result;
}

int lv_aic_ge2d_fill_test_run(void)
{
    const lv_color_format_t formats[] = {LV_COLOR_FORMAT_RGB565, LV_COLOR_FORMAT_RGB888,
                                       LV_COLOR_FORMAT_XRGB8888};
    const uint8_t opacities[] = {64,128,192,255};
    for (unsigned f = 0; f < sizeof(formats)/sizeof(formats[0]); f++) {
        for (unsigned a = 0; a < sizeof(opacities)/sizeof(opacities[0]); a++) {
            if (fill_probe(formats[f],opacities[a]) != 0) return -1;
        }
    }
    AIC_TEST_I("PASS 12 solid-fill numeric probes; panel acceptance remains separate");
    if (fake_probe(0) || fake_probe(128) || fake_probe(255)) return -1;
    return 0;
}
#endif
