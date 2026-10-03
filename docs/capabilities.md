# Current capabilities and SDK gaps

## Current player/APNG status (2026-10-04)

The unified player now routes native `.png`/`.apng` sources to the bounded
APNG worker and other suffixes to SDK media, retaining the same image object,
transforms and slave bindings across drained source replacement. APNG rate,
zero-time replay, finite repeat and metadata use checked common commands.
The standalone APNG widget remains usable without SDK media/audio.
Host **49/49 PASS** and strict combined-feature E907 compile **PASS**.
The video-plane stage clean firmware passed boot/app/static/image/manifest;
source identities and artifact evidence are recorded in [video-plane stage](video-plane-stage.md).
Physical validation remains **NOT_RUN**.

Unified player group lifecycle, checked broadcast and publication barriers are
now available; see [group contract](player-group-stage.md). Remaining player
parity: physical multi-decoder and rotated-plane validation, clipping/geometry
coverage and board test controls.
Audio mixing is unimplemented; reliable concurrent mixing is not established as
a guarantee of the SDK reference either.
An opt-in [video-plane session](video-plane-stage.md) now provides exclusive
native frame scanout and VSync-protected lifetime. Explicit player binding now
tracks a rectangular alpha-zero window, leases/restores UI pixel alpha and drains
scanout on lifecycle changes. Display/image right-angle rotation and bounded
pivots share the fake-window geometry, with an independent CMA peak budget.
Overlay roots are supported; physical validation remains open.
APNG now supports four independent instances with shared SDK decode serialization.
Application linking also protects SDK VE arbitration failure; the final image's
PNG/JPEG call sites are verified by disassembly. Media now permits four independent
explicit-budget instances, with one audio-bearing source reserved across seek/close.
A second audio source faults before SDK start; no silent muting or mixing.
Physical synchronization evidence is still pending. Group backpressure now
preserves unconsumed media/APNG frames; it does not replace SDK A/V timing.
Physical codec/timing/audio/multi-view acceptance remains pending. Arbitrary APNG time seek and
video playback-rate changes are not supported by the SDK reference either.
See [current command contract](player-command-compat.md). The milestone notes
below record historical checkpoints; older “remaining” items for APNG worker,
widget, command, metadata, automatic backend selection and group lifecycle
are superseded here. Physical multi-instance group acceptance remains incomplete.

Maintained inventory, 2026-10-04. Development branch: codex/sdk-basic-capabilities.
Comparison: the SDK's ArtInChip LVGL 9.1.0 implementation. Earlier
phase documents are historical; source presence and switches are not board proof.

