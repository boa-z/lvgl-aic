# Checked player command compatibility

`include/lv_aic_player_control.h` provides `lv_aic_player_control(obj, cmd, data)`
for native media-player and APNG widgets on the LVGL owner thread. Command enum
values and payload conventions follow the SDK widget, but this is a checked
`lv_result_t` API, not the SDK's void `lv_aic_player_set_cmd` ABI. OK acknowledges
a request; asynchronous preparation/control/display may still be pending.
Unsupported queries leave their outputs unchanged. Never pass unrelated payload
types or call from a decoder/FinSH worker.

| Command | Media player | APNG widget | Payload |
|---|---|---|---|
| START / STOP | Start / stop | Start / close; saved path retained | none |
| PAUSE / RESUME | Pause / resume | Pause / resume | none |
| PLAY_END | Observed terminal status | Observed finite end | bool output |
| SET / GET_VOLUME | Supported; GET needs applied value | Unsupported | int32_t pointer |
| SET_PLAY_TIME | Asynchronous seek | Zero only: replay | uint64_t microseconds pointer |
| GET_PLAY_TIME | Valid nonnegative observed timestamp | Unsupported | uint64_t output |
| ATTACH_SLAVE | Attach native media slave | Attach APNG slave | slave object directly |
| SET_PLAYBACK_RATE | Unsupported by SDK media backend | 0.1..10x, finite float | float pointer |
| GET_PLAYBACK_RATE | Fixed 1x | Observed applied rate, if available | float output |
| GET_MEDIA_INFO | Complete prepared SDK snapshot | File size and canvas dimensions, no audio | lv_aic_media_info_t output |
| ATTACH_GROUP | Attach/detach unified player group | Standalone widget: unsupported | group directly; NULL detaches |

APNG float rate is rounded to increments of 1e-5 and reduced to a rational
before dispatch; NaN/Inf/out-of-range input is rejected. Typed APNG rate APIs
remain available for exact rational requests. Existing native state/lifecycle
rules apply: restart/seek may wait for old readers, faults remain observable,
and media terminal does not prove clean EOF (SDK PLAY_END also covers failures).
For APNG, pause/rate intent survives zero-time replay, and static PNG may replay
as an extension. The unified player now selects its backend by source suffix;
its existing slave bindings survive backend switches. The standalone APNG widget
remains available without the SDK media/audio dependency. Unified player groups provide a publication barrier;
see [group contract and remaining concurrency work](player-group-stage.md).

## Corrected SDK seek comparison

The SDK's `packages/artinchip/lvgl-ui/aic_widgets/aic_player/player_backend/png_backend_ops.c`
`player_handle_seek` rejects every nonzero timestamp and rejects ordinary PNG.
For animated PNG, zero rewinds to the first frame and starts running. Therefore
arbitrary APNG time seek is **not an SDK parity requirement**. Earlier stage
notes describing APNG seek parity as missing were too broad. Our zero-time
command uses safe replay while preserving pause/start intent, an intentional
asynchronous lifecycle difference. The SDK `aic_backend_ops.c` also rejects
playback-rate changes; claiming video-rate support would be inaccurate.

Validation: full host **39/39 PASS**, including independent media-only and
APNG-only widget builds, invalid/NaN/Inf rates, applied rate round-trip,
zero/nonzero APNG time, untouched unsupported outputs, null payloads, invalid
object/command, signed invalid media timestamps, live media seek dispatch and
slave attachment. Strict E907 combined-feature compilation **PASS**;
`output/lvgl-player-control.o` SHA256:
`fd6a43cf3b5bcad8cca0281d1ecebf0b15e402da8ec9cce5dd3b650fbac7d67d`.
No new firmware or physical playback acceptance is claimed by this stage.

## Coherent media metadata (2026-10-03)

`lv_aic_player_get_media_info` and GET_MEDIA_INFO now share a transactional query.
`lv_aic_media_info_t` spells the current SDK `av_media_info` layout using standard
integer types; size and every field offset are compiled against the real SDK
in the playback adapter. Media preparation copies the complete SDK value into
its mutex-protected status, including 64-bit file size/duration and audio
channels/sample size/sample rate. No live SDK call occurs on the UI thread.
Command copying uses memcpy, avoiding incompatible-struct aliasing when a
current SDK consumer supplies an av_media_info object with the verified layout.

APNG reports actual loaded file bytes and canvas width/height, has_video=1,
has_audio=0, duration=0 (unknown) and general seek_able=0. This follows the SDK
PNG backend's dimension-only metadata semantics, adding known file length;
zero-time replay remains a separate supported control. It does not invent a
media duration or advertise arbitrary seek.

