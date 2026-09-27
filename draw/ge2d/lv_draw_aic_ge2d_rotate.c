/* SPDX-License-Identifier: Apache-2.0 */
#include "lv_draw_aic_ge2d_rotate.h"

#if AIC_LVGL_USE_GE2D
#include <mpp_types.h>

bool lv_aic_ge2d_rotation_crop(uint32_t src_w, uint32_t src_h,
                               const lv_area_t *dst, const lv_point_t *pivot,
                               int32_t rotation, lv_area_t *src, unsigned *flags)
{
    int32_t angle = rotation;
    int64_t xs[4], ys[4];
    if (!dst || !pivot || !src || !flags || src_w == 0 || src_h == 0) return false;
    angle %= 3600;
    if (angle < 0) angle += 3600;
    *flags = 0;
    for (int i = 0; i < 4; i++) {
        int32_t x = (i & 1) ? dst->x2 : dst->x1;
        int32_t y = (i & 2) ? dst->y2 : dst->y1;
        switch (angle) {
        case 0:
            xs[i] = x; ys[i] = y; break;
        case 900:
            xs[i] = (int64_t)y - pivot->y + pivot->x;
            ys[i] = (int64_t)pivot->x - x + pivot->y;
            *flags = MPP_ROTATION_90; break;
        case 1800:
            xs[i] = (int64_t)2 * pivot->x - x;
            ys[i] = (int64_t)2 * pivot->y - y;
            *flags = MPP_ROTATION_180; break;
        case 2700:
            xs[i] = (int64_t)pivot->y - y + pivot->x;
            ys[i] = (int64_t)x - pivot->x + pivot->y;
            *flags = MPP_ROTATION_270; break;
        default:
            return false;
        }
    }
    int64_t x1 = xs[0], x2 = xs[0], y1 = ys[0], y2 = ys[0];
    for (int i = 1; i < 4; i++) {
        if (xs[i] < x1) x1 = xs[i];
        if (xs[i] > x2) x2 = xs[i];
        if (ys[i] < y1) y1 = ys[i];
        if (ys[i] > y2) y2 = ys[i];
    }
    if (x1 < 0 || y1 < 0 || x2 >= (int64_t)src_w || y2 >= (int64_t)src_h ||
        x2 - x1 + 1 <= 0 || y2 - y1 + 1 <= 0) return false;
    *src = (lv_area_t){(int32_t)x1, (int32_t)y1, (int32_t)x2, (int32_t)y2};
    return true;
}
#else
bool lv_aic_ge2d_rotation_crop(uint32_t src_w, uint32_t src_h,
                               const lv_area_t *dst, const lv_point_t *pivot,
                               int32_t rotation, lv_area_t *src, unsigned *flags)
{ (void)src_w; (void)src_h; (void)dst; (void)pivot; (void)rotation; (void)src; (void)flags; return false; }
#endif
