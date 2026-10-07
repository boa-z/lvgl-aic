/* SPDX-License-Identifier: Apache-2.0 */
#ifndef LV_AIC_YUV_LAYER_H
#define LV_AIC_YUV_LAYER_H

#include "lv_aic_yuv_image.h"

#ifdef __cplusplus
extern "C" {
#endif

/*
 * A normal LVGL layer buffer is a packed byte surface.  The SDK's GE port
 * has one additional private convention for YUV layers: draw_buf->data points
 * at an MPP plane descriptor.  This adapter publishes that convention as an
 * explicit application-owned binding instead of guessing from a color enum.
 * Only planar I420/I422/I444/I400 frames are valid layer sources.
 */
typedef struct lv_aic_yuv_layer lv_aic_yuv_layer_t;

bool lv_aic_yuv_layer_attach(lv_layer_t *layer, const lv_aic_yuv_frame_t *frame,
                             lv_aic_yuv_retain_cb_t retain_cb,
                             lv_aic_yuv_release_cb_t release_cb,
                             void *context);

/* Owner-thread only.  A detached binding is released after any active GE
 * reader finishes; the layer pointer must not be reused until this returns. */
bool lv_aic_yuv_layer_detach(lv_layer_t *layer);

#ifdef __cplusplus
}
#endif

#endif /* LV_AIC_YUV_LAYER_H */
