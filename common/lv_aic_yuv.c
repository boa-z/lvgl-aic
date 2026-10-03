/* SPDX-License-Identifier: Apache-2.0 */
#include "lv_aic_yuv.h"

bool lv_aic_yuv_layout(lv_color_format_t format, uint32_t w, uint32_t h,
                       lv_aic_yuv_layout_t *layout)
{
    lv_aic_yuv_layout_t result = {{0}, {0}, 0};
    if (!layout || !w || !h || w > 4096 || h > 4096 ||
        (uint64_t)w * h > 8U * 1024U * 1024U) return false;
    result.row_bytes[0] = w; result.rows[0] = h; result.planes = 1;
    switch (format) {
    case LV_COLOR_FORMAT_I420:
    case LV_COLOR_FORMAT_I422:
    case LV_COLOR_FORMAT_I444:
        result.planes = 3;
        result.row_bytes[1] = result.row_bytes[2] = format == LV_COLOR_FORMAT_I444 ? w : (w+1)/2;
        result.rows[1] = result.rows[2] = format == LV_COLOR_FORMAT_I420 ? (h+1)/2 : h;
        break;
    case LV_COLOR_FORMAT_NV12:
    case LV_COLOR_FORMAT_NV21:
        result.planes = 2;
        result.row_bytes[1] = ((w+1)/2)*2; result.rows[1] = (h+1)/2;
        break;
    case LV_COLOR_FORMAT_YUY2:
    case LV_COLOR_FORMAT_UYVY:
        result.row_bytes[0] = ((w+1)/2)*4;
        break;
    case LV_COLOR_FORMAT_I400: break;
    default: return false;
    }
    *layout = result;
    return true;
}

static bool span_valid(const void *data, uint32_t stride, uint32_t row_bytes,
                       uint32_t rows, size_t capacity, size_t *span)
{
    uint64_t bytes = (uint64_t)(rows - 1) * stride + row_bytes;
    if (!data || stride < row_bytes || bytes > SIZE_MAX || bytes > capacity ||
        (uintptr_t)data > UINTPTR_MAX - (size_t)bytes) return false;
    *span = (size_t)bytes;
    return true;
}

bool lv_aic_yuv_validate(const lv_aic_yuv_frame_t *frame)
{
    lv_aic_yuv_layout_t layout;
    size_t span;
    if (!frame || frame->color_space < LV_AIC_YUV_BT601_LIMITED ||
        frame->color_space > LV_AIC_YUV_BT709_FULL ||
        !lv_aic_yuv_layout(frame->format, frame->width, frame->height, &layout)) return false;
    for (unsigned i = 0; i < layout.planes; i++) {
        if (!span_valid(frame->planes[i].data, frame->planes[i].stride,
                        layout.row_bytes[i], layout.rows[i], frame->planes[i].capacity, &span)) return false;
    }
    return true;
}

static void sample(const lv_aic_yuv_frame_t *f, uint32_t x, uint32_t y, int *luma, int *u, int *v)
{
    const uint8_t *p = f->planes[0].data + (size_t)y * f->planes[0].stride;
    *luma = p[x]; *u = *v = 128;
    if (f->format == LV_COLOR_FORMAT_I400) return;
    if (f->format == LV_COLOR_FORMAT_YUY2 || f->format == LV_COLOR_FORMAT_UYVY) {
        p += (x/2)*4;
        if (f->format == LV_COLOR_FORMAT_YUY2) { *luma = p[(x%2)*2]; *u = p[1]; *v = p[3]; }
        else { *luma = p[(x%2)*2+1]; *u = p[0]; *v = p[2]; }
        return;
    }
    if (f->format != LV_COLOR_FORMAT_I444) x /= 2;
    if (f->format == LV_COLOR_FORMAT_I420 || f->format == LV_COLOR_FORMAT_NV12 ||
        f->format == LV_COLOR_FORMAT_NV21) y /= 2;
    p = f->planes[1].data + (size_t)y * f->planes[1].stride;
    if (f->format == LV_COLOR_FORMAT_NV12) { *u = p[x*2]; *v = p[x*2+1]; }
    else if (f->format == LV_COLOR_FORMAT_NV21) { *v = p[x*2]; *u = p[x*2+1]; }
    else { *u = p[x]; *v = f->planes[2].data[(size_t)y * f->planes[2].stride + x]; }
}

static uint8_t channel(int32_t fixed)
{
    if (fixed <= 0) return 0;
    if (fixed >= 255 * 65536) return 255;
    return (uint8_t)((fixed + 32768) / 65536);
}

bool lv_aic_yuv_to_rgb888(const lv_aic_yuv_frame_t *frame, uint8_t *output,
                         uint32_t stride, size_t capacity)
{
    lv_aic_yuv_layout_t layout;
    size_t output_span, input_span;
    /* Q16 coefficients from BT.601/709 Kr/Kb equations, not the SDK GE's
     * quantized register table. Keep CPU oracle independent of hardware. */
    static const int32_t coefficients[4][5] = {
        {76309,104597,25675,53279,132201},
        {76309,117489,13975,34925,138438},
        {65536,91881,22553,46802,116130},
        {65536,103206,12276,30679,121609}
    };
    if (!lv_aic_yuv_validate(frame) ||
        !span_valid(output, stride, frame->width * 3, frame->height, capacity, &output_span)) return false;
    lv_aic_yuv_layout(frame->format, frame->width, frame->height, &layout);
    for (unsigned i = 0; i < layout.planes; i++) {
        span_valid(frame->planes[i].data, frame->planes[i].stride,
                   layout.row_bytes[i], layout.rows[i], frame->planes[i].capacity, &input_span);
        uintptr_t src = (uintptr_t)frame->planes[i].data, dst = (uintptr_t)output;
        if (src < dst + output_span && dst < src + input_span) return false;
    }
    const int32_t *c = coefficients[frame->color_space];
    int offset = frame->color_space < LV_AIC_YUV_BT601_FULL ? 16 : 0;
    for (uint32_t y = 0; y < frame->height; y++) {
        uint8_t *dst = output + (size_t)y * stride;
        for (uint32_t x = 0; x < frame->width; x++, dst += 3) {
            int luma, u, v;
            sample(frame, x, y, &luma, &u, &v);
            int32_t base = (luma - offset) * c[0];
            u -= 128; v -= 128;
            dst[0] = channel(base + u*c[4]);
            dst[1] = channel(base - u*c[2] - v*c[3]);
            dst[2] = channel(base + v*c[1]);
        }
    }
    return true;
}
