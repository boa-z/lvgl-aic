/* SPDX-License-Identifier: Apache-2.0 */
#ifndef LV_AIC_PLANE_WINDOW_H
#define LV_AIC_PLANE_WINDOW_H
#include "lv_aic_video_plane.h"
/* Internal UI-owner binding, zero initialize. Object must be an image subclass.
 * present installs an alpha-zero .fake window. Caller owns the native source
 * until present returns; plane leases preserve it through scanout/faults.
 * Close must succeed before destroying this binding. No forced cleanup. */
typedef struct {
    lv_aic_video_plane_t *plane;
    const void *source;
    lv_area_t area;
    size_t budget;
    unsigned degrees;
} lv_aic_plane_window_t;
bool lv_aic_plane_window_present(lv_aic_plane_window_t *window,lv_obj_t *obj,const void *source);
bool lv_aic_plane_window_close(lv_aic_plane_window_t *window);
#endif
