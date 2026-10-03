/* SPDX-License-Identifier: Apache-2.0 */
#ifndef LV_AIC_PLAYER_CLOCK_H
#define LV_AIC_PLAYER_CLOCK_H
#include <stdbool.h>
#include <stdint.h>
/* Serialized worker-owned microsecond timeline. No SDK or wall-clock reads.
 * Explicit reset is required on seek/source change; no guessed discontinuity
 * correction. Audio sync uses a timestamped callback sample, never a torn
 * 64-bit SDK getter. Negative PTS (audio-cache preroll) is supported. */
typedef struct {
    int64_t reference_pts;
    uint64_t reference_us, last_us;
    bool valid, paused;
} lv_aic_player_clock_t;
typedef enum {
    LV_AIC_PLAYER_CLOCK_WAIT, LV_AIC_PLAYER_CLOCK_PRESENT,
    LV_AIC_PLAYER_CLOCK_DROP, LV_AIC_PLAYER_CLOCK_PAUSED
} lv_aic_player_clock_action_t;
void lv_aic_player_clock_reset(lv_aic_player_clock_t *clock,int64_t pts,uint64_t now);
/* Sync rejects samples while paused or with a timestamp older than the last
 * observation. Caller must serialize/choose the master clock explicitly. */
bool lv_aic_player_clock_sync(lv_aic_player_clock_t *clock,int64_t pts,uint64_t now);
bool lv_aic_player_clock_pause(lv_aic_player_clock_t *clock,bool paused,uint64_t now);
bool lv_aic_player_clock_time(lv_aic_player_clock_t *clock,uint64_t now,int64_t *pts);
/* Late tolerance is caller policy. Future frames wait, never publish early.
 * Exact tolerance boundary presents; later frames drop. delay is uint64 us.
 * Invalid/backwards/overflowing time leaves state and outputs unchanged. */
bool lv_aic_player_clock_schedule(lv_aic_player_clock_t *clock,int64_t frame_pts,
    uint64_t now,uint64_t late_tolerance,lv_aic_player_clock_action_t *action,uint64_t *delay);
#endif
