/* SPDX-License-Identifier: Apache-2.0 */
#ifndef LV_AIC_PLAYER_FRAMES_H
#define LV_AIC_PLAYER_FRAMES_H
#include "lv_aic_player_session.h"
#include "lv_aic_player_allocator.h"
#include "lv_aic_yuv_image.h"
typedef struct lv_aic_player_frames lv_aic_player_frames_t;
/* UI-owner create/poll/close/destroy, serialized playback-worker submit/drain.
 * Session and allocator storage must outlive the bridge and every image reader.
 * This is a publication mailbox, not a playback clock: submit only when PTS is
 * due. Never dequeue an entire movie as fast as possible into this mailbox. */
lv_aic_player_frames_t *lv_aic_player_frames_create(lv_aic_player_session_t *session,
    lv_aic_player_allocator_t *allocator, lv_aic_yuv_color_space_t space);
/* Transfer an already-acquired SDK lease into the mailbox. False leaves the
 * lease with the worker, which must return it. True transfers responsibility
 * for both allocator pin and SDK put to drain (including a concurrent close).
 * Re-submitting a lease already owned by this mailbox is idempotent.
 * New due frames replace unpublished ones; published readers remain immutable.
 * Only bounded YUV frames are supported here; RGB and FRAME_FLAG_ERROR fail. */
bool lv_aic_player_frames_submit(lv_aic_player_frames_t *frames,uint64_t lease);
/* LVGL owner thread only; YUV decoder must be initialized. Output PTS changes
 * only on success. Detach image and finish queued draws before owner destroy. */
lv_aic_yuv_image_t *lv_aic_player_frames_poll(lv_aic_player_frames_t *frames,int64_t *pts);
/* Worker only. Releases pins then calls potentially blocking SDK put_frame.
 * A failed put retains its SDK lease and can be retried by subsequent drain.
 * A failed pin release quarantines the mailbox, retaining resources. */
bool lv_aic_player_frames_drain(lv_aic_player_frames_t *frames);
/* Close rejects new publication and retires unpublished images. Existing
 * image/decoder/GE leases delay drain; never force release quarantined DMA. */
void lv_aic_player_frames_close(lv_aic_player_frames_t *frames);
bool lv_aic_player_frames_idle(lv_aic_player_frames_t *frames);
/* First close, drain all readers, and stop/join the worker using this bridge.
 * Idle alone does not authorize concurrent destruction while worker can enter. */
bool lv_aic_player_frames_destroy(lv_aic_player_frames_t *frames);
#endif
