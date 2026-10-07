/* SPDX-License-Identifier: Apache-2.0 */
#ifndef LV_AIC_PLAYER_IMAGE_H
#define LV_AIC_PLAYER_IMAGE_H
#include "lv_aic_yuv_image.h"
#include "lv_aic_rgb_image.h"
#ifdef __cplusplus
extern "C" {
#endif
/* Unique UI-owned handle: do not copy ownership. Zero initialize before poll.
 * Detach widgets and finish queued tasks before destroying. Native/decoder
 * readers can outlive the owner, retaining producer frames until completion. */
typedef struct {
    lv_aic_yuv_image_t *yuv;
    lv_aic_rgb_image_t *rgb;
    int64_t pts;
} lv_aic_player_image_t;
const lv_image_dsc_t *lv_aic_player_image_source(const lv_aic_player_image_t *image);
void lv_aic_player_image_destroy(lv_aic_player_image_t *image);
#ifdef __cplusplus
}
#endif
#endif
