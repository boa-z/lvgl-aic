/* SPDX-License-Identifier: Apache-2.0 */
#ifndef LV_AIC_YUV_IMAGE_PRIVATE_H
#define LV_AIC_YUV_IMAGE_PRIVATE_H
#include "lv_aic_yuv_image.h"
/* Owner-thread leases for synchronous native consumers. A failed DMA must
 * retain its lease until hardware quiescence is established, never free it
 * merely because submission/sync returned an error. */
lv_aic_yuv_image_t *lv_aic_yuv_image_acquire(const void *source, const lv_aic_yuv_frame_t **frame);
void lv_aic_yuv_image_release_lease(lv_aic_yuv_image_t *image);
#endif
