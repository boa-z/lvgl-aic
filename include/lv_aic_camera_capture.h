/* SPDX-License-Identifier: Apache-2.0 */
#ifndef LV_AIC_CAMERA_CAPTURE_H
#define LV_AIC_CAMERA_CAPTURE_H
#include "lv_aic_yuv_image.h"
#ifdef __cplusplus
extern "C" {
#endif
typedef struct lv_aic_camera_capture lv_aic_camera_capture_t;
typedef enum {
    LV_AIC_CAPTURE_OPENING, LV_AIC_CAPTURE_READY, LV_AIC_CAPTURE_RUNNING, LV_AIC_CAPTURE_PAUSED,
    LV_AIC_CAPTURE_CLOSING, LV_AIC_CAPTURE_CLOSED, LV_AIC_CAPTURE_FAULT
} lv_aic_capture_state_t;
typedef enum {
    LV_AIC_INPUT_NONE, LV_AIC_INPUT_PENDING, LV_AIC_INPUT_APPLIED,
    LV_AIC_INPUT_FAILED, LV_AIC_INPUT_CANCELLED
} lv_aic_camera_input_state_t;
typedef struct {
    lv_aic_camera_input_state_t state;
    uint32_t sequence, requested, applied;
} lv_aic_camera_input_status_t;
/* Raw sensor selector (SDK range 0..3), never a VIN queue index. Only one
 * request may be pending. true means accepted; inspect get_input for driver
 * completion. applied is UINT32_MAX until acknowledged, or after failure.
 * APPLIED is not a guarantee that the next dequeued frame is from that input.
 * Close cancels a queued request; an already executing driver call completes. */
bool lv_aic_camera_capture_select_input(lv_aic_camera_capture_t *capture,uint32_t input);
lv_aic_camera_input_status_t lv_aic_camera_capture_get_input(lv_aic_camera_capture_t *capture);
/* UI-owner API. Initialize the YUV decoder first. Device operations run on
 * an independent worker; exclusively reserve SDK VIN for this capture. */
lv_aic_camera_capture_t *lv_aic_camera_capture_open(const char *camera, uint32_t channel,
    lv_aic_yuv_format_t format, lv_aic_yuv_color_space_t space);
/* Prepare configures the device/pool asynchronously without queueing or starting
 * capture. READY confirms completion. channel is a VIN queue index, not the
 * SDK camera sensor input selector (VIN1/VIN2). */
lv_aic_camera_capture_t *lv_aic_camera_capture_prepare(const char *camera, uint32_t channel,
    lv_aic_yuv_format_t format, lv_aic_yuv_color_space_t space);
/* Accept an idempotent start request while OPENING/READY/RUNNING/PAUSED.
 * Device success/failure is reported asynchronously by state(). */
bool lv_aic_camera_capture_start(lv_aic_camera_capture_t *capture);
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
