# Current capabilities and SDK gaps

Maintained inventory, 2026-10-03. Development branch: codex/sdk-basic-capabilities.
Comparison: the SDK's ArtInChip LVGL 9.1.0 implementation. Earlier
phase documents are historical; source presence and switches are not board proof.

| Area | Implementation | Remaining scope |
|---|---|---|
| Integration | App-owned pins; LV_OS_CUSTOM RT events | Board regression after app/OS refactor |
| Display | One framebuffer, DIRECT, PAN/VSync; whole-screen 90/180/270 GE copy with software fallback before submission | GE target/board rotation acceptance, SPI/multi-display, extended cache/VSync tests; see [rotation stage](display-rotation-stage.md) |
| Touch / input | Touch worker, mapping, diagnostics and optional recovery; application-owned encoder and mouse providers create native LVGL indevs | Board-specific encoder/USB mouse sampling and board acceptance remain application scope |
| Image resources | FILE/RAW JPEG/PNG; optional SDK AICP; software BMP RGB555/RGB565/24/32-bit; shared CMA/cache ownership; SDK L-drive .fake pseudo-fills; immutable RGB/YUV frame publication | New AICP/BMP/fake/YUV board probes NOT_RUN; media device integration remains absent |
| Image cache | Component LRU, byte/entry bounds, decode-option keys, referenced-reader lifetime and explicit invalidation | Resource success inferred; direct cache-hit log pending; not transparent generic LVGL cache invalidation |
| GE FILL | Solid rectangles; partial opacity on RGB565/RGB888/XRGB8888, no radius/gradient | 12 board numeric probes and operator visual acceptance PASS; partial ARGB8888 still software |
| GE IMAGE | Four RGB/ARGB/XRGB formats, alpha, bounded transformed tiling, bounded scale, right-angle rotation plus scale, unscaled arbitrary-angle rotation; exact color key for RGB888/XRGB8888 and non-antialiased ARGB8888 without scaling or arbitrary rotation | Color-key ranges/RGB565/filtering, arbitrary-angle plus scale, recolor/masks; YUV uses the separate frame path below |
| GE scale | Nominal 1/16..16; pivot/clip/per-axis handling | Small/unsafe geometry and D13x split interval fall back |
| GE LAYER | Plain composition, bounded 1/16..16 scale with right-angle rotation, and unscaled arbitrary-angle rotation when the child buffer is accessible | Ordinary D13x heap source and ROTATE regions outside 4..4096 fall back; arbitrary-angle plus scale, YUV and general HW layers remain absent |
| Scheduling | Synchronous, error/task counters, bounded refresh timing | Async work and paired GE ON/OFF board timing |
| Fonts | Optional native FreeType bitmap fonts: dynamic sizes/styles, Chinese fallback and native glyph LRU; real host render/lifecycle tests | New font image needs board validation; vendor AIC cache and global font-byte budget absent |
| GIF | Optional native LVGL 9.6 widget; FILE/RAW playback, pause/resume/restart; host pixel/lifecycle tests; board CLI panel | Default off; new GIF candidate needs board acceptance; no general GIF byte budget |
| Optional core | Host official demo selection; vector remains disabled | Target vector/demo choices and vendor extensions need separate integration |
| Native widgets | Optional upstream canvas/chart/dropdown/roller/slider/table/tabview/textarea/tileview plus arc/button/buttonmatrix/calendar/checkbox/keyboard/led/line/msgbox/spinbox/switch contracts | Board rendering/input acceptance still pending; deprecated list/menu and vendor camera/player/video-window remain outside this profile |

SDK image roller is now optional via AIC_LVGL_USE_IMG_ROLLER: application-owned
carousel, looping, direction, zoom and active selection. Host lifecycle/layout
contracts pass; board acceptance is pending. See [image roller](image-roller-stage.md).
SDK swipe_v1 is optional via AIC_LVGL_USE_SWIPE_V1, with four-position
transitions, stable IDs, state-source cycling and lifecycle contracts.
See [swipe_v1](swipe-stage.md). Both widgets have a shared manual test page;
the combined GE2D/fonts/GIF/widgets cross-build and linked-symbol checks pass.
Physical display/input acceptance remains pending;
SDK media parity remains incomplete; video-window and camera image composition
are implemented separately, while player and video-plane
ownership remain gaps.

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
