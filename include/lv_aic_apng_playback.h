/* SPDX-License-Identifier: Apache-2.0 */
#ifndef LV_AIC_APNG_PLAYBACK_H
#define LV_AIC_APNG_PLAYBACK_H
#include "lv_aic_rgb_image.h"
#include "lv_aic_apng.h"
#ifdef __cplusplus
extern "C" {
#endif
typedef struct lv_aic_apng_playback lv_aic_apng_playback_t;
typedef enum {
    LV_AIC_APNG_OPENING,LV_AIC_APNG_READY,LV_AIC_APNG_PLAYING,LV_AIC_APNG_PLAYBACK_PAUSED,
    LV_AIC_APNG_TERMINAL,LV_AIC_APNG_CLOSING,LV_AIC_APNG_CLOSED,LV_AIC_APNG_FAULT
} lv_aic_apng_playback_state_t;
typedef struct {
    lv_aic_apng_limits_t limits;
    size_t stream_budget,snapshot_budget,cma_budget,packet_limit;
    unsigned snapshots; /* 2..8 */
    uint32_t minimum_delay_us;
} lv_aic_apng_playback_options_t;
typedef struct {
    lv_aic_apng_playback_state_t state;
    bool finished,restart_pending;
    uint32_t width,height,rate_num,rate_den,frame_index;
    uint64_t composed,published,completed_plays,restarts;
} lv_aic_apng_playback_status_t;
/* LVGL-owner API; a single APNG instance. All file/parse/MPP/compose work runs
 * on an OSAL worker. Native filesystem path <=127 bytes, no LVGL drive mapping.
 * Budgets are separate: stream allocations, snapshot pool, CMA decoder frames,
 * aligned packet buffer. During load an additional file_bytes buffer exists;
 * decoder/allocator metadata, thread stack and LVGL caches are not included.
 * Initialize the RGB image decoder before poll. No auto-start or audio. */
lv_aic_apng_playback_t *lv_aic_apng_playback_prepare(const char *path,const lv_aic_apng_playback_options_t *options);
/* Start does not rewind TERMINAL; use restart for replay. */
bool lv_aic_apng_playback_start(lv_aic_apng_playback_t *p);
bool lv_aic_apng_playback_pause(lv_aic_apng_playback_t *p,bool paused);
bool lv_aic_apng_playback_rate(lv_aic_apng_playback_t *p,uint32_t numerator,uint32_t denominator);
/* Replay from frame zero after READY, preserves start/pause/rate. One pending
 * replay; queued old publication is discarded, existing images stay valid. */
bool lv_aic_apng_playback_restart(lv_aic_apng_playback_t *p);
bool lv_aic_apng_playback_poll(lv_aic_apng_playback_t *p,lv_aic_rgb_image_t **image,uint64_t *sequence);
lv_aic_apng_playback_status_t lv_aic_apng_playback_status(lv_aic_apng_playback_t *p);
/* Close is nonblocking. Worker retries failed SDK frame returns; no forced
 * thread termination. Destroy after finished AND all returned image readers
 * release. Retire images only after widgets and queued draws detach. */
void lv_aic_apng_playback_close(lv_aic_apng_playback_t *p);
bool lv_aic_apng_playback_destroy(lv_aic_apng_playback_t *p);
#ifdef __cplusplus
}
#endif
#endif