| Area | Implementation | Remaining scope |
|---|---|---|
| Integration | App-owned pins; LV_OS_CUSTOM RT events | Board regression after app/OS refactor |
| Display | One framebuffer, DIRECT, PAN/VSync; whole-screen 90/180/270 GE copy with software fallback before submission | GE target/board rotation acceptance, SPI/multi-display, extended cache/VSync tests; see [rotation stage](display-rotation-stage.md) |
| Touch / input | Touch worker, mapping, diagnostics and optional recovery; application-owned encoder and mouse providers create native LVGL indevs | Board-specific encoder/USB mouse sampling and board acceptance remain application scope |
| Image resources | FILE/RAW JPEG/PNG; optional SDK AICP; software BMP RGB555/RGB565/24/32-bit; shared CMA/cache ownership; SDK L-drive .fake pseudo-fills; immutable RGB/YUV frame publication | New AICP/BMP/fake/YUV board probes NOT_RUN; integrated media/APNG workers await physical acceptance |
| Image cache | Component LRU, byte/entry bounds, decode-option keys, referenced-reader lifetime and explicit invalidation | Resource success inferred; direct cache-hit log pending; not transparent generic LVGL cache invalidation |
| GE FILL | Solid rectangles; partial opacity on RGB565/RGB888/XRGB8888, no radius/gradient | 12 board numeric probes and operator visual acceptance PASS; partial ARGB8888 still software |
| GE IMAGE | Four RGB/ARGB/XRGB formats, alpha, bounded transformed tiling, bounded scale, right-angle rotation plus scale, unscaled arbitrary-angle rotation; exact color key for RGB888/XRGB8888 and non-antialiased ARGB8888 without scaling or arbitrary rotation | Color-key ranges/RGB565/filtering, arbitrary-angle plus scale, recolor/masks; YUV uses the separate frame path below |
| GE scale | Nominal 1/16..16; pivot/clip/per-axis handling | Small/unsafe geometry and D13x split interval fall back |
| GE LAYER | Plain composition, bounded 1/16..16 scale with right-angle rotation, and unscaled arbitrary-angle rotation when the child buffer is accessible | Ordinary D13x heap source and ROTATE regions outside 4..4096 fall back; arbitrary-angle plus scale, YUV and general HW layers remain absent |
| Scheduling | Synchronous, error/task counters, bounded refresh timing | Async work and paired GE ON/OFF board timing |
| Fonts | Optional native FreeType bitmap fonts: dynamic sizes/styles, Chinese fallback and native glyph LRU; real host render/lifecycle tests | New font image needs board validation; vendor AIC cache and global font-byte budget absent |
| GIF | Optional native LVGL 9.6 widget; FILE/RAW playback, pause/resume/restart; host pixel/lifecycle tests; board CLI panel | Default off; new GIF candidate needs board acceptance; no general GIF byte budget |
| Optional core | Host official demo selection; vector remains disabled | Target vector/demo choices and vendor extensions need separate integration |
| Native widgets | Optional upstream canvas/chart/dropdown/roller/slider/table/tabview/textarea/tileview plus arc/button/buttonmatrix/calendar/checkbox/keyboard/led/line/msgbox/spinbox/switch contracts | Board rendering/input acceptance still pending; deprecated list/menu now have host compatibility contracts; target retention and direct video-window composition need separate validation; camera/player use separate opt-in adapters |

SDK image roller is now optional via AIC_LVGL_USE_IMG_ROLLER: application-owned
carousel, looping, direction, zoom and active selection. Host lifecycle/layout
contracts pass; board acceptance is pending. See [image roller](image-roller-stage.md).
SDK swipe_v1 is optional via AIC_LVGL_USE_SWIPE_V1, with four-position
transitions, stable IDs, state-source cycling and lifecycle contracts.
See [swipe_v1](swipe-stage.md). Both widgets have a shared manual test page;
the combined GE2D/fonts/GIF/widgets cross-build and linked-symbol checks pass.
Physical display/input acceptance remains pending;
SDK media parity remains incomplete; video-window and camera image composition
are implemented separately. Explicit player-to-plane composition is available;
physical acceptance and remaining geometry coverage are incomplete.

A [component BMP decoder](bmp-stage.md) now covers FILE/RAW uncompressed
24/32-bit images with full-buffer CMA/cache ownership and host pixel tests.
RGB555 conversion and explicit RGB555/RGB565 masks are implemented.
Combined target build passes; board validation remains pending.

AICP now has an optional [SDK codec integration](aicp-stage.md), with
source/header host contracts; actual AICP decoding and target acceptance
remain unverified. The MPP table's AICP gap is not yet closed.

Declined drawing normally stays with software. Unsupported compressed resources
do not imply another decoder can read them. Whole-screen rotation and IMAGE
rotation are different paths.

## Evidence and sequence

Earlier images have display/touch, decoder stress, GE FILL/IMAGE and selected
transform board observations. LAYER was composited in software. Rotation and
combined-transform visual confirmations do not replace numeric coverage of all
pivot/clipping cases. 3C5 timing code exists; paired board timing remains open.
Current candidate: numeric fill/blend/scale and CMA stress PASS in the supplied
serial excerpt; the operator confirms normal interface appearance. Resource
logs and explicit touch/timed-running evidence remain incomplete. Async log
overflow and timing format compatibility are corrected in the new font candidate;
board confirmation remains required. See [font stage](font-stage.md).

1. Keep current docs aligned while preserving dated validation records.
2. Translucent FILL numeric probes and operator visual acceptance have passed.
   Preserve the geometry/address guards and the 12-probe regression coverage.
   Keep partial ARGB8888 on software until separately verified.
