/**
 * @file lv_draw_aic_ge2d.c
 * @brief ArtInChip GE2D draw unit: registration, evaluation and dispatch.
 *
 * Phase 3A scope is deliberately narrow: opaque, unrounded, non-gradient FILL
 * tasks whose destination buffer the GE block can address. The unit runs
 * synchronously on the dispatching thread - there is no render thread, no task
 * queue and no saved layer/clip state, which is why it does not reuse
 * lv_draw_sw_unit_t the way the legacy port did.
 *
 * Two departures from the legacy port are the reason this file exists
 * separately and are easy to regress:
 *   1. evaluate() reports acceptance (returns 1), not 0.
 *   2. a GE failure marks the task FAILED, never FINISHED. A rectangle the
 *      engine did not draw must not be reported as drawn.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#define AIC_LVGL_USE_PRIVATE_API 1
#include "lv_draw_aic_ge2d.h"
#include "lv_draw_aic_ge2d_utils.h"

#if AIC_LVGL_USE_GE2D && AIC_LVGL_BSP_MPP

#include "lvgl_aic_private.h"

#include <mpp_ge.h>

/**
 * Draw unit id. LVGL 9.6 keeps these as per-backend local defines; 10 is unused
 * by the backends shipped with this SDK (SW 1, PXP 3, DAVE2D 4, DMA2D 5,
 * OPENGLES 6, NEMA 7, G2D 8, EVE 9, SIFLI 11, PPA 80, SDL 100). It matches the
 * id the vendor GE2D port used, so the two stay interchangeable if both are
 * ever enabled at the same time during bring-up.
 */
#define AIC_GE2D_DRAW_UNIT_ID 10

/**
 * Preference score handed to accepted FILL tasks. Lower wins. The software
 * renderer claims a task at 100, so anything below 100 makes GE2D win; 70
 * matches both the vendor-validated port and the upstream LVGL G2D backend.
 */
#define AIC_GE2D_PREFERENCE_SCORE 70

static struct mpp_ge *g_ge2d_dev;
static bool g_ge2d_ready;
static bool g_ge2d_registered;
static lv_draw_aic_ge2d_stats_t g_ge2d_stats;

static int32_t lv_draw_aic_ge2d_evaluate(lv_draw_unit_t *unit, lv_draw_task_t *task);
static int32_t lv_draw_aic_ge2d_dispatch(lv_draw_unit_t *unit, lv_layer_t *layer);
static int32_t lv_draw_aic_ge2d_delete(lv_draw_unit_t *unit);

/**
 * True when Phase 3A can render @p task with GE2D.
 *
 * Every rejection here is a case the software renderer already handles, so a
 * "no" is a fallback rather than a failure.
 */
static bool lv_draw_aic_ge2d_accepts(const lv_draw_task_t *task)
{
    const lv_draw_fill_dsc_t *dsc;
    const lv_layer_t *layer;
    const lv_draw_buf_t *draw_buf;

    dsc = (const lv_draw_fill_dsc_t *)task->draw_dsc;
    if (dsc == NULL) {
        return false;
    }

    /* Rounded rectangles need per-corner masking; fillrect cannot do it. */
    if (dsc->radius != 0) {
        return false;
    }

    /* A gradient needs a colour ramp the fillrect descriptor does not carry. */
    if (dsc->grad.dir != (lv_grad_dir_t)LV_GRAD_DIR_NONE) {
        return false;
    }

    /* Phase 3A has no source alpha and no destination read-modify-write, so a
     * translucent fill would be silently wrong. */
    if (dsc->opa != LV_OPA_COVER) {
        return false;
    }

    /* target_layer is the layer the task will actually be drawn into. It is
     * fixed at task creation, unlike layer->_clip_area which may already
     * describe a later task by the time we run. */
    layer = task->target_layer;
    if (layer == NULL || layer->draw_buf == NULL) {
        return false;
    }
    draw_buf = layer->draw_buf;

    if (!lv_draw_aic_ge2d_dst_format_supported((lv_color_format_t)draw_buf->header.cf)) {
        return false;
    }

    if (!lv_draw_aic_ge2d_buf_address_valid(draw_buf)) {
        return false;
    }

    return true;
}