Queries are accepted only for prepared/playing/paused/terminal sources. Closing,
replacement, seek transition, fault and missing metadata reject without writes.
Volume/time command queries now use the same source-state gate, fixing stale
old-source values during replacement/close. Rate queries also reject inactive sources; a prepared media backend reports
fixed 1x, and APNG reports its applied rate.

Host full suite **39/39 PASS**, with updated focused playback/widget checks also
passing after source-state gating. Coverage includes a 6,000,000,000-byte media
file size, complete audio fields, real-adapter metadata publication, APNG file
length, missing metadata and untouched outputs on source replacement. Strict
E907 compile **PASS**, including SDK ABI layout assertions. Command adapter
object SHA256: `cfa4d795f8ff80978178fd03e2468d9b7928374067f0a6933152eabb919f3cdf`.
No new full firmware image or board claim is made by this stage.


## Unified image player backend selection (2026-10-03)

The existing `lv_aic_player` image subclass now chooses PNG/APNG for the exact,
case-sensitive `.png` / `.apng` suffixes, matching SDK routing. Other suffixes
(including uppercase PNG) use media. Configure media with
`lv_aic_player_configure`, PNG with `lv_aic_player_configure_apng`, both before
opening a source; each needs its own explicit budgets. PNG needs
`AIC_LVGL_USE_APNG`, not `AIC_LVGL_USE_APNG_WIDGET`. Disabled/unconfigured
PNG is rejected without changing the old source or attempting video decode.

Replacing a source drains the old worker and all native readers before opening
the latest requested path. Master/slave objects, scale/rotation and bindings stay
intact. Existing media slaves share APNG snapshots without another decoder or
pixel copy. Standalone APNG slave objects still belong to standalone APNG masters.

PNG routes pause, start, zero-time replay, metadata and 0.1..10x rate to APNG.
Volume/audio timestamp and nonzero seek remain unsupported. General seek_able
stays zero. Automatic repeat accepts finite completion only after fresh frame
publication; terminal callbacks run before repeat. Saved APNG rate survives
reopening and source replacement; video reports 1x and cannot change rate.
Rate queries during replacement, replay, inactive/faulted states preserve output.

Validation: **40/40 host PASS**, including media-only, standalone APNG and unified
player builds. The unified test holds native readers through video-to-APNG and
APNG-to-video switches, checks latest-source wins, shared slave/transform survival,
replay/pause/rate/metadata and delayed deletion. Strict combined-feature E907
compile **PASS**. Firmware linkage and physical playback are separate gates.

## SDK-shaped image transform accessors (2026-10-04)

The player header now exports paired `lv_aic_player_set/get_` APIs for pivot,
rotation, scale, scale_x, scale_y, offset_x, offset_y and inner_align (16 functions).
They operate on native LVGL image state for both main and slave player classes,
independent of media/APNG backend. Rotation uses tenths of degrees; scale 256 is
unity; `get_scale` reads the x scale, matching native LVGL. Native 9.6 validation
and notification behavior applies; these wrappers do not inject the SDK's extra
synthetic SIZE_CHANGED event or select a native video plane.

This closes the naming/API migration gap for already-supported image transforms.
It does not implement the SDK width/height request fields or deferred automatic
scaling: those must not be approximated with plain object width/height setters.
Command dispatch continues through the checked component API, not the SDK void
set_cmd interface. Physical transformed-plane behavior remains separately gated.

Validation: **70/70 host PASS**, including read/write state on real player and
slave image instances and clean deletion. Strict D13x compile PASS; SDK
`output/player-transforms-lv_aic_player.o` SHA256:
`b0e966f3802ef945415c3b2a6387c2c3ae4f047865d2d59d418990263bfed307`.
Logs: `output/player-transforms-build.log`, `output/player-transforms-tests.log`,
`output/player-transforms-target.log`. Final live-symbol linkage for these new
entry points and physical execution are **NOT_RUN**.

RGB565 GE color-key acceptance remains deferred: the existing `key565_probe`
compares keyed output to independent unkeyed hardware conversion. No hardware
result was supplied for it, so the production software fallback remains active.

## Deferred width/height requests (2026-10-04)

`lv_aic_player_set_width/height` now save independent pixel requests on a main
player, apply them to an existing frame, or wait for first frame publication.
They set both object extent and the matching image scale based on decoded image
dimensions. Requests persist through media/APNG source replacement and are
reapplied at publication. Slave players retain independent native image transforms
and do not accept these main-player requests, consistent with SDK ownership.

