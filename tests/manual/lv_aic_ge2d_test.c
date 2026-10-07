/* SPDX-License-Identifier: Apache-2.0
 * Board-only finite GE2D checks, run by the LVGL owner before the UI.
 * Does not claim pixel correctness; compare the final page on real hardware. */
#include "lv_aic_manual_test.h"
#if AIC_LVGL_USE_GE2D && AIC_LVGL_BSP_RTTHREAD
#define AIC_LVGL_USE_PRIVATE_API 1
#include "lvgl_aic_private.h"
#include "lv_draw_aic_ge2d.h"
#include <rtthread.h>
#define LOG_TAG "lvgl.ge2d.test"
#define LOG_LVL LOG_LVL_INFO
#include "lv_aic_test_log.h"

/* The manual page owns the shapes; this only measures the counters around one
 * forced full refresh. Counts are deltas, so the verdict is independent of how
 * many frames the smoke loop has already rendered.
 *
 * These are anti-vacuity guards, not correctness checks. In particular a
 * completed count does not distinguish a task the engine drew from one it
 * handed back to the software renderer, so "accepted == completed" proves the
 * scheduler ran the task to completion, not that the pixels are right. The
 * engine-drawn numbers below are computed as completed minus sw_fallback, which
 * is the only pair of counters that can tell the two apart.
 *
 * Counters cannot see pixels at all, so Phase 3C1 adds one check that can: the
 * numeric blend probe further down. It compares the engine's output against
 * LVGL's own arithmetic for both source shapes the port accepts. What is still
 * a panel question - the composite's appearance on the real panel, touch, and
 * the colour of the display path - is stated as such in the phase record. */
#define AIC_GE2D_MIN_FILL 8U /* large + medium + 6 small */

/* The image probes reuse the Phase 2 MPP fixtures, so they only exist when the
 * decoder is also enabled. The LAYER probe needs the draw unit alone. */
#if AIC_LVGL_USE_MPP_DEC
#define AIC_GE2D_IMAGE_PROBES 1
/* The blend probe below drives the engine directly, so it needs the same MPP
 * headers the draw unit uses. mpp_ge.h pulls in aic_drv_ge.h, which is where
 * struct ge_bitblt and the GE_PD_ rules live. */
#include <aic_core.h>
#include <aic_osal.h>
#include <mpp_ge.h>
#else
#define AIC_GE2D_IMAGE_PROBES 0
#endif

#if AIC_GE2D_IMAGE_PROBES

/* ------------------------------------------------------------------ *
 * Numeric blend probe (Phase 3C1).
 *
 * The counter checks below prove a task reached the engine. None of them can
 * prove the engine computed the right colour, and a look at the panel cannot
 * tell a correct blend from one that is too bright by a few counts. This probe
 * can: it blits a known source onto a known destination and compares every
 * result byte against LVGL's own composition arithmetic.
 *
 * It builds the blit itself rather than driving a draw task. The subject is the
 * GE's blend datapath under the configuration lv_draw_aic_ge2d_image.c uses,
 * and a draw task would put a decoder, a layer and a clip rectangle between the
 * test and the thing under test. The configuration below mirrors that file and
 * has to change with it.
 *
 * The destination is ARGB8888 rather than the display's RGB565 so the
 * comparison is not limited by 5/6-bit quantisation: this measures the blend,
 * not the panel. The panel check covers the display path.
 * ------------------------------------------------------------------ */

#define AIC_GE2D_PROBE_W 16
#define AIC_GE2D_PROBE_H 8
/* LVGL combines alpha with >>8 while the GE documents /255, so a single blend
 * step can legitimately differ by one count. Two counts of slack accepts that
 * rounding and still rejects the wrong Porter/Duff rule, which misses by tens. */
#define AIC_GE2D_PROBE_TOLERANCE 2

/* A source and a destination that differ in every channel, so an incorrect
 * blend produces a large error instead of a plausible one. */
