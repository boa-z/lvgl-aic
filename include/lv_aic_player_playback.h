/* SPDX-License-Identifier: Apache-2.0 */
#ifndef LV_AIC_PLAYER_PLAYBACK_H
#define LV_AIC_PLAYER_PLAYBACK_H
#include "lv_aic_player_image.h"
#ifdef __cplusplus
extern "C" {
#endif
typedef struct lv_aic_player_playback lv_aic_player_playback_t;
typedef enum {
    LV_AIC_PLAYBACK_OPENING, LV_AIC_PLAYBACK_READY, LV_AIC_PLAYBACK_PLAYING,
    LV_AIC_PLAYBACK_PAUSED, LV_AIC_PLAYBACK_TERMINAL, LV_AIC_PLAYBACK_CLOSING,
    LV_AIC_PLAYBACK_CLOSED, LV_AIC_PLAYBACK_FAULT
} lv_aic_playback_state_t;
typedef struct {
    size_t cma_budget;
    unsigned extra_frames; /* 2..8, limits application-held frames too. */
    lv_aic_yuv_color_space_t color_space;
} lv_aic_playback_options_t;
typedef struct {
    lv_aic_playback_state_t state;
    bool finished, has_video, has_audio, seekable, video_eos, sdk_terminal, position_valid;
    uint32_t width, height, frames_received, frames_queued;
    int64_t duration_us, position_us;
    int volume; /* -1 until a requested volume has been applied. */
} lv_aic_playback_status_t;
/* UI-owner API. Reserve one playback instance; both RGB/YUV decoders must be
 * initialized by caller before poll. Prepare runs blocking SDK work on an
 * independent OSAL worker. Native SDK URI, max 127 bytes; no LVGL drive mapping.
 * SDK get_frame already performs decode and A/V timing; do not layer another
 * sleep/drop scheduler on top. Counters describe mailbox work, not display. */
lv_aic_player_playback_t *lv_aic_player_playback_prepare(const char *uri,const lv_aic_playback_options_t *options);
bool lv_aic_player_playback_start(lv_aic_player_playback_t *playback);
bool lv_aic_player_playback_pause(lv_aic_player_playback_t *playback,bool paused);
bool lv_aic_player_playback_volume(lv_aic_player_playback_t *playback,int volume);
bool lv_aic_player_playback_poll(lv_aic_player_playback_t *playback,lv_aic_player_image_t *image);
lv_aic_playback_status_t lv_aic_player_playback_status(lv_aic_player_playback_t *playback);
/* Nonblocking close. Pending SDK calls must return normally, then readers and
 * frame-return retries must finish. Never kill the worker to force teardown.
 * TERMINAL preserves final image and reports SDK end notification, not clean
 * EOS certification: SDK also uses PLAY_END for decoder/resource failures. */
void lv_aic_player_playback_close(lv_aic_player_playback_t *playback);
/* Retry after finished; all outstanding images/readers must have released.
 * Stop/restart/source replacement currently means close/destroy/new prepare. */
bool lv_aic_player_playback_destroy(lv_aic_player_playback_t *playback);
#ifdef __cplusplus
}
#endif
#endif
