# Current capabilities and SDK gaps

Latest combined evidence: **78/78 SVG/vector/Lottie-enabled host PASS**, **74/74 baseline PASS** and GE/widget/SPI full firmware
build/link/image/provenance **PASS**, including the two-slot SPI display pipeline
and widgets/benchmark/music/vector/SVG/Lottie enabled together, plus premultiplied
GE sources, consistent native software fallback, checked Lottie source loading
and corrected SVG transforms/clipping;
see [current validation](validation.md). The 74-test disabled baseline belongs to
the preceding premultiplied stage; the SVG transform increment ran the 78-test suite.
Board acceptance remains scoped to previously supplied logs; the new image is
**NOT_RUN**. Milestone counts below describe their historical checkpoints.

## SVG composition increment

Native SVG now composes a premultiplied vector layer before applying image
opacity/recolor, rounded clips, masks, color keys and tiling. Straight-alpha
destinations are preserved, and shared software masking scales premultiplied
RGB with alpha. Host **79/79 PASS**, disabled baseline **74/74 PASS**; target
evidence is pending. See [composition](svg-composition-stage.md), including the
additional temporary pixel memory and native sampling boundaries.

## SVG transform increment

Native SVG custom drawing now preserves image origin, pivot, per-axis scale and
rotation through clipping and offset child layers. Canvas drawing without an
object is supported. The independent pixel regression covers 48 canvas/widget/
layer combinations; full host suite **78/78 PASS**. See the
[transform stage](svg-transforms-stage.md); combined target build/link/image/manifest
also pass. Physical execution remains **NOT_RUN**.

## Checked Lottie resources increment

Native Lottie now has component loaders with explicit errors, encoded/staging
limits and failure-preserving replacement from memory or LVGL filesystem drives.
See [checked Lottie resources](lottie-resources-stage.md) for tested failure modes
and renderer heap boundaries. Physical execution remains **NOT_RUN**.

## Premultiplied composition increment

GE IMAGE/LAYER now accepts explicit or flagged premultiplied ARGB8888 sources,
including supported transformed/tiled operations. Native software fallback now
recognizes flagged sources across plain/rounded/recoloured/transformed drawing.
See [GE premultiplied stage](ge-premult-stage.md) for configuration, host versus
hardware evidence and numeric probes; board execution remains **NOT_RUN**.

## Current player/APNG status (2026-10-04)

Player width/height requests now resize the native DE video window directly,
including pre-open and paused requests, while ordinary image playback derives
scale from actual decoded frame dimensions. Manual `lv_aic_plane_test size`
controls are packaged in the current combined firmware. Strict image transform,
visibility and clipping restrictions still apply; physical scaling is NOT_RUN.

The unified player now routes native `.png`/`.apng` sources to the bounded
APNG worker and other suffixes to SDK media, retaining the same image object,
transforms and slave bindings across drained source replacement. APNG rate,
zero-time replay, finite repeat and metadata use checked common commands.
The standalone APNG widget remains usable without SDK media/audio.
Host **53/53 PASS** and strict combined-feature E907 compile **PASS**.
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
| Display | Framebuffer DIRECT/PAN/VSync and whole-screen GE rotation; opt-in SPI full-frame double buffers, worker, blit and GE conversion | Physical GE rotation, SPI panel binding/TE and multi-display acceptance; physical GE/SPI overlap and throughput (two-slot display pipeline implemented); see [rotation stage](display-rotation-stage.md) and [SPI stage](spi-stage.md) |
| Touch / input | Touch worker, mapping, diagnostics and optional recovery; application-owned encoder and mouse providers create native LVGL indevs | Board-specific encoder/USB mouse sampling and board acceptance remain application scope |
| Image resources | FILE/RAW JPEG/PNG; optional SDK AICP; software BMP RGB555/RGB565/24/32-bit; shared CMA/cache ownership; SDK L-drive .fake pseudo-fills; immutable RGB/YUV frame publication | New AICP/BMP/fake/YUV board probes NOT_RUN; integrated media/APNG workers await physical acceptance |
| Image cache | Component LRU, byte/entry bounds, decode-option keys, referenced-reader lifetime and explicit invalidation | Resource success inferred; direct cache-hit log pending; not transparent generic LVGL cache invalidation |
| GE FILL | Solid rectangles; partial opacity on RGB565/RGB888/XRGB8888, no radius/gradient | 12 board numeric probes and operator visual acceptance PASS; partial ARGB8888 still software |
| GE IMAGE | Four RGB/ARGB/XRGB formats, alpha, bounded transformed tiling, bounded scale, right-angle rotation plus scale, unscaled arbitrary-angle rotation; exact color key for RGB888/XRGB8888 and non-antialiased ARGB8888 without scaling or arbitrary rotation | Color-key ranges/RGB565/filtering, arbitrary-angle plus scale, recolor/masks; YUV uses the separate frame path below |
| GE scale | Nominal 1/16..16; pivot/clip/per-axis handling | Small/unsafe geometry and D13x split interval fall back |
| GE LAYER | Plain composition, bounded 1/16..16 scale with right-angle rotation, and unscaled arbitrary-angle rotation when the child buffer is accessible | Bounded default CMA draw buffers remove the ordinary-heap barrier; allocation fallback and ROTATE regions outside 4..4096 still use software; arbitrary-angle plus scale, YUV and general HW layers remain absent |
| Scheduling | Synchronous, error/task counters, bounded refresh timing | Async work and paired GE ON/OFF board timing |
| Fonts | Optional native FreeType bitmap fonts: dynamic sizes/styles, Chinese fallback and native glyph LRU; real host render/lifecycle tests | New font image needs board validation; vendor AIC cache and global font-byte budget absent |
| GIF | Optional native LVGL 9.6 widget; FILE/RAW playback, pause/resume/restart; host pixel/lifecycle tests; board CLI panel | Default off; new GIF candidate needs board acceptance; no general GIF byte budget |
| Optional core | Host demo selection and opt-in application-owned target widgets/benchmark sources with required fonts and performance monitoring, plus independent music UI, opt-in native vector/ThorVG software rendering and SVG image resources plus opt-in native Lottie animations | Target demo full build/link PASS; independent music target full build/link PASS; vector/SVG/Lottie target build/link PASS; physical demo/vector/SVG/Lottie/benchmark/input acceptance remains; see [vector stage](vector-stage.md) and [target demos](target-demos-stage.md) |
| AIC canvas | Owned ARGB8888 CMA buffer; bounded peak allocation; positioned and clearing centered text; host pixels/lifecycle and target live-link gates | Board CMA/cache/display validation; standalone packed-RGB/linear-YUV fill helper implemented; gradient/CSC numeric probes implemented; physical execution pending |
| Native widgets | Optional upstream canvas/chart/dropdown/roller/slider/table/tabview/textarea/tileview plus arc/button/buttonmatrix/calendar/checkbox/keyboard/led/line/msgbox/spinbox/switch contracts | Board rendering/input acceptance still pending; deprecated list/menu have host interaction contracts, a manual page and target linkage gates; physical input/rendering and direct video-window composition remain pending; camera/player use separate opt-in adapters |

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
6. Remaining integration priorities: camera-enabled final firmware linkage,
   concrete SPI panel binding, richer native SVG coverage and renderer-internal Lottie heap/failure coverage.
   Target widgets/benchmark integration and combined firmware now pass; their
   native entry points still require explicit application startup selection.
   GE rotation, input-provider interfaces and vendor media/resource adapters now
   have implementations; their physical acceptance remains open. Double-buffer session and two-slot display-worker overlap now have host/partial-link coverage;
   physical concurrency and measured throughput remain open.