3. Complete the [resource-stage board gate](resource-stage.md): memory inputs
   and bounded cache now include ownership, invalidation, pressure/failure, LRU
   and teardown contracts plus file-memory pixel parity and cache-hit probes.
   SDK allocators remain unchanged. Development checks are in validation.md.
4. The operator confirms the current UI layout and page switching are normal.
   Retain complete native FreeType/resource probe logs for numeric acceptance.
5. Validate the optional [native GIF stage](gif-stage.md) on board. Its decoding
   and lifecycle host coverage does not establish DMA/cache or panel behavior.
6. Remaining priorities: whole-display GE rotation, board input providers and
   compressed vendor formats/media widgets as separate scopes.

## SDK parity audit

The following distinctions come from the checked-out SDK v9 driver sources,
not from assumptions that every LVGL draw feature is hardware accelerated.

| Capability | SDK source evidence | Port status / next work |
|---|---|---|
| Image tiling | lv_ge2d/lv_draw_ge2d_img.c calls the tiled image helper | Clipped IMAGE tiles with bounded scale/rotation and whole-task preflight implemented; native-size target probes exist; transformed probes and board validation remain; see [tiling stage](ge-tiling-stage.md) |
| YUV image input | lv_ge2d/lv_draw_ge2d.c accepts YUV with orthogonal rotations | Bounded 10-format views, CPU conversion, immutable image publication and native-size GE orthogonal rendering implemented; bounded GE scaling plus orthogonal rotation and transformed tiling implemented; board acceptance pending; see [YUV stage](yuv-stage.md) |
| fake image | aic_ui.h encodes dimensions/blend/color in a .fake path; GE turns it into a fill | Implemented bounded parser, LVGL 9.6 virtual-file bridge and GE/CPU replacement/blend; board pending; see [fake stage](fake-image-stage.md) |
| Arbitrary rotation plus scale | ge2d_draw_img_supported explicitly rejects it | Future extension beyond this SDK baseline |
| Recolor / bitmap mask | ge2d_draw_img_supported explicitly rejects both | Software fallback is consistent with SDK; GE support is an extension |
| Screen rotation | SDK submits a synchronous rotated bitblt | Port implemented, 90-degree target build passes; board acceptance pending |
| AICP / BMP | SDK codec / custom software BMP paths | Port implemented and target-built with resource probes; hardware results pending |

The SDK [video-window widget](video-window-stage.md) API is implemented over the existing alpha-zero replacement path; board acceptance remains pending. The camera image widget now binds asynchronous capture to LVGL composition;
full camera/player parity remains open. Their
device and frame lifetime contracts must be ported explicitly; enabling
native LVGL widgets or accepting .fake strings does not supply those devices.
The broad goal still includes GE extensions, but they must not be reported
as missing SDK functionality when the SDK itself declines them.

The [camera capture session](camera-stage.md) now provides tested VIN setup,
buffer ownership and stop/close handling, plus a background worker and immutable
frame publication with deferred queue-back. It has target compilation and
concurrent host evidence. The camera image widget adds prepare/start, pause/
resume, stop/reopen and deferred deletion, tested against the real worker and
mocked SDK. Worker-side sensor input selection now reports driver acknowledgment separately
from request acceptance. Video-plane ownership, barcode, camera-
enabled image linking and board execution remain unverified or missing.

This sequence supersedes the old instruction to stop after 3C5. It does not
waive hardware verification or authorize flashing.

The [player session foundation](player-stage.md) now wraps SDK preparation,
playback/pause/restart, seek/volume/time and decoder-frame leases. It uses
external video rendering and refuses destructive controls while readers hold
frames. An application-owned CMA allocator now supplies verified plane bounds,
explicit memory budget, cache handoff and deferred free for pinned readers;
the bounded MPP importer applies aligned YUV crops. The immutable YUV publication
bridge transfers session/allocator leases to LVGL readers and returns frames
only on its worker, with delayed close and failed-put retry. A pauseable media
clock and coherent SDK callback mailbox now provide timing/event primitives;
PLAY_END remains an ambiguous terminal notification, not clean EOS evidence.
The background playback worker now supports prepare/start/pause/volume,
RGB/YUV publication, asynchronous seek and deferred close, using SDK get_frame synchronization.
Host ABI contracts and target compilation pass;
real A/V timing, APNG, multi-player groups and video-plane integration remain open. The media-enabled image now passes build/link checks; physical playback remains unverified.

