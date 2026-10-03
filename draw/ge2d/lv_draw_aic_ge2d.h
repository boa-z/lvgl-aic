/**
 * @file lv_draw_aic_ge2d.h
 * @brief ArtInChip GE2D draw unit for LVGL 9.6 (FILL, IMAGE and LAYER).
 *
 * The GE2D unit is an independent draw unit. The legacy ArtInChip port aliased
 * it onto lv_draw_sw_unit_t, which coupled the hardware unit to the software
 * unit's thread, sync and saved layer/clip fields. This unit runs synchronously
 * and owns nothing but the task it is currently executing.
 *
 * One unit handles all three task types. There is deliberately no second draw
 * unit for IMAGE/LAYER: acceptance, dispatch and the GE2D device handle are
 * shared, and a second unit would duplicate the device lifecycle that Phase 3A
 * already had to get right.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef LV_DRAW_AIC_GE2D_H
#define LV_DRAW_AIC_GE2D_H

#include "lvgl_aic.h"
#include "lvgl_aic_compat.h"

#if AIC_LVGL_USE_GE2D
#include "lvgl_aic_private.h"
#endif

#ifdef __cplusplus
extern "C" {
#endif

struct mpp_ge;

/**
 * @brief GE2D counters, one accepted/completed pair per task type.
 *
 * @c fill_accepted / @c image_accepted / @c layer_accepted count the tasks of
 *   each type this unit claimed in evaluate(). evaluate() runs exactly once per
 *   task, at creation time, so these are exact per-refresh censuses.
 * @c fill_completed / @c image_completed / @c layer_completed count the subset
 *   that reached FINISHED. That is NOT "drawn by the engine": the executor hands
 *   a task it cannot blit to the software renderer and still reports success, so
 *   a task can finish without the engine having touched a pixel. Read the
 *   engine-drawn count as completed minus sw_fallback.
 * @c image_sw_fallback / @c layer_sw_fallback count the accepted tasks of that
 *   type the executor handed to the software renderer because the engine could
 *   not take the source - an unsupported format, or an address outside the GE
 *   window. This pair is the only way to tell "the unit claimed it" from "the
 *   engine drew it"; without it a completed count reads as acceleration that
 *   never happened. It is expected to be non-zero for LAYER on any board whose
 *   LVGL heap sits below the GE window: layer buffers come from lv_malloc, so
 *   the composite is declined and drawn in software. Nothing is dropped and
 *   nothing is misreported. There is no fill counterpart because the fill path
 *   cannot fall back - it either runs on the engine or fails.
 * @c fallback counts tasks of a supported type this unit declined in
 *   evaluate(), which the software renderer then owns. A task the executor
 *   rejects after the source became visible is counted in the sw_fallback pair
 *   instead, not here. It is not an error counter either way.
 * @c errors counts GE2D execution failures (fillrect/bitblt/emit/sync).
 */
typedef struct {
    uint32_t fill_accepted;
    uint32_t fill_completed;
    uint32_t image_accepted;
    uint32_t image_completed;
    uint32_t scaled_image_engine; /**< successful scaled IMAGE tasks only */
    uint32_t image_sw_fallback;
    uint32_t layer_accepted;
    uint32_t layer_completed;
    uint32_t layer_sw_fallback;
    uint32_t fallback;
    uint32_t errors;
    bool ready;
} lv_draw_aic_ge2d_stats_t;

#if AIC_LVGL_USE_GE2D

/** @brief The GE2D draw unit. */
typedef struct {
    lv_draw_unit_t base_unit;
    lv_draw_task_t *task_act;
} lv_draw_aic_ge2d_unit_t;

/**
 * @brief Open GE2D and register the draw unit.
 *
 * Call once per LVGL initialisation, after lv_init(). If mpp_ge_open() fails
 * the unit is still created but refuses ordinary tasks. SDK pseudo-images
 * use its CPU fill handler, preserving their replacement semantics.
 */
void lv_draw_aic_ge2d_init(void);

/** @brief Close the GE2D device handle. */
void lv_draw_aic_ge2d_deinit(void);

