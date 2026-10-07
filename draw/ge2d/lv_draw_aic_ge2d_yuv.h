/* SPDX-License-Identifier: Apache-2.0 */
#ifndef LV_DRAW_AIC_GE2D_YUV_H
#define LV_DRAW_AIC_GE2D_YUV_H
#include "lv_draw_aic_ge2d.h"
/* -1 hardware failure, 0 unsupported/not a published YUV source,
 * 1 synchronous GE completion (native CPU tail for straight ARGB targets),
 * 2 no visible work. No CPU replay after -1; planes/scratch remain retained. */
int lv_draw_aic_ge2d_yuv(lv_draw_task_t *task);
bool lv_draw_aic_ge2d_yuv_faulted(void);
#endif