7. Player now includes SDK-shaped transform accessors and checked deferred
   width/height scaling for media/APNG; their combined final-link gates pass.
   Keep differences in [player command compatibility](player-command-compat.md)
   explicit. Broader GE extensions remain in scope but are not SDK parity claims.

## SDK parity audit

The following distinctions come from the checked-out SDK v9 driver sources,
not from assumptions that every LVGL draw feature is hardware accelerated.

| Capability | SDK source evidence | Port status / next work |
|---|---|---|
| Image tiling | lv_ge2d/lv_draw_ge2d_img.c calls the tiled image helper | Clipped IMAGE tiles with bounded scale/rotation and whole-task preflight implemented; native-size target probes exist; transformed probes and board validation remain; see [tiling stage](ge-tiling-stage.md) |
| YUV image input | lv_ge2d/lv_draw_ge2d.c accepts YUV with orthogonal rotations | Bounded 12-format views, CPU conversion, immutable image publication and native-size GE orthogonal rendering implemented; bounded GE scaling plus orthogonal rotation and transformed tiling implemented; board acceptance pending; see [YUV stage](yuv-stage.md) |
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
from request acceptance. Explicit camera video-plane ownership now shares the player window adapter;
the SDK-shaped barcode enable/disable/only/callback APIs now use a bounded worker
mailbox with callbacks on the LVGL owner. Host lifecycle tests and strict D13x
combined-feature compilation pass. Camera-enabled final image linking, real
barcode decoding, throughput/stack budget and board execution remain unverified.
See [camera stage](camera-stage.md) for configuration and callback lifetime.

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

## Legacy navigation target linkage (2026-10-04)

The widget smoke profile now retains list/menu compatibility APIs and verifies
their live final-map symbols. Initial cached linking missed lv_list_create;
SDK scons -c followed by full rebuild passes boot/app/static/image/manifest.
No automatic list/menu UI is created. Host lifecycle/navigation coverage remains
49/49 PASS from the preceding stage; target input/rendering is NOT_RUN.
- sdk: a9cec9eb4db16e639e7ec4ced377733a673d2a1e
- lvgl-aic: 4fb9c547052af61bb3e2d63b43cc9f88f322d74b
- lvgl: 80ca777e37a2b176770726a02e07a6fb79ef0b39
- images/d13x.elf SHA256: d0256049319f2ba7bae05e8039a9f780ee7bfd3477aa188dd15f6b6eaf7b7302
- images/d13x_D50T-2-Lite_page_2k_block_128k_v1.0.0.img SHA256: 02c75558ff06cdccf97b0b333d64e042b9db0e714a5007bdfdada109fe7a532a

Legacy list/menu now have a dedicated manual page. Host pointer hit tests verify
menu entry/back navigation and preserve the existing carousel/swipe page index;
page refresh and repeated teardown pass. This page increment has not yet been
packaged into a target image. Physical scrolling/touch/rendering remains NOT_RUN.

## Navigation acceptance page firmware (2026-10-04)

Full host regression 49/49 PASS. The combined target now includes the dedicated
list/menu page; boot/app/static/image/manifest PASS. The prior page-only host
evidence is superseded for target integration, not physical acceptance.
Board scrolling/touch/display NOT_RUN. Clean build source identities:
- sdk: aa9c72556fd24dde2ec3505f9cbd61dfb1d49b2a
- lvgl-aic: 8867241e7a2eaaae9f81527efcb0b898b482d8b9
- lvgl: 80ca777e37a2b176770726a02e07a6fb79ef0b39
- images/d13x.elf SHA256: d22a01573346f54222737c729818000ebee5ae32696cb157d758fa579be97597
- images/d13x_D50T-2-Lite_page_2k_block_128k_v1.0.0.img SHA256: cf2c9f4043106035a5cf38b88cf24afc307654e1a5125c2c206d3c1b995c31c8

Navigation host input coverage now includes a real pointer press/move/release
sequence on the twelve-item list, settling scroll animations, leaving/returning
through header hit targets and verifying retained list position with an unmoved
outer page. Three create/delete cycles pass. Runtime sources and the latest
firmware image are unchanged; physical touch driver acceptance remains NOT_RUN.


## Complete widget host selection (2026-10-04)

