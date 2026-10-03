/* SPDX-License-Identifier: Apache-2.0 */
#ifndef LV_AIC_RGB_MPP_H
#define LV_AIC_RGB_MPP_H
#include "lv_aic_rgb_image.h"
#include "lvgl_aic_compat.h"
#if AIC_LVGL_BSP_MPP
#include <mpp_types.h>
/* Explicit allocator capacity from plane base; CPU-addressable physical
 * storage only. Apply optional crop without copying. Transactional failure. */
bool lv_aic_rgb_from_mpp(const struct mpp_buf *buffer,size_t capacity,lv_aic_rgb_frame_t *output);
#endif
#endif
