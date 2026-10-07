/* SPDX-License-Identifier: Apache-2.0
 * Real fill executor; offscreen CMA output and independent blend arithmetic.
 * Host models check staged pixels; actual GE/cache acceptance requires the board. */
#define AIC_LVGL_USE_PRIVATE_API 1
#define LOG_TAG "lvgl.ge2d.fill"
#define LOG_LVL LOG_LVL_INFO
#include "lv_aic_manual_test.h"
#if AIC_LVGL_USE_GE2D && AIC_LVGL_BSP_RTTHREAD
#include "lvgl_aic_private.h"
#include "lv_draw_aic_ge2d.h"
#include <aic_core.h>
#include <mpp_ge.h>
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

#include "lv_aic_video_window.h"
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
#if AIC_LVGL_USE_VIDEO_WINDOW
    lv_obj_t *window=NULL;
#endif
    lv_snprintf(path, sizeof(path), "L:/16x16_0_%08x.fake", ((unsigned)alpha << 24) | 0x123456);
    memset(output, 0xa5, FILL_BYTES);
    if (lv_draw_buf_init(&dst,FILL_W,FILL_H,LV_COLOR_FORMAT_ARGB8888,
                         FILL_STRIDE,output,FILL_BYTES) != LV_RESULT_OK) goto done;
    lv_draw_image_dsc_init(&image);
    image.src = path;
#if AIC_LVGL_USE_VIDEO_WINDOW
    if(alpha==0) {
        window=lv_aic_video_window_create(lv_screen_active());
        if(!window) goto done;
        lv_obj_set_hidden(window,true);
        lv_aic_video_window_set_color(window,lv_color_hex(0x123456));
        lv_aic_video_window_set_size(window,16,16);
        image.src=lv_image_get_src(window);
        if(!image.src || strcmp(image.src,path)) goto done;
    }
#endif
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
#if AIC_LVGL_USE_VIDEO_WINDOW
        if(window) lv_obj_delete(window);
#endif
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
#if AIC_LVGL_USE_VIDEO_WINDOW
    if(window) {
        lv_obj_delete(window);
        if(!result) AIC_TEST_I("PASS video-window source and ARGB alpha-zero pixels");
    }
#endif
    lv_image_header_cache_drop(path);
    aicos_free_align(MEM_CMA, output);
    if (result) AIC_TEST_E("FAIL fake replacement alpha=%u", alpha);
    return result;
}

/* Diagnostic only: bypass production RGB565 key fallback to measure what the
 * raw engine comparator actually matches. LVGL's lv_color16_to_color() uses
 * plain shifts (r<<3, g<<2, b<<3), but that is an LVGL convention the engine
 * is not required to share: on 2026-10-06 the board matched the black key
 * 0x000000 and rejected that shifted white key 0xf8fcf8 while the engine's
 * own unkeyed conversion wrote blue=0xff. Every sample therefore measures the
 * engine's own conversion of the key color first and tries it as the key,
 * then the replicating expansion, the plain-shift expansion and both
 * zero-extended 16-bit placements, before reporting the sample unexplained.
 * These finite samples are evidence, not exhaustive format parity. */
static uint32_t key565_expand(uint16_t v,unsigned combo)
{
    uint32_t r=(v>>11)&0x1fu,g=(v>>5)&0x3fu,b=v&0x1fu;
    uint32_t r8=(combo&1u)?((r<<3)|(r>>2)):(r<<3);
    uint32_t g8=(combo&2u)?((g<<2)|(g>>4)):(g<<2);
    uint32_t b8=(combo&4u)?((b<<3)|(b>>2)):(b<<3);
    return (r8<<16)|(g8<<8)|b8;
}

/* Combo bits that reproduce an RGB888 value from a packed RGB565 code. */
static unsigned key565_class(uint16_t v,uint32_t rgb)
{
    unsigned mask=0;
    for(unsigned combo=0;combo<8;combo++)
        if(key565_expand(v,combo)==rgb) mask|=1u<<combo;
    return mask;
}

static bool key565_verify(const uint8_t *keyed,const uint8_t *base)
{
    for(unsigned y=0;y<16;y++) for(unsigned b=0;b<64;b++) {
        uint8_t want=b<16 || b>=32?0xa5:base[y*64+b];
        if(keyed[y*64+b]!=want || (b>=32 && base[y*64+b]!=0xa5)) return false;
    }
    return true;
}