The combined host cache previously enabled AIC_BUILD_WIDGET_TESTS but left
AIC_BUILD_CONTROL_WIDGET_TESTS off, so the reported 49-test runs did not include
the common-control contract. AIC_BUILD_WIDGET_TESTS now includes both contracts;
the control-only option remains available and explicitly enables its textarea
dependency. Rebuilt suite **50/50 PASS**, including arc, calendar, checkbox,
keyboard, LED, line, message box, spinbox, switch and button controls. Earlier
49/49 results remain valid for their narrower executed set. Runtime/firmware
sources are unchanged; this expands verification, not hardware acceptance.

Common-control interaction follow-up: keyboard VALUE_CHANGED events now exercise
numeric entry, maximum length, backspace, READY/CANCEL notification and detached
textarea behavior. Spinbox tests cover both endpoint rollover directions and
clamping with rollover disabled. Focused control-widget test PASS. Events are
dispatched through LVGL handlers; this is not pointer hit-testing or hardware
keyboard/touch acceptance. Runtime sources and packaged image are unchanged.


### Fill DMA lifetime protection (2026-10-04)

Solid fills and SDK pseudo-image replacement fills now latch submission, emit,
and sync failures until reboot. The dispatcher retains the active task and its
destination layer instead of marking uncertain DMA as a releasable failed task.
Preflight rejection remains retryable; direct callers must retain their output
allocation on a latched fault. No SDK reset operation currently proves quiescence.

Host evidence: 50/50 tests PASS, including all three injected fill failures,
replacement-fill retries, pseudo-image propagation and scheduler task retention.
Mock-only fault resets are not a production recovery mechanism. Board NOT_RUN.
Broader raw-probe client poisoning and teardown during active DMA remain under
review; this increment does not establish safe deinit or runtime fault recovery.


#### Fill lifetime firmware evidence

Full `ge2d-fonts-gif-widgets-aicp-player-apng` build, static checks, image checks
and provenance manifest PASS with clean sources: component
`aef9e001911101c7c4616ed552efc57f6e755121`, SDK
`865af19e10788d69bfef62375b3279f1b901c1e3`, LVGL
`80ca777e37a2b176770726a02e07a6fb79ef0b39`.
Image `output/lvgl-evidence/ge2d-fonts-gif-widgets-aicp-player-apng/images/d13x_D50T-2-Lite_page_2k_block_128k_v1.0.0.img`
(SDK-relative), SHA256
`aa8b5aa8d725fd1821fcbd4a0fd4c5571f89ec7705e12ce0e111c2832d1d9122`.
Physical board validation remains NOT_RUN; this is build evidence only.


### Shared GE client quarantine (2026-10-04)

The client now aggregates fill, leased RGB/YUV and direct-probe DMA faults.
A quarantined device is unavailable to subsequent users; queued GE tasks stay
waiting, and the image entry rejects work before any software fallback can touch
a possibly active destination. The raw RGB565 key probe reports its uncertain
DMA to this shared latch while retaining its three buffers.

Client deinit retains the SDK handle on fault, and init refuses to reopen it.
Stats reset cannot clear quarantine. Healthy close still releases its client.
This supersedes the client-close/raw-probe pending items above, but does NOT make
full LVGL/display/object teardown safe during uncertain DMA: reboot is required.
Other independent GE clients and SDK reset semantics remain outside this guard.

Host coverage checks all four fault origins, no resubmission, queued-task state,
client open/close counts and pseudo-image refusal without CPU writes. Board
NOT_RUN; no runtime recovery or reset procedure is claimed.


#### Shared-client firmware evidence

Full GE/fonts/GIF/widgets/AICP/player/APNG boot/app/static/image/manifest gates PASS.
Clean source component `78e70bd531e13f2527fc43b11944bb01e36ed9d9`, SDK
`f8413d83045fc7ab207b1a3ee5cc3582d21999f1`. Host **50/50 PASS**.
SDK-relative evidence image: `output/lvgl-evidence/ge2d-fonts-gif-widgets-aicp-player-apng/images/d13x_D50T-2-Lite_page_2k_block_128k_v1.0.0.img`.
SHA256 `ca55e795522a2fff8465d00097a559425800bd3afa7c0aebcf243df9e67957a5`. Board **NOT_RUN**.

Remaining fault-lifetime work: ordinary FILE/RAW decoder resources are closed by
LVGL image helpers after draw callbacks. The leased RGB/YUV source protection
does not establish safe lifetime for every generic decoder on DMA failure.
Audit and retain those resources before claiming complete GE fault isolation.


### Generic GE decoder lifetime (2026-10-04)

The GE image path now owns one stable-address decoder descriptor across normal
and tiled full-image submissions. It closes only after successful synchronization
or preflight rejection. Failed bitblt/rotate/emit/sync retains decoder-owned pixels,
cache references and custom session state, and triggers the shared client/task
quarantine. No LVGL or SDK source changes are required. Partial decoders still
use the software renderer; successful hardware operations preserve their geometry.

Host coverage includes FILE and RAW custom decoders whose close callbacks destroy
the pixel allocation, all three DMA failure stages, normal/tiled paths, cache drop,
retry refusal and init/deinit attempts. Existing RGB snapshot/producer leases,
rotation and transformed tile contracts remain covered. Test-only cleanup is
possible because mocks start no DMA; production recovery still requires reboot.
External caller-owned variable pixels and child layers must stay alive while their
task is in progress. Full LVGL teardown during a fault is still unsupported.
Board validation NOT_RUN; this supersedes the generic decoder pending item above.


#### Generic-decoder firmware evidence

Host **50/50 PASS**. Full GE/fonts/GIF/widgets/AICP/player/APNG
boot/app/static/image/manifest gates PASS with clean sources: component
`39972d809a7fc33c33c7e99387e3752e9804e691`, SDK
`96b7b0f3f28d41a3f5722564168f3da271a44189`.
SDK-relative image `output/lvgl-evidence/ge2d-fonts-gif-widgets-aicp-player-apng/images/d13x_D50T-2-Lite_page_2k_block_128k_v1.0.0.img`,
SHA256 `b8771c2f431542055cb4345c4322d109e29c20577deac903dcb579886d2a523a`. Board **NOT_RUN**.


### Next SDK widget gap: AIC canvas

