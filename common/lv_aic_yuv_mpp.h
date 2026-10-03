/* SPDX-License-Identifier: Apache-2.0 */
#ifndef LV_AIC_YUV_MPP_H
#define LV_AIC_YUV_MPP_H
#include "lv_aic_yuv.h"
#include "lvgl_aic_compat.h"
#if AIC_LVGL_BSP_MPP
#include <mpp_types.h>
/* Build a full-frame physical MPP source descriptor without dereferencing the
 * planes or performing DMA/cache operations. Address floor is SoC policy.
 * GE has 16-bit strides and shares U/V pitch; reject truncation and layouts
 * its HAL would silently round. DMA planes require complete padded rows.
 * Output remains unchanged on failure. Crop/scale restrictions belong to the
 * eventual draw submission, not this metadata adapter. */
bool lv_aic_yuv_to_mpp(const lv_aic_yuv_frame_t *frame, uint32_t address_floor,
                       struct mpp_buf *output);
#endif
#endif
