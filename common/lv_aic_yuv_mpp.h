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
/* Borrow a CPU-addressable physical frame, applying an optional aligned crop.
 * Caller supplies independently verified capacities from each plane base and
 * explicit colorimetry. No allocation, cache maintenance or retain is implicit.
 * Original visible spans must be valid even when a crop is requested. FD-backed
 * buffers and unaligned chroma origins are rejected. Failure preserves output.
 * A valid CPU crop may still lack padded rows required by to_mpp/GE. */
bool lv_aic_yuv_from_mpp(const struct mpp_buf *buffer, const size_t capacities[3],
                         lv_aic_yuv_color_space_t space, lv_aic_yuv_frame_t *output);
#endif
#endif