The SDK `packages/artinchip/lvgl-ui/aic_widgets/aic_canvas/v9/lv_aic_canvas.h`
exports create, owned-buffer allocation, text drawing and centered text helpers;
implementation is delivered in architecture-specific archives. Its adjacent
`canvas_image.c` supplies CMA image allocation and GE fill utilities. Current
native canvas contracts do not establish parity with those ownership/convenience
APIs. Add an application-owned adapter with explicit allocation bounds and
lifecycle tests; do not link the SDK's LVGL 9.1 private-ABI archive into 9.6.


### Application-owned AIC canvas (2026-10-04)

Opt-in `AIC_LVGL_USE_CANVAS` supplies the SDK-shaped create/alloc-buffer/text/
centered-text entry points on native LVGL 9.6 canvas. Buffers are transparent
ARGB8888, 64-byte row aligned, allocated in CMA on target. Dimensions are bounded
1..4096 and a configurable 4 MiB default peak budget includes old plus replacement
storage. Invalid sizes, exhausted budgets and allocation failures preserve the
old image. Successful replacement and deletion release owned storage. Native
canvas drawing APIs can be used; native source/buffer setters must not replace
this adapter's owned buffer. Calls require the serialized LVGL owner thread.

The widget profile enables and live-links the new APIs. Host contracts exercise
real glyph rendering, centered/wrapped/clipped text, transparent initialization,
padded stride, allocation failure, budget rejection and repeated replacement/
deletion with balanced allocations. Host pixels do not validate target CMA/cache
or GE execution. No SDK archive is linked; SDK standalone `lv_mpp_image_*` and
`lv_ge_fill` helper API parity remains separate. Board NOT_RUN.

SDK E907 v9 archive disassembly confirms centered-text clears to transparent
(`lv_ge_fill` with all color/blend arguments zero), then measures unwrapped text
with zero spacing. The adapter preserves this replacement behavior; ordinary
positioned text draws over existing content. Empty centered text clears the
canvas. Centered measured text is bounded to 4096 x 8192 before mutation.


#### AIC canvas firmware evidence

Full GE/fonts/GIF/widgets/AICP/player/APNG boot/app/static/image/manifest
gates PASS, including live canvas entry points. Clean sources: component
`11e5b19d28069c88242b9b3117791f2f77a27a05`, SDK
`6166afaec164143c7eefe6faa172b3b1346983e6`.
SDK-relative image `output/lvgl-evidence/ge2d-fonts-gif-widgets-aicp-player-apng/images/d13x_D50T-2-Lite_page_2k_block_128k_v1.0.0.img`,
SHA256 `b0a49d708eebaa446403078990c76c5ce85f0daf73b12842eaf81cbaa066880f`. Board **NOT_RUN**.


### AIC canvas acceptance page (2026-10-04)

The widget-enabled manual UI adds a final `AIC canvas` page (6/6 in the full
GE profile). Its transparent text buffer sits over a blue-to-red background.
`Replace text` alternates long/short text using the clearing centered helper;
`Rebuild buffer` replaces storage and redraws the same text. The generation
counter and text choice survive page changes. The host pointer contract exercises
both buttons and checks buffer replacement, retained state and create/delete
cycles. An 800x480 software screenshot was visually inspected for clipping and
control overlap; it is not evidence of CMA, cache or hardware blending correctness.

Deferred board procedure: reach this page with Next, confirm the background is
visible around glyphs, alternate text repeatedly (no ghost glyphs), rebuild
repeatedly (no corruption), and leave/return (same generation/text). Record serial
logs and image identity with the visual result. No board result is claimed here.
Host snapshot: set `AIC_CANVAS_PAGE_PPM` to an output path when running the manual
page contract to save the software frame after replace/rebuild interactions.


#### Canvas acceptance-page firmware evidence

Host **51/51 PASS**. Complete GE/fonts/GIF/widgets/AICP/player/APNG
boot/app/static/image/manifest gates PASS. Clean component
`f7f0f7c98af63f8491adbc4ac9301e863fb65ac3`, SDK
`2054943be3f13304d32a1da003ddfbb21bed5c59`.
SDK-relative image `output/lvgl-evidence/ge2d-fonts-gif-widgets-aicp-player-apng/images/d13x_D50T-2-Lite_page_2k_block_128k_v1.0.0.img`,
SHA256 `e18661e1b9804a219177f0e1160034658acdbdfab6c6f14a680c495adc7a0e31`. Board **NOT_RUN**.


### Standalone SDK-shaped image buffers (2026-10-04)

`canvas_image.h` now supplies `lv_mpp_image_alloc`, `lv_mpp_image_flush_cache`
and `lv_mpp_image_free` under the canvas option. Only SDK-supported ARGB8888 and
RGB565 are accepted. Allocation uses true MEM_CMA with 64-byte base/stride
alignment, dimensions 1..4096 and checked 32-bit physical range. Default per-image
budget is 4 MiB; `lv_aic_mpp_image_alloc_bounded` accepts an explicit byte bound.
Pixels retain SDK uninitialized-allocation semantics: initialize before display.

An internal owner list preserves actual addresses/capacities independently of
public crop/descriptor edits. Unknown/NULL handles are ignored. Shared GE faults
refuse allocations/cache operations and retain buffers until reboot. Callers must
retire external image/cache/decoder users and complete DMA before normal free;
this allocator does not infer arbitrary external leases or impose a global budget.
UI-owner serialization is required. The standalone `lv_ge_fill` entry is still
pending; these APIs do not yet claim SDK helper parity for GE gradients/blending.

The real implementation is tested with SDK MPP headers and mocked CMA/cache calls:
format/budget/stride/range checks, CMA failure, public metadata edits, concurrent
owners, non-head removal and 100 balanced lifecycle cycles. Mocks never dereference
physical addresses and do not prove actual CMA/cache behavior. Target live-symbol
gates include all four APIs; board NOT_RUN.


#### Standalone image-buffer firmware evidence