static const uint8_t aic_ge2d_probe_src[3] = { 200U, 40U, 80U };
static const uint8_t aic_ge2d_probe_dst[3] = { 16U, 160U, 240U };

static uint32_t aic_ge2d_probe_bpp(enum mpp_pixel_format fmt)
{
    return (fmt == MPP_FMT_ARGB_8888) ? 4U : 3U;
}

/* Both formats are stored blue first - what lv_color32_t declares and what
 * LVGL's own RGB888 writer emits - so only the alpha byte differs between
 * them. The engine is told the format through mpp_buf.format; the bytes here
 * follow the same convention the decoder hands it. */
static void aic_ge2d_probe_fill_src(uint8_t *mem, uint32_t stride,
                                    enum mpp_pixel_format fmt, lv_opa_t alpha)
{
    uint32_t bpp = aic_ge2d_probe_bpp(fmt);

    for (int32_t y = 0; y < AIC_GE2D_PROBE_H; y++) {
        uint8_t *row = mem + (stride * (uint32_t)y);

        for (int32_t x = 0; x < AIC_GE2D_PROBE_W; x++) {
            uint8_t *px = row + ((uint32_t)x * bpp);

            px[0] = aic_ge2d_probe_src[2];
            px[1] = aic_ge2d_probe_src[1];
            px[2] = aic_ge2d_probe_src[0];
            if (bpp == 4U) {
                px[3] = (uint8_t)alpha;
            }
        }
    }
}

static void aic_ge2d_probe_fill_dst(uint8_t *mem, uint32_t stride)
{
    for (int32_t y = 0; y < AIC_GE2D_PROBE_H; y++) {
        uint8_t *row = mem + (stride * (uint32_t)y);

        for (int32_t x = 0; x < AIC_GE2D_PROBE_W; x++) {
            uint8_t *px = row + ((uint32_t)x * 4U);

            px[0] = aic_ge2d_probe_dst[2];
            px[1] = aic_ge2d_probe_dst[1];
            px[2] = aic_ge2d_probe_dst[0];
            px[3] = 0xffU;
        }
    }
}

/**
 * Blit one probe and return the largest per-channel deviation, or -1 on error.
 *
 * @p src_fmt decides whether the source carries its own alpha, @p global_opa is
 * the descriptor's opacity, and @p rules is the Porter/Duff pair to measure. The
 * expected value is the straight-alpha source-over blend LVGL computes,
 * Cs*As + Cd*(1-As), with As the source alpha times the global opacity.
 */
