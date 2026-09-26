/**
 * @file lv_draw_aic_ge2d.h
 * @brief ArtInChip GE2D draw unit for LVGL 9.6 (Phase 3A: FILL only).
 *
 * The GE2D unit is an independent draw unit. The legacy ArtInChip port aliased
 * it onto lv_draw_sw_unit_t, which coupled the hardware unit to the software
 * unit's thread, sync and saved layer/clip fields. Phase 3A runs synchronously
 * and owns nothing but the task it is currently executing.
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
 * @brief Phase 3A GE2D counters.
 *
 * @c fill_accepted counts FILL tasks this unit claimed in evaluate().
 * @c fill_completed counts the subset that reached FINISHED.
 * @c fallback counts FILL tasks this unit declined, which the software
 *   renderer then owns. It is not an error counter.
 * @c errors counts GE2D execution failures (fillrect/emit/sync).
 */
typedef struct {
    uint32_t fill_accepted;
    uint32_t fill_completed;
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

#endif /* AIC_LVGL_USE_GE2D */

#ifdef __cplusplus
}
#endif

#endif /* LV_DRAW_AIC_GE2D_H */