Native RGB frame publication now covers the D13x MJPEG RGB565/RGB888/ARGB8888
output path at the adapter level, including bounded crops and immutable decoder
views. GE retains producer and decoded-snapshot leases on DMA failures for both
normal and tiled RGB draws. This does not establish real MJPEG playback or
physical DMA acceptance. See [player-stage.md](player-stage.md).

The optional native `lv_aic_player` image widget now connects the background
worker to LVGL: explicit configuration, prepare/start/pause/resume, volume,
seek, stop/close/replay and deferred source replacement/deletion. Host contracts
verify RGB/YUV rendering and frame-reader lifetimes; strict target compilation
passes. Backend-specific rate, multi-player groups and video-plane output remain gaps. Seek currently drains readers and rebuilds the SDK session to isolate callback generations; exact media seeking still requires board validation.
Media firmware linkage is validated by the optional `-WithPlayer` profile. Physical playback remains NOT_RUN; see the exact image and clean source manifest in [player-stage.md](player-stage.md).

Native display-only slave players now share the master's immutable image and
support independent native image transforms, deferred attach/detach/rebind and
automatic unlink on master deletion. They use no additional decoder session or
full-frame pixel copy. Host rendering/lifetime contracts and target compilation
pass; physical multi-view composition and simultaneous panel scanout are not
certified. See [player-stage.md](player-stage.md).

Rate support is backend-specific: the official AIC video backend rejects
PLAYER_CMD_SET_PLAYBACK_RATE. Its absence here is not a video-backend regression;
rate behavior for the pending APNG backend still needs comparison and porting.

Optional auto-restart now uses asynchronous seek-to-zero after a terminal event
and observed stream progress. It does not retry faults, unseekable media or
terminal-without-progress; applications can disable it in the terminal callback.
SDK audio termination remains ambiguous, so this is not clean-EOF certification.

The [APNG container foundation](apng-stage.md) now validates bounded PNG/APNG
structure and extracts standalone frame PNGs without SDK/LVGL dependencies.
All 100 frames in three existing SDK examples were extracted and host-decoded;
straight-alpha SOURCE/OVER and NONE/BACKGROUND/PREVIOUS software composition now also matches all 100 reference frames exactly. APNG worker scheduling and widget playback are not yet integrated.

APNG now also has a worker-side timeline foundation for finite/infinite loops,
pause/resume, rational 0.1..10x rate and fractional frame delays. Host contracts
and target compilation pass. It is not yet connected to a background MPP decoder
or the player widget; SDK video timing continues to use its own synchronized path.

APNG now also has a bounded worker-only MPP PNG decoder adapter, independently
selectable with `AIC_LVGL_USE_APNG` without the SDK player/audio interface.
It converts native ARGB to compositor RGBA, pins verified CMA allocations and
retains failed frame returns for retry. Host **36/36 PASS** and strict E907
compile **PASS**; APNG playback/publication/widget and board decoding remain
unverified. See [stage details](apng-stage.md).

The APNG serialized stream now integrates container extraction, MPP decode,
RGBA composition and the rational timeline, including source-copy ownership,
bounded stream CPU allocations, ordered late frames, finite/infinite loops,
pause/rate/replay and retryable fault cleanup. Host **36/36 PASS** and strict
E907 compile **PASS**. Its borrowed canvas is not an asynchronous image source:
OSAL worker/mailbox, immutable publication and widget integration remain open.

APNG immutable publication now has a bounded snapshot pool with worker RGBA to
native ARGB conversion, latest unconsumed frame replacement, owner-thread LVGL
image creation and delayed native/GE reader release. Host **37/37 PASS**, strict
E907 compile **PASS**. Worker/file/widget integration and board acceptance remain
open; see the [APNG stage record](apng-stage.md).

