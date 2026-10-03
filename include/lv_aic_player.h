/* SPDX-License-Identifier: Apache-2.0 */
#ifndef LV_AIC_PLAYER_H
#define LV_AIC_PLAYER_H
#include "lv_aic_player_playback.h"
#if defined(AIC_LVGL_USE_APNG) && AIC_LVGL_USE_APNG
#include "lv_aic_apng_playback.h"
#endif
#ifdef __cplusplus
extern "C" {
#endif
extern const lv_obj_class_t lv_aic_player_class;
extern const lv_obj_class_t lv_aic_slave_class;
extern const lv_obj_class_t lv_aic_player_group_class;
/* Owner-thread, successful-publication barrier (not PTS/scanout sync).
 * All members must publish once before any publishes the next round. Paused,
 * starved, stopped or ended members hold the group until replay/detachment.
 * One group per master. Add is idempotent and reassigns from the old group.
 * Membership/source/seek/start/stop changes reset publication rounds.
 * Destroying a group detaches surviving masters; deleting a master removes it.
 * Grouping enables worker backpressure; detaching restores latest-wins.
 * Grouping adds no decoder instances: backend instance limits still apply.
 * General nonzero seek is rejected while grouped. */
lv_obj_t *lv_aic_player_group_create(lv_obj_t *parent);
lv_result_t lv_aic_player_set_group(lv_obj_t *player,lv_obj_t *group);
lv_obj_t *lv_aic_player_get_group(lv_obj_t *player);
lv_result_t lv_aic_player_group_add(lv_obj_t *group,lv_obj_t *player);
lv_result_t lv_aic_player_group_remove(lv_obj_t *group,lv_obj_t *player);
size_t lv_aic_player_group_get_count(lv_obj_t *group);
/* Display-only image subclass. Attach/detach on the LVGL owner thread, outside
 * draw callbacks. NULL master detaches; source updates on an idle timer pass.
 * Shares decoder frames, never starts another decoder or copies frame pixels.
 * Use native image transforms, never lv_image_set_src. Master deletion clears
 * the binding automatically; pending readers retain storage normally.
 * Updates are timer-based, not a cross-display scanout synchronization promise.
 * Keep pumping timers until pending_cleanup()==0 after deleting either class. */
lv_obj_t *lv_aic_slave_player_create(lv_obj_t *parent);
lv_result_t lv_aic_slave_player_set_master(lv_obj_t *slave,lv_obj_t *master);
lv_obj_t *lv_aic_slave_player_get_master(lv_obj_t *slave);
typedef enum {
    LV_AIC_PLAYER_CLOSED, LV_AIC_PLAYER_OPENING, LV_AIC_PLAYER_READY,
    LV_AIC_PLAYER_PLAYING, LV_AIC_PLAYER_PAUSED, LV_AIC_PLAYER_TERMINAL,
    LV_AIC_PLAYER_STOPPING, LV_AIC_PLAYER_STOPPED, LV_AIC_PLAYER_FAULT, LV_AIC_PLAYER_SEEKING
} lv_aic_player_state_t;
/* LVGL owner only, outside draw callbacks. Image subclass: use native image
 * scale/rotation/pivot/alignment APIs; do not call lv_image_set_src directly.
 * Initialize RGB and YUV decoders before
 * set_src. Explicit budget/extra-frame/colorimetry options are mandatory.
 * LV_RESULT_OK means request accepted, not media/device completion. */
lv_obj_t *lv_aic_player_create(lv_obj_t *parent);
/* Select explicit video-plane output before opening a source (default false).
 * Requires VIDEO_PLANE plus MPP .fake decoder/GE replacement support and an
 * ARGB8888 UI display. Saves/applies/restores SDK UI pixel alpha while the
 * plane is owned; no concurrent unmanaged alpha writers. Display rotation is
 * supported with an explicit rotation budget. Right-angle image rotation and
 * bounded pixel/percentage pivots are supported; objects must remain
 * fully visible rectangles; image scale, style transforms, partial
 * ancestor clipping, rounded ancestors and opacity are rejected at runtime.
 * Object position/size drives physical scanout and a transparent fake window.
 * Hidden ancestors stop scanout; slaves continue normal image composition.
 * Unsupported geometry/plane failures report FAULT; stop/close drains retained
 * scanout before backend destruction. Window repaint and DE are not atomic. */
lv_result_t lv_aic_player_set_video_plane(lv_obj_t *obj,bool enabled);
/* Additional CMA peak budget for display-rotated plane copies, independent of
 * decoder memory; set before opening a source. Zero (default) rejects rotation.
 * GE support is required. Includes both old and replacement scanout copies. */
lv_result_t lv_aic_player_set_video_plane_rotation_budget(lv_obj_t *obj,size_t bytes);
lv_result_t lv_aic_player_configure(lv_obj_t *obj,const lv_aic_playback_options_t *options);
#if defined(AIC_LVGL_USE_APNG) && AIC_LVGL_USE_APNG
/* Configure PNG budgets independently, while no backend/replacement is live. */
lv_result_t lv_aic_player_configure_apng(lv_obj_t *obj,const lv_aic_apng_playback_options_t *options);
#endif
/* Rate changes are APNG-only; media reports fixed 1/1. Queries are transactional
 * and reject inactive/replacing/seeking/faulted sources. */
lv_result_t lv_aic_player_set_rate(lv_obj_t *obj,uint32_t numerator,uint32_t denominator);
lv_result_t lv_aic_player_get_rate(lv_obj_t *obj,uint32_t *numerator,uint32_t *denominator);
/* Native SDK URI (<=127 bytes). Replacing a live source closes it safely first;
 * latest queued source wins. Case-sensitive .png/.apng uses the APNG backend,
 * requiring AIC_LVGL_USE_APNG and configure_apng; other suffixes use media.
 * Unsupported/unconfigured source requests preserve the current source.
 * The image object, transforms and attached slaves survive backend switches. New source is prepared but not auto-started.
 * start during replacement requests play once the new source is ready. */
lv_result_t lv_aic_player_set_src(lv_obj_t *obj,const char *uri);
lv_result_t lv_aic_player_start(lv_obj_t *obj);
lv_result_t lv_aic_player_stop(lv_obj_t *obj);
lv_result_t lv_aic_player_close(lv_obj_t *obj);
lv_result_t lv_aic_player_pause(lv_obj_t *obj);
lv_result_t lv_aic_player_resume(lv_obj_t *obj);
/* One asynchronous seek at a time; old image is retired on an idle draw pass.
 * PNG/APNG accepts zero-time replay only; general seekability remains false.
 * Keeps playback/pause intent; see playback_seek for decoder-reset semantics. */
lv_result_t lv_aic_player_seek(lv_obj_t *obj,uint64_t position_us);
/* Opt-in repeat, default false. Terminal VALUE_CHANGED is emitted first; the
 * next safe timer pass seeks to zero if seekable and video EOS plus a fresh
 * queued frame (or audio-only timestamp) exists. Ambiguous/error-only terminal
 * without that progress remains stopped. SDK notifications still cannot prove
 * clean audio completion. Reuses seek teardown; pause intent is preserved.
 * Disabling prevents future repeats; it does not cancel an accepted seek.
 * PNG/APNG repeats finite completion after fresh publication, using replay.
 * Counter is lifetime accepted automatic seek/replay requests, not completed loops. */
lv_result_t lv_aic_player_set_auto_restart(lv_obj_t *obj,bool enabled);
bool lv_aic_player_get_auto_restart(lv_obj_t *obj);
uint64_t lv_aic_player_get_auto_restart_count(lv_obj_t *obj);
lv_result_t lv_aic_player_set_volume(lv_obj_t *obj,int volume);
lv_aic_player_state_t lv_aic_player_get_state(lv_obj_t *obj);
lv_aic_playback_status_t lv_aic_player_get_status(lv_obj_t *obj);
/* VALUE_CHANGED reports state/applied-volume changes. Handler may delete the
 * widget or queue a new source. TERMINAL is not proof of clean EOF. start on
 * TERMINAL/STOPPED/CLOSED reopens the saved URI; FAULT requires stop/close.
 * Deleted widgets retain cleanup timers until queued draws, image readers and
 * worker exit finish. Run timers until pending_cleanup==0 before lv_deinit;
 * also close/delete live widgets. Never force-release a GE-quarantined frame. */
unsigned lv_aic_player_pending_cleanup(void);
/* SDK-shaped image transforms for player and slave image objects. Native
 * LVGL 9.6 units/validation/notifications apply: rotation is 0.1 degrees,
 * scale 256 is unity; get_scale returns the x scale. This does not select a
 * video plane or change media timing. Use owner thread, outside draw callbacks.
 * Width/height-based automatic scaling is a separate, unsupported SDK API. */
void lv_aic_player_set_pivot(lv_obj_t *obj,int32_t x,int32_t y);
void lv_aic_player_get_pivot(lv_obj_t *obj,lv_point_t *pivot);
void lv_aic_player_set_rotation(lv_obj_t *obj,int32_t value);
int32_t lv_aic_player_get_rotation(lv_obj_t *obj);
void lv_aic_player_set_scale(lv_obj_t *obj,uint32_t value);
int32_t lv_aic_player_get_scale(lv_obj_t *obj);
void lv_aic_player_set_scale_x(lv_obj_t *obj,uint32_t value);
int32_t lv_aic_player_get_scale_x(lv_obj_t *obj);
void lv_aic_player_set_scale_y(lv_obj_t *obj,uint32_t value);
int32_t lv_aic_player_get_scale_y(lv_obj_t *obj);
void lv_aic_player_set_offset_x(lv_obj_t *obj,int32_t value);
int32_t lv_aic_player_get_offset_x(lv_obj_t *obj);
void lv_aic_player_set_offset_y(lv_obj_t *obj,int32_t value);
int32_t lv_aic_player_get_offset_y(lv_obj_t *obj);
void lv_aic_player_set_inner_align(lv_obj_t *obj,lv_image_align_t value);
lv_image_align_t lv_aic_player_get_inner_align(lv_obj_t *obj);

#ifdef __cplusplus
}
#endif
#endif
