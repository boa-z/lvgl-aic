/* SPDX-License-Identifier: Apache-2.0 */
#include "lv_draw_aic_ge2d_transform.h"
#include "lv_draw_aic_ge2d_scale.h"

static int64_t floor_div(int64_t a, int64_t b)
{
    return a / b - (a % b < 0);
}

static bool axis(uint32_t size, int32_t pivot, int32_t scale,
                 int32_t *extent, int32_t *center, int32_t *crop,
                 int32_t *phase, int32_t *step)
{
    if(size < 4 || size > 4092 || scale < 16 || scale > 4096) return false;
    int64_t delta = INT64_C(16777216) / scale;
    int64_t p = floor_div(((int64_t)pivot + 2) * 65536, delta);
    int64_t first = ((int64_t)pivot + 2) * 65536 - p * delta;
    int64_t n = (((int64_t)size + 3) * 65536 - first) / delta + 1;
    if(p + 2 < -8192 || p + 2 > 8191 || n < 4 || n > 4092 ||
       first > ((int64_t)size + 3) * 65536 || size + 4 - first / 65536 < 4) return false;
    *extent = (int32_t)n + 4; *center = (int32_t)p + 2;
    *crop = (int32_t)(first / 65536); *phase = (int32_t)(first % 65536);
    *step = (int32_t)delta;
    return true;
}

bool lv_aic_ge2d_transform_plan(uint32_t w, uint32_t h,
                                const lv_draw_image_dsc_t *dsc, uint32_t budget,
                                lv_aic_ge2d_transform_plan_t *plan)
{
    if(!dsc || !plan || !budget || dsc->rotation % 900 == 0 || dsc->skew_x || dsc->skew_y ||
       (dsc->scale_x == 256 && dsc->scale_y == 256)) return false;
    lv_aic_ge2d_transform_plan_t p = {0};
    if(!axis(w,dsc->pivot.x,dsc->scale_x,&p.scaled_w,&p.pivot.x,&p.crop_x,&p.phase_x,&p.step_x) ||
       !axis(h,dsc->pivot.y,dsc->scale_y,&p.scaled_h,&p.pivot.y,&p.crop_y,&p.phase_y,&p.step_y) ||
       lv_aic_ge2d_scale_split_risk(p.step_x,p.scaled_w-4)) return false;
    p.padded_w = w + 4; p.padded_h = h + 4;
    p.padded_stride = ((uint32_t)p.padded_w * 4 + 63) & ~63U;
    p.scaled_stride = ((uint32_t)p.scaled_w * 4 + 63) & ~63U;
    p.padded_bytes = p.padded_stride * p.padded_h;
    p.scaled_bytes = p.scaled_stride * p.scaled_h;
    if((uint64_t)p.padded_bytes + p.scaled_bytes > budget) return false;
    *plan = p;
    return true;
}