APNG now provides an asynchronous native-file playback API under
`AIC_LVGL_USE_APNG`: worker preparation, start/pause/rate/replay, immutable image
polling, coherent status and deferred cleanup. Final publication retries after
reader backpressure. Host **38/38 PASS** and strict E907 compile **PASS**;
widget/backend selection, command compatibility, APNG firmware linkage and physical
codec/timing validation are still open. See [APNG stage record](apng-stage.md).

A native APNG image widget is now available with `AIC_LVGL_USE_APNG_WIDGET`:
explicit configuration, source replacement, start/pause/rate/replay, saved-path
reopen and timer-based deferred deletion. Host **39/39 PASS** and strict E907
compile **PASS**. APNG firmware linkage, SDK player backend/command compatibility
and physical codec/display acceptance remain open; see [APNG stages](apng-stage.md).

The combined GE2D/fonts/GIF/widgets/AICP/player/APNG image now passes clean SDK
compile, live-link, package and provenance checks via `build.ps1 -WithApng`.
APNG is linked but not auto-played; an interactive APNG test page and physical
codec/timing/display acceptance remain open. Exact source/image hashes are in
[APNG stage evidence](apng-stage.md).

The APNG acceptance overlay is now part of the clean combined firmware profile,
with original generated finite/infinite/static fixtures and the UI-thread
`lv_aic_apng_test` shell mailbox. Host **39/39 PASS**, independent fixture
composition **9/9 PASS**, full image/link checks **PASS**. Hardware execution is
**NOT_RUN**; this closes the missing manual test entry, not physical acceptance
or SDK unified backend/command compatibility.

Checked SDK-style command dispatch is now available for both native player and
APNG widgets, with explicit unsupported results and transactional query outputs.
SDK review confirms APNG only supports zero-time replay, while the media backend
rejects rate changes; arbitrary APNG seek/video rate are not parity gaps.
Host **39/39 PASS**, strict E907 compile **PASS**. Automatic source/backend
selection, media-info ABI, groups and APNG slave sharing remain; see
[command compatibility](player-command-compat.md).

APNG now supports display-only slave widgets sharing the master's immutable RGB
image without a second decoder or full-frame copy. The checked ATTACH_SLAVE
command dispatches to APNG or media slaves by master type. Host **39/39 PASS**,
strict E907 compile **PASS**; cross-backend unification and physical multi-view
acceptance remain open. The APNG acceptance overlay displays a master/slave pair.

Media-info querying now returns a coherent prepared snapshot through the checked
command API or typed getter. The public standard-integer layout is compile-checked
against SDK av_media_info; video/audio metadata and APNG file/dimensions are
supported. Queries reject closing/replacement/fault states without stale writes.
Host **39/39 PASS** plus updated focused tests; strict E907 **PASS**. Automatic
backend selection, group lifecycle and cross-backend slave binding remain open.


## Codec applicability on D13x (2026-10-04)

The effective D50T profile selects `AIC_VE_DRV_V30`. SDK
`packages/artinchip/mpp/Kconfig` restricts `AIC_MPP_H264_DEC_ENABLE` to
`LPKG_MPP && AIC_VE_DRV_V10`; requesting H.264 in the profile cannot override
that dependency. Therefore the D13x image has no H.264 decoder call path to
validate. This is an SDK/SoC applicability boundary, not evidence of a lost
LVGL adapter feature. H.264 linkage/arbitration needs a separate supported V10
board profile; do not force the V10 driver into D13x to satisfy a symbol check.


## Default layer selection applicability (2026-10-04)

SDK `aic_widgets/aic_player/player_backend/aic_backend_ops.c:player_select_layer`
selects `LV_AIC_PLAYER_LAYER_UI_DOUBLE_BUF` on D13x/D21x/D12p when
`AIC_MPP_PLAYER_VIDEO_EXT_RENDER` is enabled (also always on D12x). This port's
current D13x external-render default is normal LVGL image composition, consistent
with that SDK choice. Explicit video-plane mode is opt-in. Do not count the
absence of automatic video-plane selection as a missing default behavior in
this profile. Other SDK render profiles and rotated video-plane composition
still require separate implementation/validation.