static int aic_ge2d_probe_blend(enum ge_pd_rules rules,
                                enum mpp_pixel_format src_fmt,
                                lv_opa_t pixel_alpha, lv_opa_t global_opa,
                                const char *label)
{
    struct mpp_ge *ge = lv_draw_aic_ge2d_device();
    uint32_t src_stride = AIC_GE2D_PROBE_W * aic_ge2d_probe_bpp(src_fmt);
    uint32_t src_bytes = src_stride * AIC_GE2D_PROBE_H;
    uint32_t dst_stride = AIC_GE2D_PROBE_W * 4U;
    uint32_t dst_bytes = dst_stride * AIC_GE2D_PROBE_H;
    struct ge_bitblt blt = { 0 };
    uint8_t *src_mem;
    uint8_t *dst_mem;
    uint32_t alpha;
    uint32_t worst_channel = 0U;
    uint32_t worst_expected = 0U;
    uint32_t worst_actual = 0U;
    /* -1 so the first comparison always seeds the diagnostic triple below. */
    int worst = -1;

    if (ge == NULL) {
        AIC_TEST_E("FAIL GE device not open");
        AIC_TEST_E("probe: %s", label);
        return -1;
    }

    src_mem = aicos_malloc_align(MEM_CMA, src_bytes, CACHE_LINE_SIZE);
    dst_mem = aicos_malloc_align(MEM_CMA, dst_bytes, CACHE_LINE_SIZE);
    if (src_mem == NULL || dst_mem == NULL) {
        AIC_TEST_E("FAIL CMA probe allocation");
        AIC_TEST_E("probe: %s", label);
        aicos_free_align(MEM_CMA, src_mem);
        aicos_free_align(MEM_CMA, dst_mem);
        return -1;
    }

    aic_ge2d_probe_fill_src(src_mem, src_stride, src_fmt, pixel_alpha);
    aic_ge2d_probe_fill_dst(dst_mem, dst_stride);

    /* The engine reads the source and writes the destination, so the source is
     * cleaned and the destination cleaned-and-invalidated before the blit, then
     * invalidated again before the CPU reads it back. Same directions the draw
     * unit uses; a missing step here would measure the cache, not the engine. */
    aicos_dcache_clean_range((unsigned long *)src_mem, src_bytes);
    aicos_dcache_clean_invalid_range((unsigned long *)dst_mem, dst_bytes);

    blt.src_buf.buf_type = MPP_PHY_ADDR;
    blt.src_buf.phy_addr[0] = (uint32_t)(ulong)src_mem;
    blt.src_buf.stride[0] = src_stride;
    blt.src_buf.size.width = AIC_GE2D_PROBE_W;
    blt.src_buf.size.height = AIC_GE2D_PROBE_H;
    blt.src_buf.format = src_fmt;
    blt.src_buf.crop_en = 1U;
    blt.src_buf.crop.width = AIC_GE2D_PROBE_W;
    blt.src_buf.crop.height = AIC_GE2D_PROBE_H;

    blt.dst_buf.buf_type = MPP_PHY_ADDR;
    blt.dst_buf.phy_addr[0] = (uint32_t)(ulong)dst_mem;
    blt.dst_buf.stride[0] = dst_stride;
    blt.dst_buf.size.width = AIC_GE2D_PROBE_W;
    blt.dst_buf.size.height = AIC_GE2D_PROBE_H;
    blt.dst_buf.format = MPP_FMT_ARGB_8888;
    blt.dst_buf.crop_en = 1U;
    blt.dst_buf.crop.width = AIC_GE2D_PROBE_W;
    blt.dst_buf.crop.height = AIC_GE2D_PROBE_H;

    blt.ctrl.alpha_en = 1U;
    blt.ctrl.alpha_rules = rules;
    blt.ctrl.src_alpha_mode = 2U;
    blt.ctrl.src_global_alpha = global_opa;

    if (mpp_ge_bitblt(ge, &blt) < 0 || mpp_ge_emit(ge) < 0 || mpp_ge_sync(ge) < 0) {
        AIC_TEST_E("FAIL engine rejected probe blit");
        AIC_TEST_E("probe: %s", label);
        aicos_free_align(MEM_CMA, src_mem);
        aicos_free_align(MEM_CMA, dst_mem);
        return -1;
    }

    aicos_dcache_invalid_range((unsigned long *)dst_mem, dst_bytes);

    /* LVGL's own composition: an RGB source contributes an opaque pixel alpha,
     * an ARGB source multiplies its own alpha by the global one. */
    alpha = (src_fmt == MPP_FMT_ARGB_8888)
            ? (uint32_t)LV_OPA_MIX2(pixel_alpha, global_opa)
            : (uint32_t)global_opa;

    for (int32_t y = 0; y < AIC_GE2D_PROBE_H; y++) {
        const uint8_t *row = dst_mem + (dst_stride * (uint32_t)y);

        for (int32_t x = 0; x < AIC_GE2D_PROBE_W; x++) {
            const uint8_t *px = row + ((uint32_t)x * 4U);

            for (uint32_t c = 0; c < 3U; c++) {
                uint32_t expected = ((uint32_t)aic_ge2d_probe_src[c] * alpha +
                                     (uint32_t)aic_ge2d_probe_dst[c] * (255U - alpha) +
                                     127U) / 255U;
                /* The probe tables are indexed R, G, B while the buffers are
                 * stored blue first (lv_color32_t is {blue, green, red, alpha}),
                 * so channel c lives at byte 2 - c. Reading the bytes in index
                 * order instead turns a channel swap into a reported blend
                 * error: the first board run of this probe read a correct
                 * result as 52 counts off, which is exactly the red-versus-blue
                 * difference. Keep the diagnostic triple below for that reason -
                 * "expected 108, got 160" names the fault in one line, where a
                 * bare "52/255 off" does not. */
                int deviation = (int)px[2U - c] - (int)expected;

                if (deviation < 0) {
                    deviation = -deviation;
                }
                if (deviation > worst) {
                    worst = deviation;
                    worst_channel = c;
                    worst_expected = expected;
                    worst_actual = px[2U - c];
                }
            }
        }
    }

    aicos_free_align(MEM_CMA, src_mem);
    aicos_free_align(MEM_CMA, dst_mem);

    AIC_TEST_I("probe: %s", label);
    AIC_TEST_I("deviation=%d/255 alpha=%u %c expected=%u got=%u",
          worst, (unsigned)alpha, "RGB"[worst_channel],
          (unsigned)worst_expected, (unsigned)worst_actual);
    return worst;
}

