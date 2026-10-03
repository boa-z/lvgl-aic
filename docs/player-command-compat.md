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
| ATTACH_SLAVE | Attach native slave to this master | Unsupported | slave object directly |
| SET_PLAYBACK_RATE | Unsupported by SDK media backend | 0.1..10x, finite float | float pointer |
| GET_PLAYBACK_RATE | Fixed 1x | Observed applied rate, if available | float output |
| GET_MEDIA_INFO / ATTACH_GROUP | Reserved, unsupported | Reserved, unsupported | no access |

APNG float rate is rounded to increments of 1e-5 and reduced to a rational
before dispatch; NaN/Inf/out-of-range input is rejected. Typed APNG rate APIs
remain available for exact rational requests. Existing native state/lifecycle
rules apply: restart/seek may wait for old readers, faults remain observable,
and media terminal does not prove clean EOF (SDK PLAY_END also covers failures).
For APNG, pause/rate intent survives zero-time replay, and static PNG may replay
as an extension. Media source selection and APNG source selection still use
their distinct widgets; automatic suffix-based backend switching is not supplied
by this command adapter. Media info ABI, groups and APNG slave sharing remain
explicit gaps rather than partially populated outputs.

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
