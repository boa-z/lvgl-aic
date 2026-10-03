/**
 * @file lv_draw_aic_ge2d.c
 * @brief ArtInChip GE2D draw unit: registration, evaluation and dispatch.
 *
 * Scope is deliberately narrow. FILL: solid, unrounded, non-gradient tasks.
 * IMAGE: rotated or native-size tiled, unrecolored RGB copies/scales -
 * the blit blends, so a partial opacity is supported rather than declined.
 * LAYER: the same blit, fed from a child layer's buffer instead of a decoded
 * image, including bounded scaling, right-angle rotation and unscaled
 * arbitrary-angle rotation. Everything else is declined, and a declined task
 * simply stays with the software renderer.
 * The unit runs synchronously on the dispatching thread -
 * there is no render thread, no task queue and no saved layer/clip state, which
 * is why it does not reuse lv_draw_sw_unit_t the way the legacy port did.
 *
 * Two departures from the legacy port are the reason this file exists
 * separately and are easy to regress:
 *   1. evaluate() reports acceptance (returns 1), not 0.
 *   2. a GE failure never marks the task FINISHED. Ordinary failures become
 *      FAILED; a quarantined YUV DMA fault retains IN_PROGRESS and its layer
 *      until reboot because DMA completion has not been established.
 *
 * A third point is specific to this file: the counters separate "the unit
 * claimed the task" from "the engine drew it". A completed count alone would
 * let a log line claim acceleration that never happened, because the executor
 * can still succeed by handing the task to the software renderer. The
 * *_sw_fallback pair is what makes that visible.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#define AIC_LVGL_USE_PRIVATE_API 1
#include "lv_draw_aic_ge2d.h"
#include "lv_draw_aic_ge2d_utils.h"
#include "lv_aic_fake_image.h"
#include "lv_draw_aic_ge2d_yuv.h"

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
static bool g_ge2d_external_fault;
static bool g_ge2d_registered;
static lv_draw_aic_ge2d_stats_t g_ge2d_stats;

static int32_t lv_draw_aic_ge2d_evaluate(lv_draw_unit_t *unit, lv_draw_task_t *task);
static int32_t lv_draw_aic_ge2d_dispatch(lv_draw_unit_t *unit, lv_layer_t *layer);
static int32_t lv_draw_aic_ge2d_delete(lv_draw_unit_t *unit);

/**
 * True when @p task's target layer can serve as a GE2D destination.
 *
 * Shared by all three task types: FILL, IMAGE and LAYER all write into
 * task->target_layer, so they all need the same format and address guarantees.
 * Keeping the check in one place is what stops a later task type from quietly
 * skipping one of them.
 */
static bool lv_draw_aic_ge2d_accepts_dst(const lv_draw_task_t *task)
{
    const lv_layer_t *layer;
    const lv_draw_buf_t *draw_buf;

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

    return lv_draw_aic_ge2d_buf_address_valid(draw_buf);
}

/**
 * True when @p dsc carries a supported IMAGE or LAYER transform.
 *
 * Arbitrary angles use ROTATE without scaling; orthogonal transforms use BITBLT.
 */
static bool lv_draw_aic_ge2d_dsc_is_supported_transform(const lv_draw_image_dsc_t *dsc)
{
    return ((dsc->rotation == 0 || dsc->rotation == 900 ||
             dsc->rotation == 1800 || dsc->rotation == 2700) ||
            (dsc->scale_x == LV_SCALE_NONE && dsc->scale_y == LV_SCALE_NONE)) &&
           dsc->scale_x >= LV_SCALE_NONE / 16 &&
           dsc->scale_x <= LV_SCALE_NONE * 16 &&
           dsc->scale_y >= LV_SCALE_NONE / 16 &&
           dsc->scale_y <= LV_SCALE_NONE * 16 &&
           dsc->skew_x == 0 && dsc->skew_y == 0;
}

/**
 * True when @p dsc needs nothing beyond a straight copy.
 *
 * Shared by IMAGE and LAYER: recolor, masks, rounded clips and non-normal
 * blends remain software work. BITBLT supports an exact RGB color key under
 * the transform restrictions below.
 * Keeping the list here is what stops the two task types from drifting apart.
 */
