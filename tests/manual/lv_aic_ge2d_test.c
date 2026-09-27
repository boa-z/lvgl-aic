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
#include <ulog.h>

/* The manual page owns the shapes; this only measures the counters around one
 * forced full refresh. Counts are deltas, so the verdict is independent of how
 * many frames the smoke loop has already rendered.
 *
 * These are anti-vacuity guards, not correctness checks. In particular a
 * completed count does not distinguish a task the engine drew from one it
 * handed back to the software renderer, so "accepted == completed" proves the
 * scheduler ran the task to completion, not that the pixels are right. The
 * engine-drawn numbers below are computed as completed minus sw_fallback, which
 * is the only pair of counters that can tell the two apart. Whether the ARGB
 * blend and the 50% layer composite look correct is still a panel question, and
 * is stated as such in the phase record. */
#define AIC_GE2D_MIN_FILL 8U /* large + medium + 6 small */

/* The image probes reuse the Phase 2 MPP fixtures, so they only exist when the
 * decoder is also enabled. The LAYER probe needs the draw unit alone. */
#if AIC_LVGL_USE_MPP_DEC
#define AIC_GE2D_IMAGE_PROBES 1
#else
#define AIC_GE2D_IMAGE_PROBES 0
#endif

int lv_aic_ge2d_test_run(void)
{
    const lv_draw_aic_ge2d_stats_t *stats;
    lv_draw_aic_ge2d_stats_t before;
    uint32_t fill, fill_done;
    uint32_t image, image_done, image_sw;
    uint32_t layer, layer_done, layer_sw;
    uint32_t fallback, errors;

    stats = lv_draw_aic_ge2d_stats();
    if (!stats->ready) {
        /* Not a code failure: the GE block is not open, so the unit declines
         * every task. Without the driver there is nothing to validate. */
        LOG_E("FAIL GE2D device unavailable: mpp_ge_open() failed; check the GE driver");
        return -1;
    }

    before = *stats;

    /* Force a full redraw so every shape on the page is re-submitted, then
     * render it synchronously. */
    lv_obj_invalidate(lv_screen_active());
    lv_refr_now(lv_display_get_default());

    fill = stats->fill_accepted - before.fill_accepted;
    fill_done = stats->fill_completed - before.fill_completed;
    image = stats->image_accepted - before.image_accepted;
    image_done = stats->image_completed - before.image_completed;
    image_sw = stats->image_sw_fallback - before.image_sw_fallback;
    layer = stats->layer_accepted - before.layer_accepted;
    layer_done = stats->layer_completed - before.layer_completed;
    layer_sw = stats->layer_sw_fallback - before.layer_sw_fallback;
    fallback = stats->fallback - before.fallback;
    errors = stats->errors - before.errors;

    LOG_I("ge2d accepted fill=%u image=%u layer=%u | engine fill=%u image=%u layer=%u | sw_fallback image=%u layer=%u | declined=%u errors=%u",
          (unsigned)fill, (unsigned)image, (unsigned)layer,
          (unsigned)fill_done, (unsigned)(image_done - image_sw),
          (unsigned)(layer_done - layer_sw),
          (unsigned)image_sw, (unsigned)layer_sw,
          (unsigned)fallback, (unsigned)errors);

    if (errors != 0U) {
        LOG_E("FAIL GE2D reported %u execution errors", (unsigned)errors);
        return -1;
    }

    /* Phase 3A: the opaque, square-cornered rectangles. The fill path has no
     * fallback, so completed implies the engine drew it. */
    if (fill < AIC_GE2D_MIN_FILL) {
        LOG_E("FAIL only %u opaque FILL tasks reached GE2D, expected >= %u",
              (unsigned)fill, (unsigned)AIC_GE2D_MIN_FILL);
        return -1;
    }
    if (fill_done != fill) {
        LOG_E("FAIL fill accepted=%u but completed=%u", (unsigned)fill,
              (unsigned)fill_done);
        return -1;
    }

#if AIC_GE2D_IMAGE_PROBES
    /* Phase 3B: the unscaled b.png (RGB) and c.png (RGBA) probes. The decoder
     * allocates from CMA, which is inside the GE address window, so these must
     * be drawn by the engine and not merely claimed. */
    if (image == 0U) {
        LOG_E("FAIL no IMAGE task reached GE2D; expected the unscaled b.png/c.png probes");
        return -1;
    }
    if (image_done != image) {
        LOG_E("FAIL image accepted=%u but completed=%u", (unsigned)image,
              (unsigned)image_done);
        return -1;
    }
    if (image_sw != 0U) {
        LOG_E("FAIL %u of %u accepted IMAGE tasks were drawn in software, not by the engine",
              (unsigned)image_sw, (unsigned)image);
        return -1;
    }
#endif /* AIC_GE2D_IMAGE_PROBES */

    /* Phase 3B: the opa_layered rectangle, which LVGL composites through a
     * LAYER task.
     *
     * Only the CLAIM is asserted. The engine-drawn count is reported but not
     * required to be non-zero: LVGL allocates layer buffers with lv_malloc,
     * which on this SDK is the RT-Thread system heap at 0x30040000 - below the
     * 0x40000000 floor the D13x GE can reach - so the composite is expected to
     * fall back to software here. Asserting engine work for LAYER would fail on
     * correct code, and asserting nothing at all would let a dropped layer go
     * unnoticed. Claimed-and-completed, with the split reported, is what is
     * actually true on this board. */
    if (layer == 0U) {
        LOG_E("FAIL no LAYER task reached GE2D; expected the opa_layered probe");
        return -1;
    }
    if (layer_done != layer) {
        LOG_E("FAIL layer accepted=%u but completed=%u", (unsigned)layer,
              (unsigned)layer_done);
        return -1;
    }

    /* Still declining what it cannot do: the rounded rectangle, and the two
     * images the Phase 2 row scales by 4x. */
    if (fallback < 1U) {
        LOG_E("FAIL nothing fell back to software; the fallback probes are missing");
        return -1;
    }

    LOG_I("PASS GE2D: %u fills and %u images drawn by the engine; "
          "%u layer task(s) claimed, %u drawn by the engine and %u composited in software; "
          "%u declined up front",
          (unsigned)fill_done, (unsigned)(image_done - image_sw),
          (unsigned)layer, (unsigned)(layer_done - layer_sw), (unsigned)layer_sw,
          (unsigned)fallback);
    return 0;
}
#endif