Host **52/52 PASS**. Full GE/fonts/GIF/widgets/AICP/player/APNG
boot/app/static/image/manifest gates PASS, including allocator live symbols.
Clean component `3180cc38ea6d58b62e4f71ccc20381e28814dc0d`, SDK
`443b32995c4e87c310e35abfcdea888996ae5fcd`.
SDK-relative image `output/lvgl-evidence/ge2d-fonts-gif-widgets-aicp-player-apng/images/d13x_D50T-2-Lite_page_2k_block_128k_v1.0.0.img`,
SHA256 `c85f345fab0e0fd8d097b176ec8f318d46a59efbfff5643168054dc479d693d3`. Board **NOT_RUN**.


### Standalone GE fill helper (2026-10-04)

`lv_ge_fill` now submits SDK native solid, horizontal and vertical linear fills
through the initialized shared GE client. It preserves start/end ARGB values,
per-pixel alpha and GE_PD_NONE defaults, with blend off/on. All twenty SDK packed
RGB destination formats are accepted; multi-plane/packed YUV remains a separate
gap. Ordinary LVGL gradient-task acceleration is unchanged by this explicit API.

Preflight checks physical range, dimensions, stride and crop, and rejects any
span overlapping but exceeding an owned image allocation. Submission copies the
public descriptor because SDK validation may mutate crop fields. Owned image
buffers are cleaned before GE and invalidated only after successful sync. For
external buffers callers remain responsible for actual capacity, cache ownership
and lifetime; the helper does not guess adjacent cache-line ownership.

fillrect/emit/sync failures latch the shared quarantine, retain managed buffers
and refuse retries. A missing client or malformed descriptor is rejected before
submission without poisoning GE. Host contracts verify descriptor fields, all
packed formats, crop/range rejection, cache sequencing and each failure stage.
They do not establish gradient/blend pixel correctness. Native YUV fill, board
numeric probes and physical acceptance remain open; board NOT_RUN.


#### GE fill helper firmware evidence

Host **52/52 PASS**. Full GE/fonts/GIF/widgets/AICP/player/APNG
boot/app/static/image/manifest gates PASS, including live `lv_ge_fill`.
Clean component `06665c9c5ed3298296c74ac630d8820fb233f784`, SDK
`0f62248e583c41cb87cc78e72d67e2573326ea7e`.
SDK-relative image `output/lvgl-evidence/ge2d-fonts-gif-widgets-aicp-player-apng/images/d13x_D50T-2-Lite_page_2k_block_128k_v1.0.0.img`,
SHA256 `37b2883fb39eee6befafca5b3d26a020b3a55d1f8ca82f17bfdb8801ec948385`. Board **NOT_RUN**.


### Native linear YUV fill destinations (2026-10-04)

The explicit `lv_ge_fill` helper now accepts all twelve linear SDK YUV formats:
YUV420P/422P/444P/400, NV12/21/16/61, and YUYV/YVYU/UYVY/VYUY. SDK fill-checker
subsampling rules are applied to a local descriptor: even horizontal geometry for
4:2:x, even vertical geometry for 4:2:0, and effective YUV crop >=8x8. Input
geometry must lie within the declared buffer. The caller descriptor remains
unchanged. Color-space flags are forwarded to SDK conversion unchanged.

Plane stride, row count, required addresses, 32-bit spans and non-overlap are
validated before any cache or GE operation. Planar U/V strides must match because
SDK output commands share their chroma pitch. Every owned allocation is bounds
checked and synchronized once even when it contains multiple planes. Separate
owners all remain retained after an uncertain DMA failure. External allocations
still require caller-managed capacities, cache ownership and lifetime.

Host contracts cover all twelve layouts, three fill types, blend off/on,
subsampling normalization, missing/overlapping/short planes, shared/separate CMA
owners and failed-sync retention. Tiled YUV layouts are not accepted as fill
destinations. These checks establish descriptors and ownership only; color-space
conversion, gradient endpoints and YUV pixels require board numeric probes.
Board NOT_RUN; ordinary LVGL YUV drawing remains its separate frame path.


#### Linear YUV fill firmware evidence

Host **52/52 PASS**. Full GE/fonts/GIF/widgets/AICP/player/APNG
boot/app/static/image/manifest gates PASS. Clean component
`801edb6237fcdd0a8c4bc35c002fe21f6b317239`, SDK
`fb3a70ddbbc2a01e2080444c7bbb5911241b0c5a`.
SDK-relative image `output/lvgl-evidence/ge2d-fonts-gif-widgets-aicp-player-apng/images/d13x_D50T-2-Lite_page_2k_block_128k_v1.0.0.img`,
SHA256 `102632cc744f95a5d9567030ec3d1f5b94279f9f9526e3e8827c57511729e72e`. Board **NOT_RUN**.


### Native gradient numeric probes and SDK CMDQ defect (2026-10-04)

The existing fill-test runner now includes 24 offscreen native gradient cases
when canvas support is enabled: ARGB8888/RGB888/RGB565, horizontal/vertical,
blend off/on and increasing/decreasing channels. An 8x8 crop in a 16x16 owned
CMA buffer has untouched pixel/row-padding sentinels. Independent arithmetic
checks every RGB sample (tolerance 2, or 5 for RGB565) and replacement alpha.
DMA failure uses shared quarantine and retains the allocation; numeric mismatch
after completed sync releases it. These probes are compiled, not board-executed.

Source inspection found a real reference defect in SDK
`packages/artinchip/mpp/ge/cmdq_ops.c:update_gradient_cmd`: green step writes
`cmd[3]`, then blue overwrites `cmd[3]`; `cmd[2]` is uninitialized. The D50T
profile enables AIC_GE_CMDQ. Distinct green/blue slopes in the probe deliberately
expose this defect. Do NOT interpret descriptor-only helper PASS as functioning
CMDQ gradients. The application-owned correction below fixes the generated
backend; SDK files stay untouched. YUV CSC numeric probes are still pending. Board NOT_RUN.

The host probe contract injects a no-op engine, corrupt green channel and crop
boundary write, and checks all are rejected. It also verifies DMA failure retains
storage and blocks subsequent probes. A synthetic RGB generator drives the 24
nominal cases solely to test the checker; it does not emulate hardware evidence.