## Raw video-plane rotation boundary (2026-10-04)

The explicit video-plane session now supports bounded GE clockwise right-angle
rotation with a caller-owned additional CMA budget, including simultaneous old
and new scanout copies. Host 49/49 and strict E907 compilation pass. GE failures
retain potentially referenced memory until reboot; descriptor tests are not
pixel/hardware acceptance. Player widget image/display rotation and transformed
transparent-window placement remain open. See [video-plane stage](video-plane-stage.md).

Player plane display rotation is now implemented with explicit additional CMA
budget and logical-to-physical window mapping (host 49/49, strict E907 PASS).
Native image rotation/pivot and transformed style geometry remain gaps; the
player's own image must remain unrotated. Firmware/board validation for this
increment is pending; see the latest video-plane stage record.

Player image right-angle rotation and bounded pixel/percentage pivot mapping
are now implemented using the same transformed rectangle as the fake window.
Combined image/display orientation follows the SDK subtraction rule. Host
49/49 and strict E907 pass; image scaling, arbitrary angles and partial clipping
remain unsupported. Latest stage firmware/pixel/board validation is pending.


## D13x player transform parity boundary (2026-10-04)

SDK `aic_player/player_backend/aic_backend_ops.c:player_check_hw_capability`
accepts D13x video-plane image transforms only when both scales are
`LV_SCALE_NONE` and rotation is 0/900/1800/2700. The port now covers those image
angles, display-angle composition and bounded pivots. Image scaling and
arbitrary angles are not missing D13x reference behavior. D21x accepts image
scale with right angles and needs a separate capability/board profile review.
DE scaling by object rectangle is distinct from LVGL image transform scaling.
This source-level comparison does not prove physical plane/window agreement.

Player plane image offsets now follow native same-sized fake-window placement,
including right-angle rotation, with bounded offsets and full ancestor visibility.
Nonzero offsets with tile/auto alignment remain unsupported. Focused host and
strict E907 pass; the latest media-path image predates this offset increment.

## RGB565 color-key evidence boundary (2026-10-04)

The SDK GE paths (`mpp/ge/cmdq_ops.c` and `hal/ge/hal_ge_hw.c`) write ck_value
without documenting whether RGB565 comparison precedes or follows expansion.
LVGL compares `lv_color16_to_color` output. Descriptor mocks cannot prove these
spaces agree, even for full-intensity channel endpoints. RGB565 keyed images
therefore retain software fallback. Focused GE tests now verify all eight RGB
cube corner keys and an intermediate key reject before GE submission (PASS).
Closing this hardware acceleration gap needs documented GE comparison semantics
or a board pixel probe; no hardware parity claim is made by this test.

RGB565 key probe combined firmware boot/app/static/image/manifest PASS.
Production RGB565 color-key fallback is unchanged. Board probe execution NOT_RUN.
- sdk: 176b20be97fd9ef9165a0de7e412251083a91be7
- lvgl-aic: dc6a31c23de94603c80540187f7cb5f0f83239c9
- lvgl: 80ca777e37a2b176770726a02e07a6fb79ef0b39
- images/d13x.elf SHA256: 0bba2eab3d1419d6b778dad56790a149aace0cdc76032d21786bbcd067a7d53f
- images/d13x_D50T-2-Lite_page_2k_block_128k_v1.0.0.img SHA256: 301cb15429509edd83f764449bcf2f17448d0982bf2b21315c28258b6971685f


## Legacy list/menu compatibility (2026-10-04)

LVGL 9.6 retains upstream list/menu implementations with deprecation annotations.
They are not missing port implementations. The native-widget host profile now
explicitly enables them and exercises list label/button replacement, menu page
creation, click-driven navigation, return navigation and subtree deletion.
Full host 49/49 PASS. Deprecation warnings are suppressed only around this
intentional compatibility test. New UI should prefer flex containers and explicit
page navigation as upstream recommends. This does not prove target live linkage,
touch input, layout or board rendering, and no new firmware image is claimed.
