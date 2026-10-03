/* SPDX-License-Identifier: Apache-2.0 */
#ifndef LV_AIC_CANVAS_IMAGE_H
#define LV_AIC_CANVAS_IMAGE_H
#include "lvgl.h"
#include <mpp_ge.h>
#ifndef AIC_LVGL_USE_CANVAS
#define AIC_LVGL_USE_CANVAS 0
#endif
#ifdef __cplusplus
extern "C" {
#endif
#if AIC_LVGL_USE_CANVAS
/* SDK-compatible public prefix. Do not change data/size or free them directly.
 * buf crop may be adjusted by callers; the owner retains allocation metadata. */
struct lv_mpp_buf {
    struct mpp_buf buf;
    unsigned char *data;
    int size;
};
/* UI-owner thread only. ARGB8888/RGB565, dimensions 1..4096; 64-byte aligned
 * base/stride. Pixels are uninitialized: fill them before publishing an image.
 * Default allocation budget 4 MiB, explicit variant bounds each allocation. */
struct lv_mpp_buf *lv_mpp_image_alloc(int width,int height,enum mpp_pixel_format fmt);
struct lv_mpp_buf *lv_aic_mpp_image_alloc_bounded(int width,int height,
    enum mpp_pixel_format fmt,uint32_t budget);
/* Clean/invalidate the owned allocation before DMA reads CPU-written pixels. */
void lv_mpp_image_flush_cache(struct lv_mpp_buf *image);
/* Remove all image/cache users and complete DMA before freeing. Shared GE
 * faults retain storage until reboot. Unknown/NULL handles are ignored. */
void lv_mpp_image_free(struct lv_mpp_buf *image);
#endif
#ifdef __cplusplus
}
#endif
#endif
