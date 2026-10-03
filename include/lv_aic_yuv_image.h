/* SPDX-License-Identifier: Apache-2.0 */
#ifndef LV_AIC_YUV_IMAGE_H
#define LV_AIC_YUV_IMAGE_H
#include "lv_aic_yuv.h"
#ifdef __cplusplus
extern "C" {
#endif

typedef struct lv_aic_yuv_image lv_aic_yuv_image_t;
typedef bool (*lv_aic_yuv_retain_cb_t)(void *context);
typedef void (*lv_aic_yuv_release_cb_t)(void *context);

/* Optional decoder, independent of MPP/GE. Call after lv_init on the LVGL
 * owner thread. Deinit refuses while any image or decoder reader remains.
 * Destroy all images/close readers and deinit this decoder before lv_deinit. */
bool lv_aic_yuv_image_decoder_init(void);
bool lv_aic_yuv_image_decoder_deinit(void);

/* Immutable frame publication. Both callbacks are mandatory. Create copies
 * metadata and retains the producer once; release runs once after owner AND
 * decoder readers release the image. Failure never retains producer storage.
 * Callbacks must not re-enter LVGL or this API (close runs under LVGL's lock).
 * Producer must keep pixels immutable and CPU-coherent until release_cb.
 * Publish a NEW image for each frame, rather than mutating an existing one. */
lv_aic_yuv_image_t *lv_aic_yuv_image_create(const lv_aic_yuv_frame_t *frame,
                                           lv_aic_yuv_retain_cb_t retain_cb,
                                           lv_aic_yuv_release_cb_t release_cb,
                                           void *context);
const lv_image_dsc_t *lv_aic_yuv_image_source(const lv_aic_yuv_image_t *image);

/* Detach from every widget and finish queued draws before destroying the
 * owner reference. Already-open decoder readers keep their immutable pixels
 * and producer reference alive, but no new reader may open a retired image.
 * The image pointer must not be used after destroy. Owner-thread only. */
void lv_aic_yuv_image_destroy(lv_aic_yuv_image_t *image);

#ifdef __cplusplus
}
#endif
#endif
