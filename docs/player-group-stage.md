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
PTS synchronization or simultaneous display scanout. Group membership now
enables worker-side preservation of frames not yet consumed by the UI.
SDK media get_frame may itself drop late frames for A/V synchronization;
the application does not override that SDK clock or guarantee decoded-index
matching. See the backpressure stage below.
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

## Combined firmware evidence (2026-10-03)

Boot/app/static/image/manifest **PASS**, all recorded source states clean.
- sdk: `8c62260a57e29be9da760bf698aa02742d50a34b`.
- lvgl-aic: `a855c61cf096f11b568907a40fbe0b0c9e6eef2f`.
- lvgl: `80ca777e37a2b176770726a02e07a6fb79ef0b39`.

Artifact under the isolated SDK worktree:
`output/lvgl-evidence/ge2d-fonts-gif-widgets-aicp-player-apng/images/d13x_D50T-2-Lite_page_2k_block_128k_v1.0.0.img`.
Image SHA256: `259bda2c0dbcb8bc6f66a9bf89ed018942a23885fe4b9ab6375e34bb3136f3ff`.
ELF SHA256: `9437ac309f3dce721ef6b626714e4159f0af6d1b32960f51fea550760564de33`.

The linker map verifies group lifecycle/control and member attach/query roots.
Hardware execution **NOT_RUN**; this build does not establish multi-decoder or
multi-view device behavior. The existing APNG overlay remains a standalone
master/slave test; group behavior was exercised by host contracts. Evidence-only
follow-up commits do not alter these firmware source identities.


## Worker backpressure (2026-10-03)

Joining a unified player group enables preservation before start or on the live
backend. Detaching or deleting the group restores latest-wins behavior; moving
between groups never temporarily disables preservation. Worker cleanup/replay
still intentionally discards pending old publications. Multiple-instance limits
are unchanged.

Media waits before the next SDK get while a mailbox publication is unconsumed.
If get/cache handoff was already in flight when preservation was enabled, its
lease is retained pending (or pinned DEFERRED) until the older image is polled.
The checked submit result distinguishes WAIT from failure without racing a UI
poll. Final EOS leases are retried before terminal handling, not stranded.
Drain still returns old leases on the worker; uncertain DMA is never reclaimed.

APNG stops advancing composition/disposal while its prior image is unconsumed
or a composed canvas awaits a free snapshot. Pause/rate/replay/close still run
on bounded iterations. Snapshot exhaustion therefore retries the same canvas
instead of overwriting it. Timeline deadlines can become late, but every frame
is composed and published in order while preservation remains enabled.
Native readers still own their immutable snapshots until released.

Failed image creation leaves preserved READY data retryable; the widget also
allocates its shared owner before consuming a frame. Neither path should drop
an already-preserved frame merely because the LVGL owner allocation failed.
Enabling on a live producer cannot recover earlier drops, and one in-flight
frame can exist beyond the queued publication.

Host **40/40 PASS**, with focused worker/mailbox tests covering three ordered
frames, pause/volume/rate while waiting, full snapshot pools, replay discard,
late policy changes during cache handoff, EOS arriving during preservation
activation, and missing-decoder poll retry. These use real adapter/mailbox code
with mocked SDK/stream boundaries, not physical decode evidence.
