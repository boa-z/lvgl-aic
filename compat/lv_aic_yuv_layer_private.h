/* SPDX-License-Identifier: Apache-2.0 */
#ifndef LV_AIC_YUV_LAYER_PRIVATE_H
#define LV_AIC_YUV_LAYER_PRIVATE_H

#include "lv_aic_yuv_layer.h"

/* Synchronous GE consumers hold a reader lease until submission and sync
 * complete.  The returned frame remains immutable for the lease lifetime. */
lv_aic_yuv_layer_t *lv_aic_yuv_layer_acquire(const lv_layer_t *layer,
                                             const lv_aic_yuv_frame_t **frame);
void lv_aic_yuv_layer_release_lease(lv_aic_yuv_layer_t *layer);

#endif /* LV_AIC_YUV_LAYER_PRIVATE_H */