Unlike the SDK void setters, these return a checked `lv_result_t` and accept
1..4096 pixels; zero/out-of-range requests leave prior configuration unchanged.
The 256-based scale uses integer truncation, minimum 1. The object extent is the
requested size, while rendered pixels may differ due to scale precision; there
is no automatic aspect-ratio coupling. Explicit player scale/scale_x/scale_y
clears both pending requests, as the SDK's reset-size behavior does. Native image
setters are still available but do not clear component requests. No synthetic
SIZE_CHANGED notifications are injected; native LVGL setters own notifications.

These APIs supersede the earlier unsupported width/height note above. Initial
application is on decoded-frame publication rather than SDK metadata arrival;
audio-only/no-frame resources retain the pending request without manufacturing
image dimensions. Video-plane geometry remains governed by its separate adapter.

Validation: **70/70 host PASS**, covering pre-open requests, first media frame,
immediate resize, preserved requests across resource replacement, invalid bounds,
manual scaling clearing both axes, and APNG first-frame sizing. Strict D13x
compilation PASS; SDK `output/player-size-lv_aic_player.o` SHA256:
`6aa5d612e7e2638b9cd648e2be17785c584f16e29b4255edad88631e61a917bf`.
Logs: `output/player-size-build.log`, `output/player-size-tests.log`,
`output/player-size-target.log`. Full-firmware live-symbol validation and physical
GE/video-plane rendering remain pending, hardware **NOT_RUN**.

Final firmware update: clean combined build at `ad478d0` passes all 18
transform/size live-symbol checks, build and image gates. See
[validation.md](validation.md) for source identities and SHA256. Earlier pending
final-link statements above are superseded; physical execution remains NOT_RUN.

## Native-plane frame dimension correction (2026-10-04)

Deferred scaling now reads dimensions from the current published frame descriptor,
not `lv_image_t` source dimensions. Native-plane binding replaces the image source
with a `.fake` destination window, so its width/height need not match decoded
pixels. Using that window could generate non-unity scale even when the requested
size exactly matched the decoded frame, causing the strict plane adapter to
reject presentation.

A regression publishes a 4x4 frame into a 6x5 native window, then requests 4x4.
It failed before the correction at the unity-scale assertion. Afterward the
object keeps scale 256 and native scanout updates to 4x4. **70/70 host PASS**;
the plane driver is modeled, so this is not hardware geometry acceptance.
D13x strict compilation PASS; SDK `output/player-frame-size-lv_aic_player.o`
SHA256: `d7aa4489daa3efad97c92fb1a10469a1a1805077bf0c76f80609ec982ad8c3ed`.
Logs: `output/player-frame-size-before.log`, `output/player-frame-size-build.log`,
`output/player-frame-size-tests.log`, `output/player-frame-size-target.log`.
Full-firmware refresh and physical execution for this correction remain pending.
The plane adapter still rejects non-unity image scales; this fix does not extend
its supported geometry profile.

## Native-plane destination sizing (2026-10-04)

Player width/height requests now use the native-plane destination window when
plane output is enabled. The requested axis retains unity LVGL image scale;
DE scales the decoded frame to that window. This avoids applying the requested
resize twice and removes the previous non-unity-scale rejection for these APIs.
Ordinary image playback still derives scale from decoded frame dimensions.
Explicit image scale transforms remain subject to the strict plane adapter;
this change does not enable arbitrary transforms or partial clipping. Requests
apply after first frame publication, including requests made before opening,
and can resize the current frame while paused. Other-axis state is unchanged.

Regression first failed on paused 4x4-to-8x6 resizing, then passed after the fix.
Pre-open 8x6 destination sizing is also covered. **70/70 host PASS** (modeled
plane driver), strict D13x compilation with VIDEO_PLANE enabled **PASS**.
SDK object `output/player-plane-size-lv_aic_player.o` SHA256:
`8da5c28b29449ec11253984916ba4ba051e46d966b76b6e79f0f05a47803b350`.
Logs: `output/player-plane-size-before.log`, `output/player-plane-size-build.log`,
`output/player-plane-size-tests.log`, `output/player-plane-size-target.log`.
Full firmware has not been refreshed for this increment; board **NOT_RUN**.

Full-firmware follow-up: clean native-plane resize firmware now passes boot/app,
final-link, image and manifest gates. The source identities, ELF/image hashes
and limits are recorded at the top of [validation.md](validation.md).
This supersedes the pending full-firmware notes for both dimension corrections
above. Physical execution and shell mailbox runtime remain **NOT_RUN**.
