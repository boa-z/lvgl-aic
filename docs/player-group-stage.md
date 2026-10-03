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
The APNG backend now permits four independent live instances, including closing
instances with retained readers. Media still permits one. Multiple media masters
and physical mixed-codec arbitration remain to be completed and verified.

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

Backpressure stage strict E907 compile **PASS**; clean combined firmware
boot/app/static/image/manifest **PASS**. Recorded sources:

- sdk: `ddf53daeb92898bcab7ac542c79f85205b09e7dc`.
- lvgl-aic: `1845ef770b6aaa6445573a7a2136546e22eb966f`.
- lvgl: `80ca777e37a2b176770726a02e07a6fb79ef0b39`.

Latest image at the same profile path: SHA256 `74f5a74cea2319c8f7f623331491616e7d059cfff136ca90027c7cf616f2f4cb`.
ELF SHA256: `d9bb3477c88d9d72ed0844bb1d9b04e158f30f44130d6a12783719ce21dab7e8`.

The map checks the media/APNG preserve APIs and checked media submit path.
Physical playback/group timing remains **NOT_RUN**; no flashing performed.


## Bounded APNG multi-instance runtime (2026-10-03)

APNG playback now allows four independent instances, each with its own worker,
path, controls, timeline, stream and immutable frame pool. Per-instance budgets
still apply; aggregate application reservations are the sum of all configured
budgets plus four 8192-byte worker stacks and the previously documented metadata,
packet/codec scratch and LVGL overhead. This does not promise four physical
decodes at the same instant or a particular frame rate.

The owner-thread media runtime acquires one public SDK VE device reference before
launching the first managed media/APNG worker. This initializes SDK lazy state
serially. That reference remains until the last registered instance has finished
its worker and all native frame readers. Failed thread creation rolls it back;
one closing instance never clears another instance's reservation.

APNG SDK tick and close operations additionally share an application-owned
mutex. Inspection found that SDK PNG HAL does not check the result of its finite
`ve_get_client` wait; serializing APNG callers prevents our APNG workers from
contending at that unchecked boundary. The lock is released between ticks and
cleanup retries; it is not held while waiting for readers or sleeping.
SDK media codec threads and unrelated external VE users are outside this gate.
Mixed media/APNG hardware timing and arbitration still require physical tests;
no SDK source was changed and no hardware concurrency claim is made.

Host **41/41 PASS** (plus focused checks after serialization changes).
The APNG contract runs four real OSAL-mapped pthread workers and real snapshot
pools with mocked stream decoding: independent pause/rate/replay, distinct
immutable pixels, the fifth-instance rejection, reservation retention by a
native reader after worker exit, progress during another worker's cleanup retry,
and reuse while old pixels remain held. Mock codec calls deliberately overlap
without the shared gate and assert mutual exclusion. A dedicated runtime contract
covers failed VE open, shared acquire/release, final close and subsequent reopen.
This establishes adapter ownership and scheduling, not physical PNG decoding.

Multi-instance APNG stage strict E907 compile **PASS**; clean combined firmware
boot/app/static/image/manifest **PASS**. Recorded sources:

- sdk: `0b6c6eee96ef8af3312c21a2692399cec3f45df3`.
- lvgl-aic: `6926fb2ea0ad22589860c1d53952f8941b9a26c1`.
- lvgl: `80ca777e37a2b176770726a02e07a6fb79ef0b39`.

Latest image at the same profile path: SHA256 `23ae371ca2705f1982ec069712a6ab57b6d882b3c892196bc7d27857988b9fb3`.
ELF SHA256: `927759b619378dddcd5d2e0ce66164c22874418e70337d249f58dff064586622`.

The linker map retains shared runtime acquisition/release. Physical APNG
multi-instance decoding and mixed media arbitration remain **NOT_RUN**.
No flashing or SDK source edits were performed.


## SDK VE arbitration return protection (2026-10-03)

Before enabling multiple media workers, SDK review found that PNG, JPEG, H.264,
zlib and JPEG-encoder callers ignore `ve_get_client()` failure. The current HAL
waits at most 2000 ms; a contender could therefore proceed to register accesses
without owning the hardware client.