static void key565_first_diff(const uint8_t *keyed,const uint8_t *base,
                              unsigned *y,unsigned *byte,uint8_t *got,uint8_t *want)
{
    for(unsigned yy=0;yy<16;yy++) for(unsigned b=0;b<64;b++) {
        uint8_t w=b<16 || b>=32?0xa5:base[yy*64+b];
        if(keyed[yy*64+b]!=w || (b>=32 && base[yy*64+b]!=0xa5)) {
            *y=yy;*byte=b;*got=keyed[yy*64+b];*want=w;return;
        }
    }
    *y=0;*byte=0;*got=0;*want=0;
}

static int key565_probe(void)
{
    const uint16_t colors[]={0,0xffff,0xf800,0x07e0,0x001f,0xf81f,0x07ff,0xffe0,0x8410};
    static const char *const labels[]={"engine","rep","trunc","raw16lo","raw16hi"};
    uint8_t *src=NULL,*base=NULL,*keyed=NULL;
    struct mpp_ge *ge=lv_draw_aic_ge2d_device();
    unsigned all=0x1fu;
    int result=-1;
    if(fill_probe_poisoned || !ge) return -1;
    src=aicos_malloc_align(MEM_CMA,1024,64);
    base=aicos_malloc_align(MEM_CMA,1024,64);
    keyed=aicos_malloc_align(MEM_CMA,1024,64);
    if(!src || !base || !keyed) goto done;
    for(unsigned c=0;c<sizeof(colors)/sizeof(colors[0]);c++) {
        unsigned sample=0;
        uint32_t cand[5],engine;
        struct ge_bitblt blt={0};
        memset(src,0xa5,1024);memset(base,0xa5,1024);memset(keyed,0xa5,1024);
        for(unsigned y=0;y<16;y++) for(unsigned x=0;x<8;x++) {
            uint16_t pixel=x<4?colors[c]:(uint16_t)~colors[c];
            memcpy(src+y*64+x*2,&pixel,2);
        }
        blt.src_buf=(struct mpp_buf){.buf_type=MPP_PHY_ADDR,.format=MPP_FMT_RGB_565,
            .size={8,16},.stride={64},.phy_addr={(uint32_t)(uintptr_t)src}};
        blt.dst_buf=(struct mpp_buf){.buf_type=MPP_PHY_ADDR,.format=MPP_FMT_ARGB_8888,
            .size={8,16},.stride={64}};
        blt.ctrl.src_global_alpha=255;
        aicos_dcache_clean_invalid_range((unsigned long *)src,1024);
        /* The unkeyed pass is both the pixel baseline and the engine's own
         * RGB565->RGB888 conversion of this key color. */
        blt.dst_buf.phy_addr[0]=(uint32_t)(uintptr_t)base;
        blt.ctrl.ck_en=0;
        blt.ctrl.ck_value=0;
        aicos_dcache_clean_invalid_range((unsigned long *)base,1024);
        if(mpp_ge_bitblt(ge,&blt)<0 || mpp_ge_emit(ge)<0 || mpp_ge_sync(ge)<0) {
            fill_probe_poisoned=true;
            lv_draw_aic_ge2d_quarantine();
            AIC_TEST_E("FAIL key565 DMA; retaining 3072 CMA bytes until reboot");
            return -1;
        }
        aicos_dcache_invalid_range((unsigned long *)base,1024);
        /* An unchanged baseline must not make a no-op engine look correct. */
        for(unsigned y=0;y<16;y++) for(unsigned x=0;x<8;x++) {
            const uint8_t *pixel=base+y*64+x*4;
            if(pixel[0]==0xa5 && pixel[1]==0xa5 && pixel[2]==0xa5) {
                AIC_TEST_E("FAIL key565 unchanged baseline sample=%u",c);goto done;
            }
        }
        /* ARGB8888 bytes are blue, green, red, alpha. */
        engine=((uint32_t)base[2]<<16)|((uint32_t)base[1]<<8)|base[0];
        cand[0]=engine;
        cand[1]=key565_expand(colors[c],7);
        cand[2]=key565_expand(colors[c],0);
        cand[3]=(uint32_t)colors[c];
        cand[4]=(uint32_t)colors[c]<<8;
        AIC_TEST_I("key565 sample=%u src=%04x base=%06x class=%02x",
              c,(unsigned)colors[c],(unsigned)engine,key565_class(colors[c],engine));
        for(unsigned i=0;i<5;i++) {
            unsigned duplicate=0;
            for(unsigned j=0;j<i;j++) duplicate|=cand[i]==cand[j];
            if(duplicate) continue;
            blt.dst_buf.phy_addr[0]=(uint32_t)(uintptr_t)keyed;
            blt.ctrl.ck_en=1;
            blt.ctrl.ck_value=cand[i];
            aicos_dcache_clean_invalid_range((unsigned long *)keyed,1024);
            if(mpp_ge_bitblt(ge,&blt)<0 || mpp_ge_emit(ge)<0 || mpp_ge_sync(ge)<0) {
                fill_probe_poisoned=true;
                lv_draw_aic_ge2d_quarantine();
                AIC_TEST_E("FAIL key565 DMA; retaining 3072 CMA bytes until reboot");
                return -1;
            }
            aicos_dcache_invalid_range((unsigned long *)keyed,1024);
            if(!key565_verify(keyed,base)) {
                if(i==0) {
                    unsigned y=0,byte=0;uint8_t got=0,want=0;
                    key565_first_diff(keyed,base,&y,&byte,&got,&want);
                    AIC_TEST_E("FAIL key565 sample=%u key=%06x y=%u byte=%u got=%u want=%u; trying other encodings",
                          c,(unsigned)cand[i],y,byte,(unsigned)got,(unsigned)want);
                }
                continue;
            }
            for(unsigned j=0;j<5;j++) if(cand[j]==cand[i]) sample|=1u<<j;
            AIC_TEST_I("PASS key565 sample=%u enc=%s key=%06x base=%06x class=%02x guards=OK",
                  c,labels[i],(unsigned)cand[i],(unsigned)engine,key565_class(colors[c],engine));
            break;
        }
        if(!sample) {
            AIC_TEST_E("FAIL key565 sample=%u unexplained: engine=%06x rep=%06x trunc=%06x raw16lo=%06x raw16hi=%06x",
                  c,(unsigned)cand[0],(unsigned)cand[1],(unsigned)cand[2],
                  (unsigned)cand[3],(unsigned)cand[4]);
        }
        /* Keep measuring later samples; the suite reports the soft failure. */
        all&=sample;
    }
    if(!all) {
        AIC_TEST_E("FAIL key565: no single encoding matched every sample");
        goto done;
    }
    else {
        unsigned first=0;
        while(first<5 && !(all&(1u<<first))) first++;
        AIC_TEST_I("PASS 9 key565 samples; common encoding=%s mask=%02x",
              labels[first],all);
    }
    result=0;
done:
    if(src) aicos_free_align(MEM_CMA,src);
    if(base) aicos_free_align(MEM_CMA,base);
    if(keyed) aicos_free_align(MEM_CMA,keyed);
    return result;
}