#### Native gradient probe firmware evidence

Host **53/53 PASS**. Full GE/fonts/GIF/widgets/AICP/player/APNG
boot/app/static/image/manifest gates PASS; final map includes
`lv_aic_native_fill_test_run`. Clean component
`aad97aead616430782334589b742502a4ff8741f`, SDK
`2e7ecbab5c33985d2939266459f314350eb8f054`.
SDK-relative image `output/lvgl-evidence/ge2d-fonts-gif-widgets-aicp-player-apng/images/d13x_D50T-2-Lite_page_2k_block_128k_v1.0.0.img`,
SHA256 `2ed4e33fad1522258e3d4c0a6addbac9fb0dcb79cbe90e40222579688fa34e63`. Board **NOT_RUN**.
This diagnostic image still contains the known SDK CMDQ gradient defect;
build success is not gradient acceptance. Correct it in application build glue
before promoting a gradient-ready candidate.

### Application-owned CMDQ correction (2026-10-04)

GE CMDQ profiles now generate `build/lvgl-ge-cmdq.c` from the reviewed SDK
source, correcting the green slot and replacing signed negative left shifts
with defined multiplication. The full normalized SDK source SHA256 is guarded;
an SDK update fails closed until this adapter is reviewed. No SDK file is edited.
The generated source retains the SDK license and uses its private GE headers,
so this narrow adapter is SDK-version-specific, not a portable GE replacement.

The application link wraps `ge_cmdq_ops`. Static verification reads the final
ELF operations table to prove CMDQ mode selects the corrected backend. Generated
source and provenance JSON are copied into image evidence and hashed in the
manifest. The original backend remains available to other SDK applications.

Host regression compiles the actual original and corrected gradient builder:
the original fails, the corrected builder passes all ARGB step words in both
directions at lengths 1/2/8/17/4096, with boundary guards. Full host suite:
**54/54 PASS**. This establishes command generation, not hardware pixels.
The older diagnostic image above remains historical evidence; board numeric
acceptance and YUV CSC probes are still pending.

#### Corrected CMDQ firmware evidence

Clean component `5e65ff6f2b6b04e3c4f48821137ef003a36db9f4`, SDK `a97c270ea066ee18279f3ab3994d973449b0d8b9`.
Full GE/fonts/GIF/widgets/AICP/player/APNG boot/app/static/image/manifest gates
PASS, including the final ELF operations-table routing check. Generated backend
and provenance are included in the evidence manifest.
Image `images/d13x_D50T-2-Lite_page_2k_block_128k_v1.0.0.img`,
SHA256 `c2d01c894d3da24b539d613394e1ac34eba0f3e19f428c67fed7e0d298b69464`. Board **NOT_RUN**.

### Native YUV CSC numeric probes (2026-10-04)

The native fill runner now checks 240 solid fills: all 12 linear YUV layouts
(YUV420P/422P/444P, NV12/21/16/61, YUYV/YVYU/UYVY/VYUY and YUV400),
four SDK CSC2 color spaces (BT.601/709 limited/full), and black/white/R/G/B.
Each probe uses a 16x16 descriptor with an 8x8 cropped output in a privately
owned 4 KiB CMA allocation. Every allocated byte is checked: expected Y/U/V
channels within tolerance 2, crop guards, row padding and unused plane capacity.
Solid colors deliberately avoid claiming chroma resampling phase correctness.

The reference uses the reviewed SDK fixed-point CSC2 coefficients. Host mocks
use tabulated primary-color outputs and independently write each layout;
injected no-op, luma/chroma corruption, guard writes and uncertain DMA verify
checker failure and allocation retention. Host PASS is checker evidence only.
This supersedes the missing solid YUV CSC probe entry above. YUV gradients,
blending/round-trip CSC accuracy and real-board pixel acceptance remain open.

YUV-probe firmware evidence: host **54/54 PASS**, full-profile boot/app/static/
image/manifest **PASS**. Clean component `8cbb4b1f95979d05834ddd08124232526336218f`,
SDK `93cf1254efce054ce707339693f30e93cc36d563`.
Image `images/d13x_D50T-2-Lite_page_2k_block_128k_v1.0.0.img`,
SHA256 `93daaa919777fb8f8b3f3304ed9dc7ca10cae9605dfe786baf72ff68752024dc`. Board **NOT_RUN**.

### Packed YVYU / VYUY frame parity (2026-10-04)

The SDK accepts YVYU and VYUY packed 4:2:2 sources, but the port previously
rejected them. `LV_AIC_YUV_YVYU` and `LV_AIC_YUV_VYUY` now extend the frame-only
format namespace without leaking custom tags into native LVGL image headers.
Both formats support checked odd-width CPU layouts, independent BT.601/709
limited/full conversion, immutable image publication, MPP import/export with
packed crop offsets, and the existing bounded GE rotation/scale/tile path.
GE still requires even subsampled dimensions; unsupported geometry falls back
to the CPU decoder. Ordinary JPEG/PNG metadata and RGB GE policies are unchanged.

Host tests cover all 12 formats against independent floating-point conversion
across 256 input patterns and four color spaces, capacity guards, cropped MPP
round trips, native image publication pixels, and GE rotation/scale descriptors.
The board runner additionally checks both new packed layouts in all four color
spaces against the CPU reference, including cropped output guards. Host and
firmware evidence does not establish hardware CSC or panel acceptance.

### Bounded CMA draw buffers for GE layers (2026-10-04)

Default RGB565/RGB888/XRGB8888/ARGB8888 draw-buffer creation now prefers
64-byte-aligned CMA in GE profiles, with a 4 MiB aggregate live-allocation
budget (`AIC_LVGL_GE_DRAW_BUF_BUDGET`). This removes the ordinary-heap address
barrier for child-layer composition. Existing LVGL layer accounting, stride,
alpha clearing and buffer ownership remain active. CMA allocation/budget
failure uses the original LVGL allocator and the existing GE address checks
select software when needed. This applies to default draw-buffer creation,
including default native canvases; custom font/image handlers are separate.