/**
 * Measure the blend arithmetic, and prove the measurement discriminates.
 *
 * The first three cases are the shapes Phase 3C1 accepts: an RGB source with a
 * global opacity, an ARGB source with per-pixel alpha, and both at once. The
 * fourth runs the same inputs under the premultiplied rule and must NOT match -
 * if both rules matched, the probe could not tell them apart and the choice in
 * lv_draw_aic_ge2d_image.c would be unverified. Same anti-vacuity idea as the
 * counter guards below.
 */
static int aic_ge2d_blend_checks(void)
{
    int worst;
    int cross;

    worst = aic_ge2d_probe_blend(GE_PD_NONE, MPP_FMT_RGB_888, LV_OPA_COVER, 128U,
                                 "blend rgb888 + global 128");
    if (worst < 0) {
        return -1;
    }
    if (worst > AIC_GE2D_PROBE_TOLERANCE) {
        AIC_TEST_E("FAIL the rgb888 global-alpha blend is %d/255 off LVGL's result", worst);
        return -1;
    }

    worst = aic_ge2d_probe_blend(GE_PD_NONE, MPP_FMT_ARGB_8888, 128U, LV_OPA_COVER,
                                 "blend argb8888 + pixel alpha 128");
    if (worst < 0) {
        return -1;
    }
    if (worst > AIC_GE2D_PROBE_TOLERANCE) {
        AIC_TEST_E("FAIL the argb8888 per-pixel blend is %d/255 off LVGL's result", worst);
        return -1;
    }

    worst = aic_ge2d_probe_blend(GE_PD_NONE, MPP_FMT_ARGB_8888, 200U, 64U,
                                 "blend argb8888 + pixel 200 x global 64");
    if (worst < 0) {
        return -1;
    }
    if (worst > AIC_GE2D_PROBE_TOLERANCE) {
        AIC_TEST_E("FAIL the combined pixel x global blend is %d/255 off LVGL's result", worst);
        return -1;
    }

    cross = aic_ge2d_probe_blend(GE_PD_SRC_OVER, MPP_FMT_RGB_888, LV_OPA_COVER, 128U,
                                 "cross-check the premultiplied rule");
    if (cross < 0) {
        return -1;
    }
    if (cross <= AIC_GE2D_PROBE_TOLERANCE) {
        AIC_TEST_E("FAIL cross-check=%d/255; alpha rules are indistinguishable", cross);
        return -1;
    }

    AIC_TEST_I("PASS GE2D blend: three cases within %d/255; cross-check=%d/255",
          (int)AIC_GE2D_PROBE_TOLERANCE, cross);
    return 0;
}

