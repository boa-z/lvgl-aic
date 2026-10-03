/* SPDX-License-Identifier: Apache-2.0 */
#ifndef LV_AIC_CAMERA_CAPTURE_H
#define LV_AIC_CAMERA_CAPTURE_H
#include "lv_aic_yuv_image.h"
#ifdef __cplusplus
extern "C" {
#endif
typedef struct lv_aic_camera_capture lv_aic_camera_capture_t;
typedef enum {
    LV_AIC_CAPTURE_OPENING, LV_AIC_CAPTURE_RUNNING, LV_AIC_CAPTURE_PAUSED,
    LV_AIC_CAPTURE_CLOSING, LV_AIC_CAPTURE_CLOSED, LV_AIC_CAPTURE_FAULT
} lv_aic_capture_state_t;
/* UI-owner API. Initialize the YUV decoder first. Device operations run on
 * an independent worker; exclusively reserve SDK VIN for this capture. */
lv_aic_camera_capture_t *lv_aic_camera_capture_open(const char *camera, uint32_t channel,
    lv_aic_yuv_format_t format, lv_aic_yuv_color_space_t space);
/* Returns a new immutable image, or NULL when no frame is ready. Caller owns
 * it and must detach widgets/finish queued draws before destroying it. */
lv_aic_yuv_image_t *lv_aic_camera_capture_poll(lv_aic_camera_capture_t *capture);
void lv_aic_camera_capture_pause(lv_aic_camera_capture_t *capture, bool paused);
lv_aic_capture_state_t lv_aic_camera_capture_state(lv_aic_camera_capture_t *capture);
/* Nonblocking shutdown request. Outstanding images keep their VIN buffers.
 * SDK dequeue can wait 60 seconds. Never delete/kill its worker to hurry close. */
void lv_aic_camera_capture_close(lv_aic_camera_capture_t *capture);
/* Free only once worker shutdown and every image reader have completed.
 * Returns false while resources remain live; retry from the UI owner. */
bool lv_aic_camera_capture_destroy(lv_aic_camera_capture_t *capture);
#ifdef __cplusplus
}
#endif
#endif