An application link wrapper redirects creation without editing LVGL/SDK or
mutating global handlers. Each CMA buffer owns a stable handler set and an
allocation record. GE quarantine refuses new buffers and retains owned pixel
allocations until reboot; full teardown during uncertain DMA remains unsupported.
The normal owner-thread contract and bounded 1..4096 geometry apply.

Host tests use real low-address memory and real LVGL layer allocation/clearing/
release, exercising all four formats, budget/allocation fallback, handler
isolation, fault retention and repeated LVGL lifetime. Final target disassembly
must show the LVGL layer allocation call routed through the wrapper. Physical
layer composition/cache acceptance remains NOT_RUN.

CMA draw-buffer evidence: **55/55 host PASS**; full GE/fonts/GIF/widgets/AICP/
player/APNG boot/app/static/image/manifest **PASS**, including the actual LVGL
layer allocator call site. Clean component `c1574ee837101a14c59cdb1000e9831358433f1d`,
SDK `65edf972ea87542192b14a2b14c9f9d82e9a31a9`.
Image `images/d13x_D50T-2-Lite_page_2k_block_128k_v1.0.0.img`,
SHA256 `66ea09e3f6b5a7f9450c3e9855bdaedbf6519f78e1386841e3aaf91ab5145394`. Board **NOT_RUN**.

### Lazy child-layer GE scheduling (2026-10-04)

GE evaluation now accepts supported child-layer formats before LVGL allocates
their buffers. Previously the missing buffer forced all initial child tasks
to software even when the eventual allocation was CMA. Dispatch still checks
the actual destination after allocation; fill on heap fallback is rendered by
LVGL software, while IMAGE/LAYER keep their existing source/fake/lease fallback
logic. Partial ARGB fills still stay in software.

Temporary draw-buffer allocation failure keeps the task WAITING for retry,
matching LVGL software scheduling instead of dropping it as FAILED. Tests
exercise delayed allocation, retry, actual software fill pixels, GE submission
and completion/fallback counters. `fill_sw_fallback` now distinguishes accepted
software fills, and board logs subtract it from reported engine work.
Hardware execution/cache acceptance remains NOT_RUN.

Lazy-layer evidence: **55/55 host PASS**; full GE/fonts/GIF/widgets/AICP/player/
APNG boot/app/static/image/manifest **PASS**. Clean component
`87cc9b57cfcc9a4405eceb2d4fba72c93b8a3be9`, SDK
`06cc6063f1271e23ed37865a618a3ddd2bfc1bbd`. Image `images/d13x_D50T-2-Lite_page_2k_block_128k_v1.0.0.img`,
SHA256 `25d3342646ef58b4626e520c6e6e7324cf70135ed0092e62823bfd26464d3103`. Board **NOT_RUN**.


### Camera barcode integration regression (2026-10-04)

SDK-shaped widget APIs and worker decoding are implemented with explicit opt-in;
this is no longer an absent API gap. Host **57/57 PASS**, including callback-driven
close/deletion, binary data, mailbox backpressure and in-flight cancellation.
Strict camera + video-plane + barcode D13x compile and symbol references **PASS**.
Full GE/fonts/GIF/widgets/AICP/player/APNG/barcode boot/app/static/image/manifest
regression **PASS** from clean component `9f7cd6198325e2571b8f7d27d62d4bc8d2d1b3ce`
and SDK `c375eb8f37273fd09dbd8b39ca8dc14545eb7686`.
Image SHA256 `9524bfe6d1b376f7f2a99862e1c91665f51e6d714f76fe7fd4071cf8da637913`.
The image is unchanged because this regression profile disables camera capture.
It links the barcode adapter/vendor archive but does not invoke decoding.
Evidence lives in SDK `output/lvgl-evidence/ge2d-fonts-gif-widgets-aicp-player-apng-barcode/manifest.json`.
Camera-enabled final linking and physical acceptance are **NOT_RUN**; do not
substitute this image for a sensor-configured camera test.


### Full-resolution YUV native gradient probes

The native-fill board runner now adds 32 replacement-gradient cases for YUV400
and YUV444P: four CSC spaces, horizontal/vertical direction and forward/reverse
RGB endpoints. Every allocation byte is checked, including crop guards, stride
padding and unused plane capacity. Expected output uses the reviewed SDK CSC2
coefficients after RGB interpolation, with tolerance 2. These cases complement
24 RGB gradient and 240 solid YUV cases (296 submissions total).
Host **57/57 PASS**; the probe contract injects no-op, luma/chroma corruption,
outside-crop writes and uncertain-DMA retention. Physical GE execution remains
**NOT_RUN**. Subsampled YUV gradients need phase-aware characterization; YUV
blend/round-trip checks remain open. This does not enable ordinary LVGL gradient
draw-task acceleration; it validates the existing native fill helper.


YUV-gradient firmware evidence: full GE/fonts/GIF/widgets/AICP/player/APNG/
barcode boot/app/static/image/manifest **PASS**. Clean component
`65a3e115b9d5a6e47b3dd23ea310c7f95f433158`, SDK
`e742006b3b3bdd62429dac0028ee84b3092b8cb6`. Image SHA256
`bf75eb960d4662c2a2c731e0fe41304f490054a8723770909575cebde2b15d89`.
Evidence directory: SDK `output/lvgl-evidence/ge2d-fonts-gif-widgets-aicp-player-apng-barcode`.
The new probe is built into this candidate; no physical execution or flashing
was performed. Camera capture remains disabled in this profile.


### Native fill stride register guard

`lv_ge_fill` now rejects any active-plane stride above 65535 before cache
handoff or GE submission. SDK `bsp/artinchip/hal/ge/hal_ge_reg.h`
`DST_STRIDE_SET`/`OUTPUT_STRIDE_SET` mask pitches to 16 bits, so a larger
caller-owned buffer could previously pass capacity validation but submit a
truncated row pitch. Existing immutable YUV import already enforced this limit.
RGB888 pitch 65535, aligned ARGB8888 pitch 65532 and planar/semiplanar YUV
pitch 65535 remain accepted; 65536 is rejected without quarantining GE.
Host **57/57 PASS**, including boundary pitches and independent chroma checks.
This is an application-side guard; SDK code is unchanged.


