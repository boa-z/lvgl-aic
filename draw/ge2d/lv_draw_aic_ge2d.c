/**
 * @file lv_draw_aic_ge2d.c
 * @brief ArtInChip GE2D draw unit: registration, evaluation and dispatch.
 *
 * FILL supports solid and bounded horizontal/vertical two-stop gradients.
 * IMAGE: rotated or bounded transformed tiled, RGB copies/scales with bounded CPU recolor -
 * the blit blends, so a partial opacity is supported rather than declined.
 * LAYER: the same blit, fed from a child layer's buffer instead of a decoded
 * image, including bounded scaling and arbitrary-angle scaled rotation.
 * Everything else is declined, and a declined task
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
#include "lv_aic_pixel_format.h"
#include "lv_draw_aic_ge2d_yuv.h"
#include "lv_aic_yuv_layer_private.h"
#include "lv_aic_yuv_mpp.h"

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
    if (layer == NULL) return false;
    if (layer->draw_buf == NULL) {
        /* LVGL allocates child buffers lazily, after task evaluation. The
         * actual DMA address is checked again after allocation in dispatch. */
        int64_t w=(int64_t)layer->buf_area.x2-layer->buf_area.x1+1;
        int64_t h=(int64_t)layer->buf_area.y2-layer->buf_area.y1+1;
        return w>0 && h>0 && w<=4096 && h<=4096 &&
               lv_draw_aic_ge2d_dst_format_supported(layer->color_format);
    }
    draw_buf = layer->draw_buf;
    if (draw_buf->header.flags & LV_IMAGE_FLAGS_PREMULTIPLIED) return false;

    if (!lv_draw_aic_ge2d_dst_format_supported((lv_color_format_t)draw_buf->header.cf)) {
        return false;
    }

    return lv_draw_aic_ge2d_buf_layout_valid(draw_buf);
}

/**
 * True when @p dsc carries a supported IMAGE or LAYER transform.
 *
 * Arbitrary scaled angles use bounded scratch passes; orthogonal transforms
 * use BITBLT.
 */
static bool lv_draw_aic_ge2d_dsc_is_supported_transform(const lv_draw_image_dsc_t *dsc)
{
    return dsc->scale_x >= LV_SCALE_NONE / 16 &&
           dsc->scale_x <= LV_SCALE_NONE * 16 &&
           dsc->scale_y >= LV_SCALE_NONE / 16 &&
           dsc->scale_y <= LV_SCALE_NONE * 16 &&
           dsc->skew_x == 0 && dsc->skew_y == 0;
}

/**
 * True when @p dsc uses effects handled by GE or its bounded preparation.
 *
 * Shared by IMAGE and LAYER: rounded clips and non-normal blends remain
 * software. Recolor and bounded bitmap masks use native CPU preparation.
 * Exact single-value keys may use BITBLT directly; range, packed RGB565 and
 * filtered/rotated keys are normalized by the image executor before GE.
 * When recolor is also requested, the executor stages the key first and then
 * recolors the keyed ARGB copy, so the combination remains on the GE path.
 * Keeping the list here is what stops the two task types from drifting apart.
 */
static bool lv_draw_aic_ge2d_dsc_is_plain(const lv_draw_image_dsc_t *dsc)
{
    if (dsc->bitmap_mask_src != NULL || dsc->clip_radius != 0) {
        return false;
    }

    return dsc->blend_mode == LV_BLEND_MODE_NORMAL;
}

/* The SDK evaluator filters the image descriptor's advertised source format
 * before assigning the task to GE2D.  RAW is an encoded-resource marker: the
 * decoder replaces it with a readable RGB/YUV buffer before submission.  Keep
 * that admission separate from lv_aic_pixel_format_is_ge2d_src(), which is
 * deliberately the post-decode RGB mapping used by the GE blitter. */
static bool lv_draw_aic_ge2d_image_source_format_admitted(lv_color_format_t cf)
{
    /* Direct unit callers can leave header.cf unset; the decoder remains the
     * authority in that case, matching LVGL's task construction order. */
    if (cf == LV_COLOR_FORMAT_UNKNOWN) return true;

    switch (cf) {
    case LV_COLOR_FORMAT_RGB565:
    case LV_COLOR_FORMAT_RGB888:
    case LV_COLOR_FORMAT_ARGB8888:
    case LV_COLOR_FORMAT_ARGB8888_PREMULTIPLIED:
    case LV_COLOR_FORMAT_XRGB8888:
    case LV_COLOR_FORMAT_I420:
    case LV_COLOR_FORMAT_I422:
    case LV_COLOR_FORMAT_I444:
    case LV_COLOR_FORMAT_I400:
    case LV_COLOR_FORMAT_RAW:
    case LV_COLOR_FORMAT_RAW_ALPHA:
        return true;
    default:
        return false;
    }
}

/* LAYER sources may be explicitly premultiplied.  The MPP descriptor uses
 * ARGB8888 plus MPP_BUF_IS_PREMULTIPLY, so this is a source-only extension of
 * the four direct mappings accepted by lv_aic_pixel_format_is_ge2d_src(). */