Application SCons integration now applies GNU ld `--wrap=ve_get_client` when
media sessions or APNG are enabled. The component wrapper calls the real SDK
entry and retries a nonzero result after a 5 ms yield. It returns only once SDK
arbitration succeeds. SDK release and hardware decode timeout/reset policies are
unchanged. No SDK source is patched. This covers linked SDK codec callers,
including media player's internal decoding threads; the existing APNG operation
gate remains useful for serializing its tick/cleanup operations.

A permanently unavailable arbiter intentionally stalls the decoder worker and
retains its resources. Returning an error is not a safe cancellation mechanism
because these SDK callers ignore it. This is not a bounded-close guarantee; do
not terminate blocked workers or force-release DMA readers. The real SDK lock's
timeout still bounds each individual retry, not the entire acquisition.

The host contract uses an unchecked caller in a separate translation unit and
actual GNU symbol wrapping. Forced failures cannot reach simulated registers,
then two contending pthreads complete forty calls with mutual exclusion and
balanced SDK releases. The firmware verifier disassembles the final ELF and
requires live PNG/JPEG/H.264 call sites to target the wrapper, the wrapper to
target the real SDK entry, and no direct bypass from other linked functions.

Media remains single-instance until its independent session, callback, allocator
and shared-audio lifecycle are verified. Runtime and hardware mixing are not
inferred from this prerequisite. Physical contention/codec acceptance **NOT_RUN**.

Arbitration stage: host **42/42 PASS**, strict E907 compilation **PASS**;
clean combined firmware boot/app/static/image/manifest **PASS**. Sources:

- sdk: `1ceadb6db4cf05ce94c19a55ece68326293f1443`.
- lvgl-aic: `0cb4072651f59c1ae4d1a65c30c3eeaaf9259afb`.
- lvgl: `80ca777e37a2b176770726a02e07a6fb79ef0b39`.

Latest image at the same profile path: SHA256 `460d7afe2ae7e8bf409fabd025ae661d330786b39dba7f9a574f3019501e2d55`.
ELF SHA256: `013f285e81e4ec20635078b8e698b7612eff713bfad16b16276a264c8780fea6`.

The new verifier rejected the previous unwrapped image at `ve_decode_jpeg`.
Final ELF checks passed for `png_hardware_decode` and `ve_decode_jpeg`.
H.264 was not linked into this profile, so its final call path is **NOT_RUN**
for this build. Physical arbitration/decode execution also remains **NOT_RUN**.


## Bounded media instances and audio admission (2026-10-03)

Media playback now permits four independent sessions. Closing instances retain
capacity until worker teardown and native readers finish. Each instance retains
its own CMA budget, allocator, SDK callback mailbox, frame bridge and controls.
This bound is admission policy, not a four-stream hardware throughput guarantee.

SDK audio rendering uses a shared global device. One managed audio-bearing
source therefore reserves an exclusive lease after metadata preparation and
before SDK start. Another audio source faults asynchronously, without silent
muting. The lease persists across seek/reopen, and is released only after SDK
session teardown. Video-only sources coexist. Direct unmanaged SDK audio users
are outside this lease; applications must not mix them with managed playback.
Multi-source audio mixing remains unsupported.

The new real-worker/mocked-SDK contract covers four concurrent sessions, fifth
rejection, per-instance PTS, pause/seek and end callbacks, delayed native-reader
cleanup without blocking peers, audio rejection before start, lease retention
across seek and a deliberately blocked SDK stop, and reacquisition after close.
CMA allocations/free counts balance. Host **43/43 PASS**, strict E907 **PASS**.
Physical multi-codec, audio and mixed media/APNG group execution **NOT_RUN**.

Multi-media stage clean firmware boot/app/static/image/manifest **PASS**.
Sources: SDK `cc369ed149c8c657fda8f6c82b0392edf20cd3c8`, lvgl-aic `4fee7b7d8400b7969cb6312512826f1c702bd2e4`,
LVGL `80ca777e37a2b176770726a02e07a6fb79ef0b39`.
Image SHA256: `58c2c82daef875aa6008ec1a23fcae3cf557ccf8914579f4ae06de49e1387edb`.
ELF SHA256: `208232d5f4bf916164e33950693c09c6cc259bec2d0da381ee95ff267c9c7b14`.
The same combined-profile evidence directory contains the manifest and logs;
board validation remains **NOT_RUN**.