#endif /* AIC_GE2D_IMAGE_PROBES */

int lv_aic_ge2d_test_run(void)
{
    const lv_draw_aic_ge2d_stats_t *stats;
    lv_draw_aic_ge2d_stats_t before;
    uint32_t fill, fill_done;
    uint32_t image, image_done, image_sw;
    uint32_t layer, layer_done, layer_sw;
    uint32_t fallback, errors;
    int fill_result;
    unsigned long long refresh_start_us, refresh_elapsed_us;

    stats = lv_draw_aic_ge2d_stats();
    if (!stats->ready) {
        /* Not a code failure: the GE block is not open, so the unit declines
         * every task. Without the driver there is nothing to validate. */
        AIC_TEST_E("FAIL GE2D device unavailable: mpp_ge_open() failed; check the GE driver");
        return -1;
    }

    /* Positive fill results are soft measurement flags (bit 0: RGB565 color
     * key). The engine itself is healthy, so the YUV, video-window, counter
     * and refresh blocks must still be evaluated; the soft failure is
     * reported at the end of this function instead of hiding them. */
    fill_result = lv_aic_ge2d_fill_test_run();
    if (fill_result < 0) return -1;
    if (lv_aic_yuv_test_run() != 0) return -1;
#if LV_USE_VECTOR_GRAPHIC
    if (lv_aic_vector_surface_test_run() != 0) return -1;
#endif
#if LV_USE_SVG
    if (lv_aic_svg_document_test_run() != 0) return -1;
#endif

#if AIC_LVGL_USE_VIDEO_WINDOW
    if (lv_aic_video_window_test_run() != 0) return -1;
#endif
    before = *stats;

    /* Force a full redraw so every shape on the page is re-submitted, then
     * render it synchronously. */
    refresh_start_us = aic_get_time_us();
    lv_obj_invalidate(lv_screen_active());
    lv_refr_now(lv_display_get_default());
    refresh_elapsed_us = aic_get_time_us() - refresh_start_us;

    fill = stats->fill_accepted - before.fill_accepted;
    fill_done = stats->fill_completed - before.fill_completed -
                (stats->fill_sw_fallback - before.fill_sw_fallback);
    image = stats->image_accepted - before.image_accepted;
    image_done = stats->image_completed - before.image_completed;
    image_sw = stats->image_sw_fallback - before.image_sw_fallback;
    layer = stats->layer_accepted - before.layer_accepted;
    layer_done = stats->layer_completed - before.layer_completed;
    layer_sw = stats->layer_sw_fallback - before.layer_sw_fallback;
    fallback = stats->fallback - before.fallback;
    errors = stats->errors - before.errors;

    AIC_TEST_I("ge2d accepted fill=%u image=%u layer=%u",
          (unsigned)fill, (unsigned)image, (unsigned)layer);
    AIC_TEST_I("ge2d engine fill=%u image=%u layer=%u",
          (unsigned)fill_done, (unsigned)(image_done - image_sw),
          (unsigned)(layer_done - layer_sw));
    AIC_TEST_I("ge2d sw fill=%u", (unsigned)(stats->fill_sw_fallback-before.fill_sw_fallback));
    AIC_TEST_I("ge2d sw image=%u layer=%u declined=%u errors=%u",
          (unsigned)image_sw, (unsigned)layer_sw,
          (unsigned)fallback, (unsigned)errors);
    /* RT-Thread's small printf does not necessarily implement %llu. Split
     * into seconds and a bounded microsecond remainder without truncating it. */
    AIC_TEST_I("refresh timing s=%u us_part=%06u",
          (unsigned)(refresh_elapsed_us / 1000000U),
          (unsigned)(refresh_elapsed_us % 1000000U));
    AIC_TEST_I("ge2d_ready=%u", stats->ready ? 1U : 0U);

    if (errors != 0U) {
        AIC_TEST_E("FAIL GE2D reported %u execution errors", (unsigned)errors);
        return -1;
    }

    /* Solid, square-cornered rectangles. fill_done excludes SW fallback. */
    if (fill < AIC_GE2D_MIN_FILL) {
        AIC_TEST_E("FAIL only %u solid FILL tasks reached GE2D, expected >= %u",
              (unsigned)fill, (unsigned)AIC_GE2D_MIN_FILL);
        return -1;
    }
    if (fill_done != fill) {
        AIC_TEST_E("FAIL fill accepted=%u but completed=%u", (unsigned)fill,
              (unsigned)fill_done);
        return -1;
    }

#if AIC_GE2D_IMAGE_PROBES
    /* Phase 3B: the unscaled b.png (RGB) and c.png (RGBA) probes. The decoder
     * allocates from CMA, which is inside the GE address window, so these must
     * be drawn by the engine and not merely claimed. */
    if (image == 0U) {
        AIC_TEST_E("FAIL no IMAGE task reached GE2D; expected the unscaled b.png/c.png probes");
        return -1;
    }
    if (image_done != image) {
        AIC_TEST_E("FAIL image accepted=%u but completed=%u", (unsigned)image,
              (unsigned)image_done);
        return -1;
    }
    if (image_sw != 0U) {
        AIC_TEST_E("FAIL %u of %u accepted IMAGE tasks were drawn in software, not by the engine",
              (unsigned)image_sw, (unsigned)image);
        return -1;
    }

    /* Phase 3C1: the blend arithmetic itself. Everything above proves the
     * engine ran; this proves it computed LVGL's result, which no counter can
     * see. It runs offscreen, so it does not depend on what is on the page. */
    if (aic_ge2d_blend_checks() != 0) {
        return -1;
    }
    if (lv_aic_ge2d_scale_test_run() != 0) {
        return -1;
    }
    if (stats->scaled_image_engine == before.scaled_image_engine) {
        AIC_TEST_E("FAIL no scaled IMAGE task was engine-drawn by the scheduler");
        return -1;
    }
    AIC_TEST_I("scale scheduler engine=%u errors=%u",
          (unsigned)(stats->scaled_image_engine - before.scaled_image_engine), (unsigned)errors);
#endif /* AIC_GE2D_IMAGE_PROBES */

    /* Phase 3B: the opa_layered rectangle, which LVGL composites through a
     * LAYER task.
     *
     * Require claim and completion. CMA now enables hardware composition,
     * but budget pressure can still use heap/software fallback. The separate
     * engine count reports which happened; numeric board acceptance remains
     * distinct from scheduler completion. */
    if (layer == 0U) {
        AIC_TEST_E("FAIL no LAYER task reached GE2D; expected the opa_layered probe");
        return -1;
    }
    if (layer_done != layer) {
        AIC_TEST_E("FAIL layer accepted=%u but completed=%u", (unsigned)layer,
              (unsigned)layer_done);
        return -1;
    }

    /* Still declining the rounded rectangle. The Phase 2 row now also
     * exercises the scaler through its 4x PNG images. */
    if (fallback < 1U) {
        AIC_TEST_E("FAIL nothing fell back to software; the fallback probes are missing");
        return -1;
    }

    if (fill_result > 0) {
        AIC_TEST_E("FAIL GE2D key565 measurement inconclusive; later blocks still evaluated");
        return -1;
    }
    AIC_TEST_I("PASS GE2D fill=%u image=%u layer=%u hw_layer=%u sw_layer=%u declined=%u",
          (unsigned)fill_done, (unsigned)(image_done - image_sw),
          (unsigned)layer, (unsigned)(layer_done - layer_sw), (unsigned)layer_sw,
          (unsigned)fallback);
    return 0;
}
#endif
