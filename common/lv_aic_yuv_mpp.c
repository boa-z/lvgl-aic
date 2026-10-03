/* SPDX-License-Identifier: Apache-2.0 */
#include "lv_aic_yuv_mpp.h"
#include "lv_aic_pixel_format.h"
#if AIC_LVGL_BSP_MPP
bool lv_aic_yuv_to_mpp(const lv_aic_yuv_frame_t *frame, uint32_t address_floor,
                       struct mpp_buf *output)
{
    lv_aic_yuv_layout_t layout;
    struct mpp_buf result = {0};
    static const uint32_t spaces[] = {
        MPP_COLOR_SPACE_BT601, MPP_COLOR_SPACE_BT709,
        MPP_COLOR_SPACE_BT601_FULL_RANGE, MPP_COLOR_SPACE_BT709_FULL_RANGE
    };
    if (!output || !lv_aic_yuv_validate(frame) ||
        !lv_aic_pixel_format_to_mpp(frame->format, &result.format)) return false;
    bool sub_x = frame->format != LV_COLOR_FORMAT_I400 && frame->format != LV_COLOR_FORMAT_I444;
    bool sub_y = frame->format == LV_COLOR_FORMAT_I420 ||
                 frame->format == LV_COLOR_FORMAT_NV12 || frame->format == LV_COLOR_FORMAT_NV21;
    if ((sub_x && (frame->width & 1)) || (sub_y && (frame->height & 1))) return false;
    lv_aic_yuv_layout(frame->format, frame->width, frame->height, &layout);
    if (layout.planes == 3 && frame->planes[1].stride != frame->planes[2].stride) return false;
    for (unsigned i=0; i<layout.planes; i++) {
        uintptr_t address = (uintptr_t)frame->planes[i].data;
        uint64_t span = (uint64_t)frame->planes[i].stride * layout.rows[i];
        if (address < address_floor || address > UINT32_MAX ||
            frame->planes[i].stride > UINT16_MAX || span > frame->planes[i].capacity ||
            span > (uint64_t)UINT32_MAX - address + 1) return false;
        result.phy_addr[i] = (uint32_t)address;
        result.stride[i] = frame->planes[i].stride;
    }
    result.buf_type = MPP_PHY_ADDR;
    result.size.width = frame->width; result.size.height = frame->height;
    result.flags = spaces[frame->color_space];
    *output = result;
    return true;
}
#endif
