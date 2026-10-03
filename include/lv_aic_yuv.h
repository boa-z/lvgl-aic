/* SPDX-License-Identifier: Apache-2.0 */
#ifndef LV_AIC_YUV_H
#define LV_AIC_YUV_H
#include "lvgl.h"
#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>
#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    LV_AIC_YUV_BT601_LIMITED,
    LV_AIC_YUV_BT709_LIMITED,
    LV_AIC_YUV_BT601_FULL,
    LV_AIC_YUV_BT709_FULL
} lv_aic_yuv_color_space_t;

typedef struct {
    const uint8_t *data;
    uint32_t stride;
    size_t capacity;
} lv_aic_yuv_plane_t;

/* Borrowed CPU-addressable frame. No retain/release or cache maintenance is
 * implicit: producer owns every plane and must synchronize CPU/DMA access.
 * Keep planes stable until conversion/draw completion; never pass this
 * structure as lv_image_dsc_t.data or a legacy SDK mpp_buf.
 * Plane order: Y/U/V (planar), Y/UV or Y/VU (semi-planar), packed bytes (plane 0).
 */
typedef struct {
    lv_color_format_t format;
    uint32_t width, height;
    lv_aic_yuv_color_space_t color_space;
    lv_aic_yuv_plane_t planes[3];
} lv_aic_yuv_frame_t;

typedef struct {
    uint32_t row_bytes[3], rows[3];
    uint8_t planes;
} lv_aic_yuv_layout_t;

/* Accepts I420/I422/I444/I400/NV12/NV21/YUY2/UYVY, 1..4096 dimensions,
 * <=8M pixels. Odd dimensions use ceil-sized chroma/macro-pixel storage.
 * Layout is independent of plane pointers. Outputs stay unchanged on error. */
bool lv_aic_yuv_layout(lv_color_format_t format, uint32_t width, uint32_t height,
                       lv_aic_yuv_layout_t *layout);
bool lv_aic_yuv_validate(const lv_aic_yuv_frame_t *frame);

/* Convert to native LVGL RGB888 (B,G,R bytes), no allocation or cache entry.
 * Padding/bytes outside visible rows remain unchanged. Invalid input,
 * undersized output and any source/output overlap fail before writing.
 * Nearest chroma samples, explicit BT.601/709 range conversion with clipping.
 * This is a CPU reference/fallback, not proof of GE CSC pixel equivalence. */
bool lv_aic_yuv_to_rgb888(const lv_aic_yuv_frame_t *frame, uint8_t *output,
                         uint32_t stride, size_t capacity);
#ifdef __cplusplus
}
#endif
#endif
