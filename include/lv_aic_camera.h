/* SPDX-License-Identifier: Apache-2.0 */
#ifndef LV_AIC_CAMERA_H
#define LV_AIC_CAMERA_H
#include "lv_aic_camera_capture.h"
#ifdef __cplusplus
extern "C" {
#endif
extern const lv_obj_class_t lv_aic_camera_class;
typedef enum {
    LV_AIC_CAMERA_FORMAT_NV16, LV_AIC_CAMERA_FORMAT_NV12,
    LV_AIC_CAMERA_FORMAT_YUV400, _LV_AIC_CAMERA_FORMAT_LAST
} lv_aic_camera_format;
typedef enum {
    LV_AIC_CAMERA_CLOSED, LV_AIC_CAMERA_OPENING, LV_AIC_CAMERA_READY,
    LV_AIC_CAMERA_RUNNING, LV_AIC_CAMERA_PAUSED, LV_AIC_CAMERA_STOPPING,
    LV_AIC_CAMERA_STOPPED, LV_AIC_CAMERA_FAULT
} lv_aic_camera_state_t;
/* SDK aliases, not board wiring or VIN queue indices. */
typedef enum { LV_AIC_CAMERA_CH_VIN1=0, LV_AIC_CAMERA_CH_VIN2=2 } lv_aic_camera_ch_t;
/* 0 means asynchronous request accepted; -1 means invalid/busy/not opened.
 * VALUE_CHANGED also reports input status changes. get_channel returns the
 * last driver-acknowledged selector, or UINT32_MAX if unknown. It does not
 * identify the source of a displayed frame. Restart reapplies the requested
 * selector; configure resets that choice when changing device configuration. */
int lv_aic_camera_set_channel(lv_obj_t *obj,uint32_t input);
uint32_t lv_aic_camera_get_channel(lv_obj_t *obj);
lv_aic_camera_input_status_t lv_aic_camera_get_channel_status(lv_obj_t *obj);
/* LVGL owner only, outside draw callbacks. Initialize the YUV decoder before
 * opening. An LV_RESULT_OK means request accepted, not device completion.
 * LV_EVENT_VALUE_CHANGED reports state transitions; query get_state in the
 * callback. Deleting the widget from that callback is supported. */
lv_obj_t *lv_aic_camera_create(lv_obj_t *parent);
/* Optional exclusive native video-plane output. Configure while closed; needs
 * ARGB8888 default display and initialized MPP/.fake decoder support.
 * Shares player rectangle/rotation/clipping rules.
 * rotation_budget bounds current+next CMA copies; zero permits unrotated only.
 * No implicit selection. Geometry/plane failure reports FAULT and closes
 * capture; stop/close/deletion drains scanout before releasing VIN readers. */
lv_result_t lv_aic_camera_set_video_plane(lv_obj_t *obj,bool enabled,size_t rotation_budget);
lv_result_t lv_aic_camera_set_format(lv_obj_t *obj, lv_aic_camera_format format);
/* Explicit sensor colorimetry is mandatory before open. Configure only while
 * closed/stopped, after old transport cleanup. queue is NOT sensor input. */
lv_result_t lv_aic_camera_configure(lv_obj_t *obj, const char *device,
    uint32_t queue, lv_aic_yuv_color_space_t color_space);
/* SDK-shaped barcode APIs. Configure only while closed/stopped after cleanup.
 * Requires AIC_LVGL_USE_BARCODE. only suppresses preview, but does not enable
 * decoding by itself. disable restores preview on the next open.
 * Callback runs on the LVGL owner, after transport state notification, only
 * for nonempty successful results. Input is binary and valid only during the
 * callback; output storage is caller-owned and must remain live until closed.
 * The callback may close/stop/delete this widget. Never retain in_data.
 * Unlike the SDK implementation, callbacks never execute on a worker thread. */
typedef void (*lv_aic_camera_barcode_cb_t)(const char *in_data,int in_length,
    char *out_data,int out_length);
lv_result_t lv_aic_camera_barcode_enable(lv_obj_t *obj);
lv_result_t lv_aic_camera_barcode_disable(lv_obj_t *obj);
lv_result_t lv_aic_camera_barcode_only(lv_obj_t *obj);
lv_result_t lv_aic_camera_barcode_callback(lv_obj_t *obj,
    lv_aic_camera_barcode_cb_t callback,char *data,int data_size);
lv_result_t lv_aic_camera_open(lv_obj_t *obj);
lv_result_t lv_aic_camera_start(lv_obj_t *obj);
lv_result_t lv_aic_camera_stop(lv_obj_t *obj);
lv_result_t lv_aic_camera_pause(lv_obj_t *obj);
lv_result_t lv_aic_camera_resume(lv_obj_t *obj);
lv_result_t lv_aic_camera_close(lv_obj_t *obj);
lv_aic_camera_state_t lv_aic_camera_get_state(lv_obj_t *obj);
/* Deleted widgets keep a timer/context until capture and draw readers finish.
 * Keep running the LVGL timer handler until this returns zero before lv_deinit.
 * Live widgets must also be closed/deleted first. A GE quarantine may retain
 * resources until reboot. Never kill the capture thread to force shutdown. */
unsigned lv_aic_camera_pending_cleanup(void);
#ifdef __cplusplus
}
#endif
#endif
