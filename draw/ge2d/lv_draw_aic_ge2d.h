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
 *   that reached FINISHED, which is not the same as "drawn by the engine": the
 *   executor hands a task it cannot blit to the software renderer and still
 *   reports success, so the task is finished but was not accelerated. A layer
 *   task also finishes without drawing anything when its layer has no buffer -
 *   there is nothing to blend. Neither case is a failure.
 * @c fallback counts tasks of a supported type this unit declined in
 *   evaluate(), which the software renderer then owns. A task the executor
 *   rejects after the source became visible is not counted here. It is not an
 *   error counter either way.
 * @c errors counts GE2D execution failures (fillrect/bitblt/emit/sync).
 */
typedef struct {
    uint32_t fill_accepted;
    uint32_t fill_completed;
    uint32_t image_accepted;
    uint32_t image_completed;
    uint32_t layer_accepted;
    uint32_t layer_completed;
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
 * the unit is still created but refuses every task, so the software renderer
 * keeps drawing instead of the unit dispatching into a NULL device.
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
 * @brief Execute an opaque solid fill for @p task through GE2D.
 *
 * Runs fillrect -> emit -> sync synchronously. Returns LV_RESULT_INVALID if any
 * step fails, so the dispatcher can mark the task FAILED rather than FINISHED.
 * The task must already have been accepted by this unit's evaluate().
 */
lv_result_t lv_draw_aic_ge2d_fill(lv_draw_task_t *task);

/**
 * @brief Execute a plain image blit for @p task through GE2D.
 *
 * Handles both LV_DRAW_TASK_TYPE_IMAGE and LV_DRAW_TASK_TYPE_LAYER. A LAYER task
 * carries an lv_layer_t in place of the image source; this function wraps that
 * layer's draw buffer in an image descriptor and runs the identical blit, so
 * there is only one code path for both types.
 *
 * Runs bitblt -> emit -> sync synchronously, with a GE_PD_SRC_OVER blend when
 * the source has an alpha channel or the descriptor has a partial opacity, and
 * a plain copy otherwise. Returns LV_RESULT_OK for every outcome that leaves the
 * screen correct, including two that draw nothing through the engine:
 *   - the source turns out to be unusable after the decode (an unsupported
 *     format, or an address outside the GE window). The task is then handed to
 *     the software renderer.
 *   - the task is a LAYER whose layer has no buffer, because nothing was drawn
 *     on it. There is nothing to blend.
 * LV_RESULT_INVALID means the task itself is malformed (wrong type, NULL
 * descriptor) - a programming error, not a runtime condition.
 *
 * The task must already have been accepted by this unit's evaluate().
 */
lv_result_t lv_draw_aic_ge2d_image(lv_draw_task_t *task);

#endif /* AIC_LVGL_USE_GE2D */

#ifdef __cplusplus
}
#endif

#endif /* LV_DRAW_AIC_GE2D_H */
