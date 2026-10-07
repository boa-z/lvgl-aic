/* SPDX-License-Identifier: Apache-2.0 */
#ifndef LV_AIC_RGB_IMAGE_PRIVATE_H
#define LV_AIC_RGB_IMAGE_PRIVATE_H
#include "lv_aic_rgb_image.h"
/* Owner-thread native-consumer lease. On uncertain DMA completion retain
 * this lease until reboot, including all lazy decoded snapshots it owns. */
lv_aic_rgb_image_t *lv_aic_rgb_image_acquire(const void *source,const lv_aic_rgb_frame_t **frame);
void lv_aic_rgb_image_release_lease(lv_aic_rgb_image_t *image);
#endif