/* Native fill oracle also covers the near-transparent opacity threshold. */
static int alpha_fill_probe(uint8_t opacity, uint8_t alpha, bool fake)
{
    if(fill_probe_poisoned) return -1;
    uint8_t *output=aicos_malloc_align(MEM_CMA,FILL_BYTES,64);
    uint8_t *expected=lv_malloc(FILL_BYTES);
    int result=-1;
    if(!output || !expected) goto done;
    memset(output,0xa5,FILL_BYTES);
    for(unsigned y=0;y<FILL_H;y++) for(unsigned x=0;x<FILL_W;x++) {
        uint8_t *p=output+y*FILL_STRIDE+x*4;
        p[0]=(x*17+y*3)%256;p[1]=(y*11+x*7)%256;p[2]=(x*5+y*13)%256;p[3]=alpha;
    }
    memcpy(expected,output,FILL_BYTES);
    lv_draw_buf_t dst,ref;
    if(lv_draw_buf_init(&dst,FILL_W,FILL_H,LV_COLOR_FORMAT_ARGB8888,FILL_STRIDE,output,FILL_BYTES)!=LV_RESULT_OK ||
       lv_draw_buf_init(&ref,FILL_W,FILL_H,LV_COLOR_FORMAT_ARGB8888,FILL_STRIDE,expected,FILL_BYTES)!=LV_RESULT_OK) goto done;
    lv_layer_t layer={0};layer.draw_buf=&dst;layer.color_format=LV_COLOR_FORMAT_ARGB8888;
    layer.buf_area=(lv_area_t){100,200,115,215};
    lv_draw_fill_dsc_t fill;lv_draw_fill_dsc_init(&fill);
    fill.color=lv_color_make(200,40,80);fill.opa=opacity;
    lv_draw_task_t task={0};task.type=LV_DRAW_TASK_TYPE_FILL;task.draw_dsc=&fill;task.target_layer=&layer;
    task.area=(lv_area_t){97,198,112,211};task.clip_area=(lv_area_t){95,203,109,219};
    lv_layer_t reference=layer;reference.draw_buf=&ref;
    lv_draw_task_t sw=task;sw.target_layer=&reference;
    lv_area_intersect(&sw.clip_area,&sw.clip_area,&reference.buf_area);
    lv_draw_sw_fill(&sw,&fill,&sw.area);
    aicos_dcache_clean_invalid_range((unsigned long *)output,FILL_BYTES);
    lv_result_t status;
    lv_draw_aic_ge2d_outcome_t outcome=LV_DRAW_AIC_GE2D_OUTCOME_ENGINE;
    if(fake) {
        char path[64];lv_draw_image_dsc_t image;lv_draw_image_dsc_init(&image);
        lv_snprintf(path,sizeof path,"L:/16x14_1_%08x.fake",((unsigned)opacity<<24)|0xc82850U);
        image.src=path;image.opa=0; /* Encoded pseudo-fill alpha owns this operation. */
        task.type=LV_DRAW_TASK_TYPE_IMAGE;task.draw_dsc=&image;
        status=lv_draw_aic_ge2d_image(&task,&outcome);
    }
    else status=lv_draw_aic_ge2d_fill(&task);
    if(status!=LV_RESULT_OK) {
        if(lv_draw_aic_ge2d_faulted()) {
            fill_probe_poisoned=true;
            AIC_TEST_E("FAIL ARGB fill DMA; retaining target and stage until reboot");
            output=NULL; /* The allocation must outlive uncertain DMA. */
        }
        goto done;
    }
    if(outcome!=LV_DRAW_AIC_GE2D_OUTCOME_ENGINE) goto done;
    aicos_dcache_invalid_range((unsigned long *)output,FILL_BYTES);
    for(unsigned i=0;i<FILL_BYTES;i++) {
        if(output[i]!=expected[i]) {
            AIC_TEST_E("FAIL ARGB fill fake=%u opa=%u a=%u byte=%u got=%u want=%u",
                       (unsigned)fake,opacity,alpha,i,output[i],expected[i]);
            goto done;
        }
    }
    AIC_TEST_I("PASS ARGB fill fake=%u opa=%u a=%u pixels=90 guards=OK",
               (unsigned)fake,opacity,alpha);
    result=0;
done:
    if(output) aicos_free_align(MEM_CMA,output);
    if(expected) lv_free(expected);
    if(result) AIC_TEST_E("FAIL ARGB fill fake=%u opa=%u a=%u",(unsigned)fake,opacity,alpha);
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
    const uint8_t target_alphas[]={0,1,64,128,254,255};
    const uint8_t fill_opacities[]={LV_OPA_MIN+1,64,128,192,LV_OPA_MAX-1,LV_OPA_MAX,255};
    for(unsigned fake=0;fake<2;fake++) for(unsigned a=0;a<sizeof target_alphas;a++)
        for(unsigned o=0;o<sizeof fill_opacities;o++)
            if(alpha_fill_probe(fill_opacities[o],target_alphas[a],fake)) return -1;
    AIC_TEST_I("PASS 84 ARGB solid/fake fill probes; hardware timing acceptance remains separate");
    /* Soft measurements decide whether raw engine semantics were established.
     * Bit 0: the RGB565 color-key encoding. Keep it separate from the numeric
     * fill verdict so an open measurement neither hides the canvas fill probes
     * nor the later GE2D blocks (return >0 from this function). The native
     * fill runner hard-checks the board-measured ARGB8888 blended destination
     * alpha and has no soft bit. */
    int soft_result = key565_probe() == 0 ? 0 : 1;
#if AIC_LVGL_USE_CANVAS
    if(lv_aic_native_fill_test_run() < 0) return -1;
#endif
    return soft_result;
}
#endif