static bool lv_draw_aic_ge2d_dsc_is_plain(const lv_draw_image_dsc_t *dsc)
{
    if (dsc->recolor_opa > LV_OPA_MIN) {
        return false;
    }
    if (dsc->bitmap_mask_src != NULL || dsc->clip_radius != 0) {
        return false;
    }
    if (dsc->colorkey != NULL) {
        /* GE compares one RGB value; LVGL also supports a component range.
         * Filtering before keying changes matches, so keep scaled/ROTATE
         * requests on software. The executor checks the decoded format. */
        if (!lv_color_eq(dsc->colorkey->low, dsc->colorkey->high) ||
            dsc->scale_x != LV_SCALE_NONE || dsc->scale_y != LV_SCALE_NONE ||
            dsc->rotation % 900 != 0) return false;
    }

    return dsc->blend_mode == LV_BLEND_MODE_NORMAL;
}

/**
 * True when the fill step can render @p task with GE2D.
 *
 * Every rejection here is a case the software renderer already handles, so a
 * "no" is a fallback rather than a failure.
 */
static bool lv_draw_aic_ge2d_accepts_fill(const lv_draw_task_t *task)
{
    const lv_draw_fill_dsc_t *dsc;

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

    if (dsc->opa <= LV_OPA_MIN || !lv_draw_aic_ge2d_accepts_dst(task)) {
        return false;
    }

    /* Partial fills on alpha-bearing destinations need separate composition
     * validation. Opaque destinations use straight-alpha source-over. */
    return dsc->opa >= LV_OPA_MAX ||
           task->target_layer->draw_buf->header.cf != LV_COLOR_FORMAT_ARGB8888;
}

/**
 * True when the plain-blit step can render @p task with GE2D.
 *
 * The source is deliberately NOT checked here: it only exists after the decode,
 * which happens in the executor. A source the engine cannot read is handled
 * there by falling back, not by declining the task up front.
 */
static bool lv_draw_aic_ge2d_accepts_image(const lv_draw_task_t *task)
{
    const lv_draw_image_dsc_t *dsc;

    dsc = (const lv_draw_image_dsc_t *)task->draw_dsc;
    if (dsc == NULL || dsc->src == NULL) {
        return false;
    }

    if (!lv_draw_aic_ge2d_dsc_is_supported_transform(dsc)) {
        return false;
    }

    if (!lv_draw_aic_ge2d_dsc_is_plain(dsc)) {
        return false;
    }

    /* Partial opacity is the operation, not a reason to decline: the blit
     * blends the source's per-pixel alpha with the descriptor's global alpha
     * through one Porter/Duff rule, which covers RGB + global alpha and
     * ARGB + per-pixel x global alpha alike. Only an image that is effectively
     * invisible is left to the software renderer, at the same floor LVGL's own
     * renderer and the vendor port use. */
    if (dsc->opa <= LV_OPA_MIN) {
        return false;
    }

    return lv_draw_aic_ge2d_accepts_dst(task);
}

/**
 * True when the layer-blend step can render @p task with GE2D.
 *
 * A LAYER task carries an lv_layer_t where an image carries its source, so the
 * unit wraps that layer's buffer and reuses the image blit. Two things differ
 * from an IMAGE task and are the reason this is a separate function rather than
 * a shared branch:
 *
 *   - the source buffer belongs to the CHILD layer and does not exist yet at
 *     evaluation time. LVGL allocates it when the first task draws into it, and
 *     never allocates it at all when nothing was drawn on the layer. It is
 *     therefore not checked here, exactly like a decoded image source; the
 *     executor handles a missing buffer by completing the task with nothing to
 *     draw.
 *   - the layer's own opacity is applied as GE global alpha, so a partial
 *     opacity is expected rather than a reason to decline. LVGL only creates a
 *     LAYER task for a transform or for a partial layer opacity, so declining
 *     partial opacity would decline nearly every layer.
 */
