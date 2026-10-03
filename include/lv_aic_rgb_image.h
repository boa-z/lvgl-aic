/* SPDX-License-Identifier: Apache-2.0 */
#ifndef LV_AIC_RGB_IMAGE_H
#define LV_AIC_RGB_IMAGE_H
#include "lv_aic_yuv_image.h"
#ifdef __cplusplus
extern "C" {
#endif
typedef struct {
    lv_color_format_t format;
    uint32_t width, height, stride;
    const uint8_t *data;
    size_t capacity;
} lv_aic_rgb_frame_t;
typedef struct lv_aic_rgb_image lv_aic_rgb_image_t;
/* Native RGB565/RGB888/XRGB8888/straight ARGB8888 only. Borrowed immutable
 * CPU-coherent storage; dimensions 1..4096, <=8M pixels, explicit capacity. */
bool lv_aic_rgb_frame_validate(const lv_aic_rgb_frame_t *frame);
bool lv_aic_rgb_image_decoder_init(void);
bool lv_aic_rgb_image_decoder_deinit(void);
/* LVGL owner-thread API. Retain once; release after owner and all decoder/GE
 * readers finish. Callbacks must not enter LVGL or block on SDK operations.
 * Decoder borrows complete padded rows; minimal-span/stride-normalized or
 * premultiplied requests use immutable per-image snapshots. No generic cache. */
lv_aic_rgb_image_t *lv_aic_rgb_image_create(const lv_aic_rgb_frame_t *frame,
    lv_aic_yuv_retain_cb_t retain,lv_aic_yuv_release_cb_t release,void *context);
const lv_image_dsc_t *lv_aic_rgb_image_source(const lv_aic_rgb_image_t *image);
/* Detach widgets and finish queued draw tasks first. Existing readers retain
 * all storage; retired images reject new decoder opens. */
void lv_aic_rgb_image_destroy(lv_aic_rgb_image_t *image);
#ifdef __cplusplus
}
#endif
#endif
