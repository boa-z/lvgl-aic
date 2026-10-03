/* SPDX-License-Identifier: Apache-2.0 */
#define AIC_LVGL_USE_PRIVATE_API 1
#include "lv_draw_aic_ge2d_display.h"
#include "lv_draw_aic_ge2d.h"
#include "lv_draw_aic_ge2d_utils.h"
#include "lv_aic_pixel_format.h"

#if AIC_LVGL_USE_GE2D && AIC_LVGL_BSP_MPP
#include <mpp_ge.h>

static bool buffer_valid(const lv_draw_buf_t *buf)
{
    if (!buf || !buf->data || !lv_draw_aic_ge2d_buf_address_valid(buf)) return false;
    uint32_t w = buf->header.w, h = buf->header.h, stride = buf->header.stride;
    uint32_t bpp = lv_color_format_get_size((lv_color_format_t)buf->header.cf);
    if (!w || !h || w > 4096 || h > 4096 || !bpp || stride < w * bpp) return false;
    uint64_t size = (uint64_t)stride * h;
    uintptr_t start = (uintptr_t)buf->data;
    return size <= buf->data_size && start <= UINT32_MAX &&
           size <= (uint64_t)UINT32_MAX + 1 - start;
}

int lv_draw_aic_ge2d_display_rotate(const lv_draw_buf_t *src, lv_draw_buf_t *dst,
                                   lv_display_rotation_t rotation)
{
    if (lv_draw_aic_ge2d_faulted()) return -1;
    struct mpp_ge *device = lv_draw_aic_ge2d_device();
    enum mpp_pixel_format format;
    if (!device || !buffer_valid(src) || !buffer_valid(dst) ||
        src->header.cf != dst->header.cf ||
        !lv_aic_pixel_format_is_ge2d_dst((lv_color_format_t)src->header.cf) ||
        !lv_aic_pixel_format_to_mpp((lv_color_format_t)src->header.cf, &format)) return 0;
    if (rotation < LV_DISPLAY_ROTATION_90 || rotation > LV_DISPLAY_ROTATION_270) return 0;
    bool swap = rotation != LV_DISPLAY_ROTATION_180;
    if (dst->header.w != (swap ? src->header.h : src->header.w) ||
        dst->header.h != (swap ? src->header.w : src->header.h)) return 0;
    uintptr_t s = (uintptr_t)src->data, d = (uintptr_t)dst->data;
    uint64_t send = (uint64_t)s + src->header.stride * src->header.h;
    uint64_t dend = (uint64_t)d + dst->header.stride * dst->header.h;
    if ((uint64_t)s < dend && (uint64_t)d < send) return 0;
    struct ge_bitblt blt = {0};
    /* Display rotation is counterclockwise; MPP flags are clockwise. */
    blt.ctrl.flags = rotation == LV_DISPLAY_ROTATION_90 ? MPP_ROTATION_270 :
                     rotation == LV_DISPLAY_ROTATION_180 ? MPP_ROTATION_180 : MPP_ROTATION_90;
    blt.src_buf.buf_type = blt.dst_buf.buf_type = MPP_PHY_ADDR;
    blt.src_buf.phy_addr[0] = (uint32_t)s;
    blt.dst_buf.phy_addr[0] = (uint32_t)d;
    blt.src_buf.stride[0] = src->header.stride;
    blt.dst_buf.stride[0] = dst->header.stride;
    blt.src_buf.size.width = src->header.w;
    blt.src_buf.size.height = src->header.h;
    blt.dst_buf.size.width = dst->header.w;
    blt.dst_buf.size.height = dst->header.h;
    blt.src_buf.format = blt.dst_buf.format = format;
    lv_area_t sa = {0, 0, src->header.w - 1, src->header.h - 1};
    lv_area_t da = {0, 0, dst->header.w - 1, dst->header.h - 1};
    lv_draw_aic_ge2d_prepare_src_cache(src, &sa);
    lv_draw_aic_ge2d_prepare_dst_cache(dst, &da);
    if (mpp_ge_bitblt(device, &blt) < 0 || mpp_ge_emit(device) < 0 || mpp_ge_sync(device) < 0) {
        lv_draw_aic_ge2d_quarantine();
        return -1;
    }
    return 1;
}
#endif
