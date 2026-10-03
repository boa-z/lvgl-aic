# Player groups: lifecycle and publication barrier

## SDK comparison

The reference `packages/artinchip/lvgl-ui/aic_widgets/aic_player/lv_aic_player.c`
implements `player_should_wait_sync`: after all members reach
`target_frame_pos`, it permits the next successful GET_FRAME round.
`cur_frame_index` counts successful frame requests, not PTS. Group seek is
restricted to zero. Group commands iterate members; they are not transactions.

The application-owned implementation now provides:
- `lv_aic_player_group_create/add/remove/get_count`.
- `lv_aic_player_set_group/get_group`; checked ATTACH_GROUP uses the same path.
- `lv_aic_player_group_control(group, command, payload, &accepted)`.
- A publication-round barrier integrated into the existing unified player timer.

One master belongs to at most one group. Add is idempotent; reassignment unlinks
the old group. Deleting a master removes membership immediately, independently
of worker/frame cleanup. Deleting the group detaches surviving masters without
stopping them. Group objects may also parent their member widgets.

## Progress and controls

A master publishes at most once per round. All members must publish before the
next round can advance. A starved, paused, stopped or terminal member holds
progress; applications must replay, remove or replace it intentionally.
Automatic repeat also respects the barrier. Membership, source, start, stop and
accepted seek/replay changes reset the round. Grouped nonzero seeks reject.
A failed image-wrapper allocation does not count as publication.

Supported broadcasts: START, STOP, PAUSE, RESUME, SET_VOLUME,
SET_PLAYBACK_RATE and zero SET_PLAY_TIME. Every member is attempted; INVALID
with a nonzero accepted count explicitly reports partial acceptance. Accepted
worker requests are not rolled back. Queries, membership commands and empty
groups reject; query payloads are untouched. Only the accepted-count output is
initialized to zero on rejection. Controls enqueue work and do not emit
synchronous widget events, so owner-thread traversal is stable.

Group widgets are distinct from shared-frame slaves: group members own separate
sources/workers; slaves mirror one master without an extra decoder.

## Remaining parity and evidence boundary

This is a successful-publication barrier, not exact decoded-frame matching,
PTS synchronization or simultaneous display scanout. Current asynchronous
mailboxes retain the latest frame and may drop intermediate publications while
a group waits. Worker-side frame-preserving backpressure remains to be added.
Each backend still permits only one live instance, including closing instances
with retained readers: currently one media plus one APNG master can coexist.
Multiple media or multiple APNG masters still require resource/lifecycle work.

SDK review also found a shared VE device with lazy mutex initialization in
`packages/artinchip/mpp/ve/common/ve.c`. Removing reservations without resolving
initialization, decoder serialization and multi-instance resource evidence would
not prove concurrency support. No SDK edits are made in this stage.

Host **40/40 PASS**, including video/APNG producers with deliberate starvation,
automatic-repeat gating, pause/replay, partial rate broadcast, untouched query
outputs, reassignment, duplicate add, group-first/member-first and parent-group
deletion. Strict E907 combined-feature compilation **PASS**. Mocks establish
owner/lifecycle behavior, not hardware decode concurrency. Physical acceptance
**NOT_RUN**. See the firmware evidence below when available.
