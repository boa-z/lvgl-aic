/* SPDX-License-Identifier: Apache-2.0 */
#ifndef LV_AIC_PLAYER_EVENTS_H
#define LV_AIC_PLAYER_EVENTS_H
#include "lv_aic_player_session.h"
typedef struct lv_aic_player_events lv_aic_player_events_t;
typedef struct {
    uint64_t sequence, audio_sample_us;
    int64_t audio_pts;
    bool audio_valid, terminal, format_detected, format_failed, registration_failed;
} lv_aic_player_event_snapshot_t;
/* Worker create/attach/destroy; UI may read snapshots while storage is live.
 * Attach before start; callbacks only record data under their own OSAL mutex,
 * never call SDK/LVGL or take a playback control lock. Session must outlive
 * this mailbox. terminal is an SDK notification, NOT proof of successful EOS:
 * SDK also emits PLAY_END for decoder errors/resource failures. */
lv_aic_player_events_t *lv_aic_player_events_create(void);
bool lv_aic_player_events_attach(lv_aic_player_events_t *events,lv_aic_player_session_t *session);
bool lv_aic_player_events_snapshot(lv_aic_player_events_t *events,lv_aic_player_event_snapshot_t *snapshot);
/* Close/destroy SDK session first, ensuring callbacks have stopped. A failed
 * callback registration may still have installed a pointer: retain mailbox
 * until session close succeeds. Do not destroy concurrently with snapshot. */
bool lv_aic_player_events_destroy(lv_aic_player_events_t *events);
#endif
