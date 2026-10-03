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
    if (!output || !lv_aic_yuv_validate(frame)) return false;
    if (frame->format==LV_AIC_YUV_NV16) result.format=MPP_FMT_NV16;
    else if (frame->format==LV_AIC_YUV_NV61) result.format=MPP_FMT_NV61;
    else if (!lv_aic_pixel_format_to_mpp(frame->format, &result.format)) return false;
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
/* SDK physical addresses are directly CPU-addressable only on the caller's
 * platform. Capacities must come from allocator records, never stride guesses. */
bool lv_aic_yuv_from_mpp(const struct mpp_buf *buffer, const size_t capacities[3],
                         lv_aic_yuv_color_space_t space, lv_aic_yuv_frame_t *output)
{
    lv_aic_yuv_frame_t result = {0};
    lv_aic_yuv_layout_t layout;
    lv_color_format_t format;
    if (!buffer || !capacities || !output || buffer->buf_type != MPP_PHY_ADDR ||
        buffer->size.width <= 0 || buffer->size.height <= 0) return false;
    if (buffer->format == MPP_FMT_NV16) result.format = LV_AIC_YUV_NV16;
    else if (buffer->format == MPP_FMT_NV61) result.format = LV_AIC_YUV_NV61;
    else {
        if (!lv_aic_pixel_format_from_mpp(buffer->format, &format)) return false;
        result.format = format;
    }
    result.width = buffer->size.width; result.height = buffer->size.height;
    result.color_space = space;
    if (!lv_aic_yuv_layout(result.format, result.width, result.height, &layout)) return false;
    for (unsigned i = 0; i < layout.planes; i++) {
        uint64_t span = (uint64_t)(layout.rows[i] - 1) * buffer->stride[i] + layout.row_bytes[i];
        if (span > (uint64_t)UINT32_MAX - buffer->phy_addr[i] + 1) return false;
        result.planes[i] = (lv_aic_yuv_plane_t){
            (const uint8_t *)(uintptr_t)buffer->phy_addr[i], buffer->stride[i], capacities[i]};
    }
    if (!lv_aic_yuv_validate(&result)) return false;
    if (buffer->crop_en) {
        const struct mpp_rect *c = &buffer->crop;
        bool sub_x = result.format != LV_COLOR_FORMAT_I400 && result.format != LV_COLOR_FORMAT_I444;
        bool sub_y = result.format == LV_COLOR_FORMAT_I420 || result.format == LV_COLOR_FORMAT_NV12 ||
                     result.format == LV_COLOR_FORMAT_NV21;
        bool packed = result.format == LV_COLOR_FORMAT_YUY2 || result.format == LV_COLOR_FORMAT_UYVY;
        if (c->x < 0 || c->y < 0 || c->width <= 0 || c->height <= 0 ||
            (uint64_t)c->x + c->width > result.width ||
            (uint64_t)c->y + c->height > result.height ||
            (sub_x && (c->x & 1)) || (sub_y && (c->y & 1))) return false;
        for (unsigned i = 0; i < layout.planes; i++) {
            uint32_t x = c->x, y = c->y;
            if (i && sub_y) y /= 2;
            if (i && sub_x && layout.planes == 3) x /= 2;
            if (packed) x *= 2;
            size_t offset = (size_t)y * result.planes[i].stride + x;
            if (offset >= result.planes[i].capacity) return false;
            result.planes[i].data = (const uint8_t *)((uintptr_t)result.planes[i].data + offset);
            result.planes[i].capacity -= offset;
        }
        result.width = c->width; result.height = c->height;
        if (!lv_aic_yuv_validate(&result)) return false;
    }
    *output = result;
    return true;
}
#endif
