/* SPDX-License-Identifier: Apache-2.0 */
#ifndef LV_AIC_VIN_SESSION_H
#define LV_AIC_VIN_SESSION_H
#include <stdbool.h>
#include <stdint.h>
#include <mpp_vin.h>
#ifndef _IO
#include <sys/ioctl.h>
#endif

/* Internal camera transport, serialized by its owner (not thread-safe).
 * Zero initialize once. Never copy an open session. Borrowed buffer metadata
 * stays valid until successful close. Each acquired index must be released
 * only after all CPU/GE/display readers are finished. No implicit queueing. */
typedef struct {
    struct vin_dev_ctx device;
    struct vin_video_buf buffers;
    uint32_t channel, held;
    bool opened, pool, streaming, paused, faulted;
} lv_aic_vin_session_t;

bool lv_aic_vin_open(lv_aic_vin_session_t *s, const char *camera,
                     uint32_t channel, enum mpp_pixel_format format, uint32_t count);
bool lv_aic_vin_start(lv_aic_vin_session_t *s);
bool lv_aic_vin_pause(lv_aic_vin_session_t *s);
bool lv_aic_vin_resume(lv_aic_vin_session_t *s);
bool lv_aic_vin_acquire(lv_aic_vin_session_t *s, uint32_t *index);
bool lv_aic_vin_release(lv_aic_vin_session_t *s, uint32_t index);
bool lv_aic_vin_stop(lv_aic_vin_session_t *s);
/* Returns false without freeing any storage if frames are held or STREAM_OFF
 * fails. Caller must retain the session and device resources for retry. */
bool lv_aic_vin_close(lv_aic_vin_session_t *s);
#endif