static bool lv_draw_aic_ge2d_accepts_layer(const lv_draw_task_t *task)
{
    const lv_draw_image_dsc_t *dsc;
    const lv_layer_t *layer_to_draw;

    dsc = (const lv_draw_image_dsc_t *)task->draw_dsc;
    if (dsc == NULL || dsc->src == NULL) {
        return false;
    }

    layer_to_draw = (const lv_layer_t *)dsc->src;
    if (layer_to_draw == NULL) {
        return false;
    }

    if (!lv_draw_aic_ge2d_dsc_is_supported_transform(dsc)) {
        return false;
    }

    /* LVGL never tiles a layer, so this cannot currently trigger. It is here
     * because lv_draw_sw_layer() would honour a tiled descriptor and this path
     * cannot, and a silent divergence is worse than a declined task. */
    if (dsc->tile != 0) {
        return false;
    }

    if (!lv_draw_aic_ge2d_dsc_is_plain(dsc)) {
        return false;
    }

    return lv_draw_aic_ge2d_accepts_dst(task);
}

void lv_draw_aic_ge2d_init(void)
{
    lv_draw_aic_ge2d_unit_t *unit;

    if (lv_draw_aic_ge2d_faulted()) {
        LV_LOG_ERROR("GE DMA fault: reinitialization requires reboot");
        return;
    }

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
        /* Ordinary tasks stay with SW. SDK pseudo-images retain this unit's
         * software fill handler so replacement semantics remain available. */
        LV_LOG_ERROR("GE2D device unavailable; using software rendering");
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

bool lv_draw_aic_ge2d_faulted(void)
{
    return g_ge2d_external_fault || lv_draw_aic_ge2d_fill_faulted() ||
           lv_draw_aic_ge2d_yuv_faulted() || lv_draw_aic_ge2d_image_faulted();
}

void lv_draw_aic_ge2d_quarantine(void)
{
    g_ge2d_external_fault = true;
    g_ge2d_stats.ready = false;
}

void lv_draw_aic_ge2d_deinit(void)
{
    if (lv_draw_aic_ge2d_faulted()) {
        /* close frees the SDK client without proving DMA has stopped. Keep
         * it alive. This cannot make LVGL display/object teardown safe. */
        g_ge2d_stats.ready = false;
        LV_LOG_ERROR("GE DMA fault: retaining client until reboot");
        return;
    }
    if (g_ge2d_dev != NULL) {
        mpp_ge_close(g_ge2d_dev);
        g_ge2d_dev = NULL;
    }
    g_ge2d_ready = false;
    g_ge2d_stats.ready = false;
}

struct mpp_ge *lv_draw_aic_ge2d_device(void)
{
    return lv_draw_aic_ge2d_faulted() ? NULL : g_ge2d_dev;
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
    g_ge2d_stats.image_accepted = 0U;
    g_ge2d_stats.image_completed = 0U;
    g_ge2d_stats.scaled_image_engine = 0U;
    g_ge2d_stats.image_sw_fallback = 0U;
    g_ge2d_stats.layer_accepted = 0U;
    g_ge2d_stats.layer_completed = 0U;
    g_ge2d_stats.layer_sw_fallback = 0U;
    g_ge2d_stats.fallback = 0U;
    g_ge2d_stats.errors = 0U;
    g_ge2d_stats.ready = ready;
}

static int32_t lv_draw_aic_ge2d_evaluate(lv_draw_unit_t *unit, lv_draw_task_t *task)
{
    bool accepted;
    lv_aic_fake_image_t fake;
    const lv_draw_image_dsc_t *image = task->type == LV_DRAW_TASK_TYPE_IMAGE ? task->draw_dsc : NULL;
    bool is_fake = image && image->src && lv_image_src_get_type(image->src) == LV_IMAGE_SRC_FILE &&
                   lv_aic_fake_image_parse(image->src, &fake);

    LV_UNUSED(unit);

    if (!g_ge2d_ready && !is_fake) {
        return 0;
    }

    /* evaluate() runs exactly once per task, at creation time, for EVERY task
     * type. Returning before the counters below is what makes them an exact
     * census of the types this unit handles, instead of a count of every draw
     * task in the frame. */
    switch (task->type) {
    case LV_DRAW_TASK_TYPE_FILL:
        accepted = lv_draw_aic_ge2d_accepts_fill(task);
        break;
    case LV_DRAW_TASK_TYPE_IMAGE:
        /* SDK pseudo-images need our fill semantics even if GE is unavailable.
         * The executor can perform the same replacement/blend on the CPU. */
        accepted = is_fake || lv_draw_aic_ge2d_accepts_image(task);
        break;
    case LV_DRAW_TASK_TYPE_LAYER:
        accepted = lv_draw_aic_ge2d_accepts_layer(task);
        break;
    default:
        return 0;
    }

    if (!accepted) {
        /* Not ours. The software renderer keeps the task; not an error. */
        g_ge2d_stats.fallback++;
        return 0;
    }

    if (task->preference_score > AIC_GE2D_PREFERENCE_SCORE) {
        task->preference_score = AIC_GE2D_PREFERENCE_SCORE;
        task->preferred_draw_unit_id = AIC_GE2D_DRAW_UNIT_ID;
    }

    switch (task->type) {
    case LV_DRAW_TASK_TYPE_FILL:
        g_ge2d_stats.fill_accepted++;
        break;
    case LV_DRAW_TASK_TYPE_IMAGE:
        g_ge2d_stats.image_accepted++;
        break;
    default:
        g_ge2d_stats.layer_accepted++;
        break;
    }
    return 1;
}

static int32_t lv_draw_aic_ge2d_dispatch(lv_draw_unit_t *unit, lv_layer_t *layer)
{
    lv_draw_aic_ge2d_unit_t *ge2d = (lv_draw_aic_ge2d_unit_t *)unit;
    lv_draw_aic_ge2d_outcome_t outcome = LV_DRAW_AIC_GE2D_OUTCOME_NOTHING;
    lv_draw_task_t *task;
    lv_result_t result;

    /* Synchronous unit: exactly one task in flight. */
    if (ge2d->task_act != NULL || lv_draw_aic_ge2d_faulted()) {
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

    switch (task->type) {
    case LV_DRAW_TASK_TYPE_FILL:
        /* The fill path has no fallback: it either runs on the engine or
         * reports a failure, so the outcome is always ENGINE here. */
        result = lv_draw_aic_ge2d_fill(task);
        outcome = LV_DRAW_AIC_GE2D_OUTCOME_ENGINE;
        break;
    case LV_DRAW_TASK_TYPE_IMAGE:
    case LV_DRAW_TASK_TYPE_LAYER:
        /* A LAYER task is the same blit with the child layer's buffer as the
         * source, so both types enter through the one entry point. It reports
         * back whether the engine really did the copy. */
        result = lv_draw_aic_ge2d_image(task, &outcome);
        break;
    default:
        /* evaluate() claims nothing else, so this is unreachable. */
        result = LV_RESULT_INVALID;
        break;
    }

    if (result == LV_RESULT_OK) {
        ge2d->task_act->state = LV_DRAW_TASK_STATE_FINISHED;
        switch (task->type) {
        case LV_DRAW_TASK_TYPE_FILL:
            g_ge2d_stats.fill_completed++;
            break;
        case LV_DRAW_TASK_TYPE_IMAGE:
            g_ge2d_stats.image_completed++;
            if (outcome == LV_DRAW_AIC_GE2D_OUTCOME_ENGINE &&
                (((const lv_draw_image_dsc_t *)task->draw_dsc)->scale_x != LV_SCALE_NONE ||
                 ((const lv_draw_image_dsc_t *)task->draw_dsc)->scale_y != LV_SCALE_NONE)) {
                g_ge2d_stats.scaled_image_engine++;
            }
            if (outcome == LV_DRAW_AIC_GE2D_OUTCOME_SOFTWARE) {
                g_ge2d_stats.image_sw_fallback++;
            }
            break;
        default:
            g_ge2d_stats.layer_completed++;
            if (outcome == LV_DRAW_AIC_GE2D_OUTCOME_SOFTWARE) {
                g_ge2d_stats.layer_sw_fallback++;
            }
            break;
        }
    }
    else {
        if (lv_draw_aic_ge2d_faulted()) {
            /* A failed GE submission or sync may leave DMA active. Keep this task and its
             * destination layer in flight, and retain the source lease.
             * Rendering is intentionally stopped until hardware reboot. */
            g_ge2d_stats.errors++;
            LV_LOG_ERROR("GE DMA fault: rendering stopped; reboot required");
            return 1;
        }
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
