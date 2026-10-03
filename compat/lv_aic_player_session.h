/* SPDX-License-Identifier: Apache-2.0 */
#ifndef LV_AIC_PLAYER_SESSION_H
#define LV_AIC_PLAYER_SESSION_H
#include <stdbool.h>
#include <stdint.h>
#include <aic_player.h>
#define LV_AIC_PLAYER_LEASES 8
/* Internal serialized worker API. Zero initialize once; never copy an open
 * session. Prepare/start/get-frame/stop can block: do not call from LVGL UI.
 * SDK external video render must be enabled to prevent competing frame users.
 * Frames are borrowed until release. Stop/seek/close refuse outstanding leases.
 * Duplicate decoder ownership quarantines the entire session until reboot. */
typedef struct {
    struct aic_player *player;
    struct av_media_info info;
    struct mpp_frame frames[LV_AIC_PLAYER_LEASES];
    char uri[128]; /* SDK MM_MAX_STRINGNAME_SIZE, including terminating NUL. */
    uint32_t held;
    uint64_t next_ticket, tickets[LV_AIC_PLAYER_LEASES];
    bool prepared, started, paused, faulted, quarantined;
} lv_aic_player_session_t;
/* False may leave resources live if cleanup failed: retry close, never free
 * session storage until close returns true. URI is a native SDK path. */
bool lv_aic_player_session_open(lv_aic_player_session_t *s,const char *uri);
/* Install before start. SDK borrows allocator through final close, including
 * stop/start. On failure, retain allocator until close succeeds: a partially
 * applied control may still reference it. extra_frames is additional decoder
 * capacity, not total frame count; it must be 1..8. */
struct frame_allocator;
bool lv_aic_player_session_allocator(lv_aic_player_session_t *s,
    struct frame_allocator *allocator, unsigned extra_frames);
bool lv_aic_player_session_start(lv_aic_player_session_t *s);
bool lv_aic_player_session_pause(lv_aic_player_session_t *s,bool paused);
bool lv_aic_player_session_acquire(lv_aic_player_session_t *s,uint64_t *lease,
                                   const struct mpp_frame **frame);
bool lv_aic_player_session_release(lv_aic_player_session_t *s,uint64_t lease);
bool lv_aic_player_session_seek(lv_aic_player_session_t *s,uint64_t microseconds);
bool lv_aic_player_session_volume(lv_aic_player_session_t *s,int volume);
bool lv_aic_player_session_get_volume(lv_aic_player_session_t *s,int *volume);
bool lv_aic_player_session_time(lv_aic_player_session_t *s,int64_t *microseconds);
bool lv_aic_player_session_stop(lv_aic_player_session_t *s);
bool lv_aic_player_session_close(lv_aic_player_session_t *s);
#endif