static bool lv_draw_aic_ge2d_layer_src_format_supported(const lv_layer_t *layer)
{
    if (!layer) return false;
    if (lv_aic_pixel_format_is_ge2d_src(layer->color_format) ||
        layer->color_format == LV_COLOR_FORMAT_ARGB8888_PREMULTIPLIED)
        return true;

    /* LVGL's generic layer allocator cannot describe planar storage.  A YUV
     * layer is therefore admitted only when the application attached an
     * immutable, physically addressable frame through the explicit adapter. */
    if (layer->color_format != LV_COLOR_FORMAT_I420 &&
        layer->color_format != LV_COLOR_FORMAT_I422 &&
        layer->color_format != LV_COLOR_FORMAT_I444 &&
        layer->color_format != LV_COLOR_FORMAT_I400)
        return false;
    const lv_aic_yuv_frame_t *frame;
    lv_aic_yuv_layer_t *lease = lv_aic_yuv_layer_acquire(layer, &frame);
    if (!lease) return false;
    struct mpp_buf source;
    uint32_t floor = 0;
#if defined(AIC_CHIP_D13X) || defined(AIC_CHIP_G73X)
    floor = 0x40000000U;
#endif
    bool valid = lv_aic_yuv_to_mpp(frame, floor, &source);
    lv_aic_yuv_layer_release_lease(lease);
    return valid;
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

    if (!lv_draw_aic_ge2d_fill_dsc_supported(dsc)) return false;

    if (dsc->opa <= LV_OPA_MIN || !lv_draw_aic_ge2d_accepts_dst(task)) {
        return false;
    }

    /* Partial straight ARGB fills use bounded GE staging and native blend. */
    return true;
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

    if (!lv_draw_aic_ge2d_image_source_format_admitted(dsc->header.cf)) {
        return false;
    }

    if (!lv_draw_aic_ge2d_dsc_is_supported_transform(dsc)) {
        return false;
    }

    lv_draw_image_dsc_t effects = *dsc;
    effects.bitmap_mask_src = NULL; /* The executor prepares a bounded image copy. */
    if (!lv_draw_aic_ge2d_dsc_is_plain(&effects)) {
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

    /* Match the SDK evaluator's source-side gate.  A child layer is often
     * still lazy (draw_buf == NULL) when evaluate() runs, so defer address
     * and stride checks until dispatch, but its declared format is already
     * authoritative.  Claiming an unsupported/YUV child here would make the
     * GE unit own the task and only discover the software fallback after the
     * decoder opens, skewing acceptance counters and scheduling preference. */
    if (!lv_draw_aic_ge2d_layer_src_format_supported(layer_to_draw)) {
        return false;
    }
    if (layer_to_draw->draw_buf != NULL &&
        layer_to_draw->draw_buf->header.cf != layer_to_draw->color_format) {
        /* LVGL normally keeps these fields equal.  Treat a stale or malformed
         * child descriptor as software-owned instead of reading pixels under
         * a format different from the layer contract. */
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

    lv_draw_image_dsc_t effects = *dsc;
    effects.bitmap_mask_src = NULL; /* The executor prepares a bounded layer copy. */
    if (!lv_draw_aic_ge2d_dsc_is_plain(&effects)) {
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
    g_ge2d_stats.fill_sw_fallback = 0U;
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

/* Native blending expects the task clip to stay inside the allocated layer.
 * Keep scheduler fallback safe for the same clipped task geometry as GE. */
static void lv_draw_aic_ge2d_sw_fill(lv_draw_task_t *task)
{
    lv_draw_task_t copy = *task;
    if (lv_area_intersect(&copy.clip_area, &copy.clip_area, &copy.target_layer->buf_area))
        lv_draw_sw_fill(&copy, copy.draw_dsc, &copy.area);
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
        /* Match LVGL SW scheduling: memory pressure is retryable; never
         * discard a task just because another layer currently owns memory. */
        return LV_DRAW_UNIT_IDLE;
    }

    task->state = LV_DRAW_TASK_STATE_IN_PROGRESS;
    task->draw_unit = unit;
    ge2d->task_act = task;

    if (task->type==LV_DRAW_TASK_TYPE_FILL && !lv_draw_aic_ge2d_accepts_dst(task)) {
        /* Lazy allocation may use heap fallback. IMAGE/LAYER retain their
         * executor's source leases and special .fake replacement semantics. */
        lv_draw_aic_ge2d_sw_fill(task);
        result=LV_RESULT_OK;outcome=LV_DRAW_AIC_GE2D_OUTCOME_SOFTWARE;
    }
    else switch (task->type) {
    case LV_DRAW_TASK_TYPE_FILL:
        result = lv_draw_aic_ge2d_fill(task);
        outcome = LV_DRAW_AIC_GE2D_OUTCOME_ENGINE;
        if (result != LV_RESULT_OK && !lv_draw_aic_ge2d_faulted()) {
            /* Scratch budget/allocation failure occurs before any DMA. A
             * latched DMA failure must never replay onto the original target. */
            lv_draw_aic_ge2d_sw_fill(task);
            result = LV_RESULT_OK;
            outcome = LV_DRAW_AIC_GE2D_OUTCOME_SOFTWARE;
        }
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
            if(outcome==LV_DRAW_AIC_GE2D_OUTCOME_SOFTWARE) g_ge2d_stats.fill_sw_fallback++;
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
