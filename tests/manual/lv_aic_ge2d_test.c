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
 * many frames the smoke loop has already rendered. */
#define AIC_GE2D_MIN_ACCEPTED 8U /* large + medium + 6 small */

int lv_aic_ge2d_test_run(void)
{
    const lv_draw_aic_ge2d_stats_t *stats;
    uint32_t accepted_before, completed_before, fallback_before, errors_before;
    uint32_t accepted, completed, fallback, errors;

    stats = lv_draw_aic_ge2d_stats();
    if (!stats->ready) {
        /* Not a code failure: the GE block is not open, so the unit declines
         * every task. Without the driver there is nothing to validate. */
        LOG_E("FAIL GE2D device unavailable: mpp_ge_open() failed; check the GE driver");
        return -1;
    }

    accepted_before = stats->fill_accepted;
    completed_before = stats->fill_completed;
    fallback_before = stats->fallback;
    errors_before = stats->errors;

    /* Force a full redraw so every shape on the page is re-submitted, then
     * render it synchronously. */
    lv_obj_invalidate(lv_screen_active());
    lv_refr_now(lv_display_get_default());

    accepted = stats->fill_accepted - accepted_before;
    completed = stats->fill_completed - completed_before;
    fallback = stats->fallback - fallback_before;
    errors = stats->errors - errors_before;

    LOG_I("ge2d fill accepted=%u completed=%u fallback=%u errors=%u",
          (unsigned)accepted, (unsigned)completed,
          (unsigned)fallback, (unsigned)errors);

    if (errors != 0U) {
        LOG_E("FAIL GE2D reported %u execution errors", (unsigned)errors);
        return -1;
    }
    if (accepted < AIC_GE2D_MIN_ACCEPTED) {
        LOG_E("FAIL only %u opaque FILL tasks reached GE2D, expected >= %u",
              (unsigned)accepted, (unsigned)AIC_GE2D_MIN_ACCEPTED);
        return -1;
    }
    if (completed != accepted) {
        LOG_E("FAIL accepted=%u but completed=%u", (unsigned)accepted,
              (unsigned)completed);
        return -1;
    }
    if (fallback < 1U) {
        LOG_E("FAIL the rounded rectangle did not fall back to software");
        return -1;
    }

    LOG_I("PASS GE2D opaque fill: %u rectangles accelerated, %u fell back to software",
          (unsigned)completed, (unsigned)fallback);
    return 0;
}
#endif
