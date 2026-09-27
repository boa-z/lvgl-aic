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

bool lv_aic_ge2d_rotation_scale_crop(uint32_t src_w, uint32_t src_h,
                                     const lv_area_t *dst, const lv_point_t *pivot,
                                     int32_t rotation, uint32_t sx, uint32_t sy,
                                     lv_area_t *src, unsigned *flags,
                                     int32_t *phase_x, int32_t *phase_y)
{
    int32_t angle = rotation % 3600;
    int64_t px, py, x[4], y[4], minx, maxx, miny, maxy;
    if (!dst || !pivot || !src || !flags || !phase_x || !phase_y ||
        !src_w || !src_h || sx < 16 || sx > 4096 || sy < 16 || sy > 4096)
        return false;
    if (angle < 0) angle += 3600;
    if (angle != 0 && angle != 900 && angle != 1800 && angle != 2700) return false;
    px = (int64_t)pivot->x * sx * 256 / 256;
    py = (int64_t)pivot->y * sy * 256 / 256;
    *flags = angle == 0 ? 0 : angle == 900 ? MPP_ROTATION_90 :
             angle == 1800 ? MPP_ROTATION_180 : MPP_ROTATION_270;
    for (int i = 0; i < 4; i++) {
        int64_t u = ((i & 1) ? dst->x2 : dst->x1) * 256;
        int64_t v = ((i & 2) ? dst->y2 : dst->y1) * 256;
        int64_t dx = u - px, dy = v - py;
        switch (angle) {
        case 0:    x[i] = (int64_t)pivot->x * 65536 + dx * 65536 / sx;
                   y[i] = (int64_t)pivot->y * 65536 + dy * 65536 / sy; break;
        case 900:  x[i] = (int64_t)pivot->x * 65536 + dy * 65536 / sy;
                   y[i] = (int64_t)pivot->y * 65536 - dx * 65536 / sx; break;
        case 1800: x[i] = (int64_t)pivot->x * 65536 - dx * 65536 / sx;
                   y[i] = (int64_t)pivot->y * 65536 - dy * 65536 / sy; break;
        default:   x[i] = (int64_t)pivot->x * 65536 - dy * 65536 / sy;
                   y[i] = (int64_t)pivot->y * 65536 + dx * 65536 / sx; break;
        }
    }
    minx = maxx = x[0]; miny = maxy = y[0];
    for (int i = 1; i < 4; i++) {
        if (x[i] < minx) minx = x[i];
        if (x[i] > maxx) maxx = x[i];
        if (y[i] < miny) miny = y[i];
        if (y[i] > maxy) maxy = y[i];
    }
    if (minx < 0 || miny < 0 || maxx >= (int64_t)src_w * 65536 ||
        maxy >= (int64_t)src_h * 65536) return false;
    src->x1 = (int32_t)(minx / 65536); src->y1 = (int32_t)(miny / 65536);
    src->x2 = (int32_t)((maxx + 65535) / 65536);
    src->y2 = (int32_t)((maxy + 65535) / 65536);
    if (src->x2 >= (int32_t)src_w) src->x2 = (int32_t)src_w - 1;
    if (src->y2 >= (int32_t)src_h) src->y2 = (int32_t)src_h - 1;
    *phase_x = (int32_t)(angle == 900 || angle == 2700 ? miny : minx) & 0xffff;
    *phase_y = (int32_t)(angle == 900 || angle == 2700 ? minx : miny) & 0xffff;
    return src->x2 - src->x1 >= 3 && src->y2 - src->y1 >= 3;
}
#else
bool lv_aic_ge2d_rotation_crop(uint32_t src_w, uint32_t src_h,
                               const lv_area_t *dst, const lv_point_t *pivot,
                               int32_t rotation, lv_area_t *src, unsigned *flags)
{ (void)src_w; (void)src_h; (void)dst; (void)pivot; (void)rotation; (void)src; (void)flags; return false; }
bool lv_aic_ge2d_rotation_scale_crop(uint32_t a, uint32_t b, const lv_area_t *c,
                                     const lv_point_t *d, int32_t e, uint32_t f,
                                     uint32_t g, lv_area_t *h, unsigned *i,
                                     int32_t *j, int32_t *k)
{ (void)a;(void)b;(void)c;(void)d;(void)e;(void)f;(void)g;(void)h;(void)i;(void)j;(void)k; return false; }
#endif