void lv_draw_aic_ge2d_init(void)
{
    lv_draw_aic_ge2d_unit_t *unit;

    /* The device is opened on EVERY init, not only the first one. The smoke
     * application runs LVGL deinit/init cycles, and lv_draw_aic_ge2d_deinit()
     * closes the device while LVGL keeps the unit in its draw-unit list (there
     * is no API to unregister one). Guarding the open on g_ge2d_registered left
     * the device closed from the second cycle onwards, so every task silently
     * fell back to software. */
    if (g_ge2d_dev == NULL) {
        g_ge2d_dev = mpp_ge_open();
    }
    g_ge2d_ready = (g_ge2d_dev != NULL);
    g_ge2d_stats.ready = g_ge2d_ready;

    if (!g_ge2d_ready) {
        /* The unit is still registered: it declines every task, so the software
         * renderer keeps the display working instead of the unit dispatching
         * into a NULL device. */
        LV_LOG_ERROR("GE2D device unavailable; the GE2D unit will decline every task");
    }

    if (g_ge2d_registered) {
        /* Device reopened; the unit is already in LVGL's draw-unit list. */
        return;
    }

    unit = (lv_draw_aic_ge2d_unit_t *)lv_draw_create_unit(sizeof(lv_draw_aic_ge2d_unit_t));
    if (unit == NULL) {
        LV_LOG_ERROR("cannot register the GE2D draw unit");
        return;
    }

    unit->base_unit.evaluate_cb = lv_draw_aic_ge2d_evaluate;
    unit->base_unit.dispatch_cb = lv_draw_aic_ge2d_dispatch;
    unit->base_unit.delete_cb = lv_draw_aic_ge2d_delete;
    unit->base_unit.name = "AIC_GE2D";
    unit->task_act = NULL;

    g_ge2d_registered = true;
}

void lv_draw_aic_ge2d_deinit(void)
{
    if (g_ge2d_dev != NULL) {
        mpp_ge_close(g_ge2d_dev);
        g_ge2d_dev = NULL;
    }
    g_ge2d_ready = false;
    g_ge2d_stats.ready = false;
}

struct mpp_ge *lv_draw_aic_ge2d_device(void)
{
    return g_ge2d_dev;
}

const lv_draw_aic_ge2d_stats_t *lv_draw_aic_ge2d_stats(void)
{
    return &g_ge2d_stats;
}

void lv_draw_aic_ge2d_stats_reset(void)
{
    bool ready = g_ge2d_stats.ready;

    g_ge2d_stats.fill_accepted = 0U;
    g_ge2d_stats.fill_completed = 0U;
    g_ge2d_stats.fallback = 0U;
    g_ge2d_stats.errors = 0U;
    g_ge2d_stats.ready = ready;
}

static int32_t lv_draw_aic_ge2d_evaluate(lv_draw_unit_t *unit, lv_draw_task_t *task)
{
    LV_UNUSED(unit);

    if (!g_ge2d_ready || task->type != LV_DRAW_TASK_TYPE_FILL) {
        return 0;
    }

    if (!lv_draw_aic_ge2d_accepts(task)) {
        /* Not ours. The software renderer keeps the task; not an error. */
        g_ge2d_stats.fallback++;
        return 0;
    }

    if (task->preference_score > AIC_GE2D_PREFERENCE_SCORE) {
        task->preference_score = AIC_GE2D_PREFERENCE_SCORE;
        task->preferred_draw_unit_id = AIC_GE2D_DRAW_UNIT_ID;
    }
    g_ge2d_stats.fill_accepted++;
    return 1;
}

static int32_t lv_draw_aic_ge2d_dispatch(lv_draw_unit_t *unit, lv_layer_t *layer)
{
    lv_draw_aic_ge2d_unit_t *ge2d = (lv_draw_aic_ge2d_unit_t *)unit;
    lv_draw_task_t *task;
    lv_result_t result;

    /* Synchronous unit: exactly one task in flight. */
    if (ge2d->task_act != NULL) {
        return LV_DRAW_UNIT_IDLE;
    }

    task = lv_draw_get_available_task(layer, NULL, AIC_GE2D_DRAW_UNIT_ID);
    if (task == NULL || task->preferred_draw_unit_id != AIC_GE2D_DRAW_UNIT_ID) {
        return LV_DRAW_UNIT_IDLE;
    }

    /* Make sure the layer has a buffer before the engine is pointed at it. */
    if (lv_draw_layer_alloc_buf(layer) == NULL) {
        task->state = LV_DRAW_TASK_STATE_FAILED;
        g_ge2d_stats.errors++;
        return LV_DRAW_UNIT_IDLE;
    }

    task->state = LV_DRAW_TASK_STATE_IN_PROGRESS;
    task->draw_unit = unit;
    ge2d->task_act = task;

    result = lv_draw_aic_ge2d_fill(task);

    if (result == LV_RESULT_OK) {
        ge2d->task_act->state = LV_DRAW_TASK_STATE_FINISHED;
        g_ge2d_stats.fill_completed++;
    }
    else {
        ge2d->task_act->state = LV_DRAW_TASK_STATE_FAILED;
        g_ge2d_stats.errors++;
    }

    ge2d->task_act = NULL;

    /* Free again: ask the scheduler to hand over the next task. */
    lv_draw_dispatch_request();

    return 1;
}

static int32_t lv_draw_aic_ge2d_delete(lv_draw_unit_t *unit)
{
    LV_UNUSED(unit);

    lv_draw_aic_ge2d_deinit();
    g_ge2d_registered = false;

    return LV_RESULT_OK;
}

#endif /* AIC_LVGL_USE_GE2D && AIC_LVGL_BSP_MPP */