/** @brief The open GE2D device, or NULL when unavailable. */
struct mpp_ge *lv_draw_aic_ge2d_device(void);

/** @brief Current counters. Never NULL. */
const lv_draw_aic_ge2d_stats_t *lv_draw_aic_ge2d_stats(void);

/** @brief Zero the counters. @c ready is preserved. */
void lv_draw_aic_ge2d_stats_reset(void);

/**
 * @brief Execute a solid fill for @p task through GE2D.
 *
 * Runs fillrect -> emit -> sync synchronously. Returns LV_RESULT_INVALID if any
 * step fails. DMA failures latch until reboot; the dispatcher retains the
 * task and destination in flight. Preflight rejection does not latch a fault.
 * The task must already have been accepted by this unit's evaluate().
 */
lv_result_t lv_draw_aic_ge2d_fill(lv_draw_task_t *task);

/** True after an uncertain fill DMA failure; no runtime reset is safe. */
bool lv_draw_aic_ge2d_fill_faulted(void);

/**
 * Replace a solid FILL region with the exact ARGB value, without alpha blending.
 * Used by SDK pseudo-images with blend=0, including zero-alpha clears.
 * Descriptor opacity is ignored; clipping, format/address validation and
 * synchronous failure handling are identical to the normal fill executor.
 */
lv_result_t lv_draw_aic_ge2d_fill_replace(lv_draw_task_t *task, uint32_t argb);

/**
 * @brief How a dispatched image-shaped task's pixels were produced.
 *
 * The executors report this so the dispatcher can count what actually happened
 * instead of what was attempted. Without it a task the engine declined after
 * acceptance is indistinguishable from one it drew, and the counters overstate
 * acceleration.
 */
typedef enum {
    LV_DRAW_AIC_GE2D_OUTCOME_ENGINE,   /**< the GE engine performed the copy */
    LV_DRAW_AIC_GE2D_OUTCOME_SOFTWARE, /**< the engine declined; software drew it */
    LV_DRAW_AIC_GE2D_OUTCOME_NOTHING,  /**< there was nothing to draw */
} lv_draw_aic_ge2d_outcome_t;

/**
 * @brief Execute an RGB copy or scale for @p task through GE2D.
 *
 * Handles both LV_DRAW_TASK_TYPE_IMAGE and LV_DRAW_TASK_TYPE_LAYER. A LAYER task
 * carries an lv_layer_t in place of the image source; this function wraps that
 * layer's draw buffer in an image descriptor and runs the identical blit, so
 * there is only one code path for both types.
 *
 * Runs bitblt -> emit -> sync synchronously, with a Porter/Duff blend when the
 * source has an alpha channel or the descriptor has a partial opacity, and a
 * plain copy otherwise. The rule is the straight-alpha source-over pair, which
 * the GE names GE_PD_NONE - not GE_PD_SRC_OVER, which is the premultiplied
 * form. See the note in lv_draw_aic_ge2d_image.c for why that distinction
 * matters and how it is verified. @p outcome, when not NULL, reports which of
 * the three cases happened. Returns LV_RESULT_OK for every outcome that leaves
 * the screen correct, including two that draw nothing through the engine:
 *   - the source turns out to be unusable after the decode (an unsupported
 *     format, or an address outside the GE window). The task is then handed to
 *     the software renderer, and @p outcome is OUTCOME_SOFTWARE.
 *   - the task is a LAYER whose layer has no buffer, because nothing was drawn
 *     on it. There is nothing to blend, and @p outcome is OUTCOME_NOTHING.
 * LV_RESULT_INVALID means a malformed task or a bitblt/emit/sync failure.
 * Hardware failures are never retried as software blends.
 *
 * The task must already have been accepted by this unit's evaluate().
 */
bool lv_draw_aic_ge2d_image_faulted(void);
lv_result_t lv_draw_aic_ge2d_image(lv_draw_task_t *task,
                                   lv_draw_aic_ge2d_outcome_t *outcome);

#endif /* AIC_LVGL_USE_GE2D */

#ifdef __cplusplus
}
#endif

#endif /* LV_DRAW_AIC_GE2D_H */
