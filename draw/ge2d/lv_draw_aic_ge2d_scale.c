/* SPDX-License-Identifier: Apache-2.0 */
#include "lv_draw_aic_ge2d_scale.h"

bool lv_aic_ge2d_scale_axis(int32_t source_size, int32_t dest_start,
                           int32_t dest_size, int32_t pivot, uint32_t scale,
                           lv_aic_ge2d_scale_axis_t *axis)
{
    int64_t step, first, last, end;
    if (!axis || source_size < 4 || dest_size < 4 || scale < 16 || scale > 4096) {
        return false;
    }
    step = (INT64_C(65536) * 256) / scale;
    first = ((int64_t)dest_start - pivot) * step + (int64_t)pivot * 65536;
    last = first + ((int64_t)dest_size - 1) * step;
    /* A rounded LVGL bound can include a fractional point outside the image.
     * Decline the whole task rather than trim it and silently omit pixels. */
    if (first < 0 || last > ((int64_t)source_size - 1) * 65536) {
        return false;
    }
    axis->crop = (int32_t)(first / 65536);
    /* D13x RGB filter needs ceil(last) plus the adjacent tap. Never advertise
     * padded pixels outside the decoded buffer: clamp at its actual edge. */
    end = (last + 65535) / 65536 + 2;
    if (end > source_size) end = source_size;
    axis->extent = (int32_t)(end - axis->crop);
    axis->step_16 = (int32_t)step;
    axis->phase_16 = (int32_t)(first % 65536);
    return axis->extent >= 4;
}

bool lv_aic_ge2d_scale_split_risk(int32_t step_16, int32_t dest_width)
{
    /* Vendor calculate_split_params outer condition (D13x/D12x/G73x/D12p).
     * 0.5x/1.5x/2x do not enter it. Defer split submission and safely decline
     * the entire risk interval, instead of hoping the last block is correct. */
    return step_16 < 65536 && step_16 > (65536 / 32) * 29 && dest_width >= 32;
}