Stride-guard firmware regression: GE/fonts/GIF/widgets/AICP/player/APNG/barcode
boot/app/static/image/manifest **PASS**, clean component
`849dd923702d9db0a964ca64c6cfe7c5695630d8`, SDK
`1f12ae63117686178dcd5576b1491caa6d7ba15f`. Image SHA256
`61643bc962d1ecec22e5df8c8ed19e50f20666d7b4a78b586ff2abaf0678f8f6`.
Evidence: SDK `output/lvgl-evidence/ge2d-fonts-gif-widgets-aicp-player-apng-barcode`.
Physical validation remains **NOT_RUN**; no flashing performed.


### Native RGB alpha-gradient probes

Added 12 cropped RGB gradient/blend probes: ARGB8888/RGB888/RGB565, horizontal/
vertical, forward/reverse endpoints with alpha 32..224. Expected source-over
uses independently interpolated color and alpha against an opaque background;
ARGB output alpha must remain 255. All crop/padding guards remain checked.
RGB565 alpha-ramp tolerance is 9/255 (one 5-bit quantization step plus integer
rounding); existing constant-alpha RGB565 cases retain 5/255 and 24/32-bit cases
retain 2/255. Host fault injection specifically fixes alpha at 128 to verify
that ignoring the alpha gradient cannot pass. Host diagnostics now retain
error logs instead of discarding the failing pixel context.
Host **57/57 PASS**. Board runner now performs 308 submissions: 36 RGB gradient,
240 solid YUV, 32 full-resolution YUV gradient. Actual GE alpha interpolation
and blend behavior remain **NOT_RUN**, including transparent-background cases.


Alpha-gradient firmware regression: full GE/fonts/GIF/widgets/AICP/player/APNG/
barcode boot/app/static/image/manifest **PASS**. Clean component
`6a6ba75de0c5130ceded3abbead983bac125d62e`, SDK
`8043d66184ea9c7c8acf9ec39b966bbaa1e73e5e`. Image SHA256
`4b4967497378ecc0df36e443a5c4dc72f25b2a6132a3e39d2b614811f5d0737c`.
Evidence: SDK `output/lvgl-evidence/ge2d-fonts-gif-widgets-aicp-player-apng-barcode`.
Board execution remains **NOT_RUN**; no flashing performed.


### SPI display migration status

The [SPI stage](spi-stage.md) now includes immutable RGB565 frame preparation,
orthogonal rotation/nearest resize, checked transfer lifetime, component bus/tx
claims, cache handoff, budgeted owned CMA storage, exact-count SDK submission and
checked completion. Reusable panel command sequences provide explicit D/C,
cache cleaning and synchronous command completion with sticky fault retention.
Host **62/62 PASS** and enabled-path D13x compile/component partial link **PASS**.

SPI now also has a bounded producer/worker handoff, OSAL worker lifecycle and
LVGL full-frame RGB565 display binding with one/two budgeted draw buffers.
Host **65/65 PASS**. The new `-WithSpi` profile passes enabled final firmware
linkage and live-symbol checks across the entire SPI chain; it does not initialize
a panel or establish physical output. Subsequent stages provide direct-blit
ownership, statistics/stage timing, lifecycle callbacks and opt-in GE conversion.
The composed CPU and GE worker pipelines pass host contracts (70 tests total).
Opt-in two-tx sessions and a two-slot display worker now separate CPU-source
release from checked DMA completion, with idle polling and bounded backpressure.
Host **72/72 PASS**, enabled D13x component partial link **PASS**.
Still pending: concrete panel initialization/power/TE binding, physical filtering,
GE/SPI concurrency/throughput and multi-display board acceptance.

Full GE/fonts/GIF/widgets/AICP/player/APNG/barcode regression after the SPI stages:
boot/app/static/image/manifest **PASS**, clean component
`0623fafa229e3c6358564e8ee16fcd3fcbe46a86`, SDK
`ff711ba970a6c492ec0b2317ae3ec64a202d1cac`.
Image SHA256 `7857af1e1078c07d7e09c2966229c7a188ea3e78c2b9728ee060b61506eb9867`.
Evidence: SDK `output/lvgl-evidence/ge2d-fonts-gif-widgets-aicp-player-apng-barcode`.
Board validation **NOT_RUN**; no flashing performed.


### GE complete allocation address-window validation

The shared GE buffer gate now validates nonzero data_size and the complete
allocation against the 32-bit DMA address space, before any pointer truncation.
D13x/G73x retain the 0x40000000 lower bound. Previously only the truncated start
address was checked, permitting a wrapped allocation or a wide host pointer to
appear reachable. IMAGE/LAYER/FILL/display paths using the gate now decline these
buffers before hardware submission; existing software fallback policy is unchanged.

**67/67 host PASS**, including actual shared-validator tests for lower boundary,
exact upper endpoint, one-byte overflow, empty span and a >32-bit pointer whose
low bits would otherwise pass. Strict real-header D13x GE utility compilation
PASS; SDK `output/lvgl-ge-address-utils.o` SHA256:
`c335178f361272648e3bae936ab5c9e9c19667acda9bb3d6243f5fed5a3c3dc9`.
Host logs: `output/ge-address-build.log`, `output/ge-address-tests.log`.
No cache/DMA calls occur in the boundary contract. Full firmware refresh and
hardware execution of this increment remain pending; hardware **NOT_RUN**.

### SPI GE conversion backend status

A dedicated CMDQ RGB565 rotate/resize backend with two budgeted CMA staging
buffers is available (`lv_aic_spi_ge2d.h`). Host combined session contract and D13x compile/partial link PASS;
selected explicitly by `lv_aic_spi_session_enable_ge2d` before worker startup. Combined firmware linkage PASS at `eab1b40`, including scaler guards and
image-roller child lifecycle protection. Physical filtering/performance remain NOT_RUN. See [spi-stage.md](spi-stage.md) for source
ownership, fault retention and SDK arbitration boundaries.
