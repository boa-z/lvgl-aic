/* SPDX-License-Identifier: Apache-2.0 */
#ifndef LV_DRAW_AIC_GE2D_STRIPES_H
#define LV_DRAW_AIC_GE2D_STRIPES_H
#include "lvgl_aic.h"
#if AIC_LVGL_USE_GE2D && AIC_LVGL_BSP_MPP
#include <mpp_ge.h>

/* Preflight every command without modifying input or submitting DMA. The RGB
 * near-unity scaler interval uses balanced strips of at most 31 output pixels
 * along the source-X/scaler-X axis, including orthogonal rotations. */
bool lv_aic_ge2d_stripe_count(const struct ge_bitblt *blt, unsigned *count);
bool lv_aic_ge2d_stripe(const struct ge_bitblt *blt, unsigned index, struct ge_bitblt *command);
/* 1 complete, 0 unsupported before submission, -1 uncertain DMA completion. */
int lv_aic_ge2d_stripe_run(struct mpp_ge *ge, const struct ge_bitblt *blt);
#endif
#endif
