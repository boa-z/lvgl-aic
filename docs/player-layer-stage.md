# Player draw-layer compatibility

The ArtInChip SDK player exposes `lv_aic_player_set_draw_layer()` with five
choices: `DEFAULT`, `UI_SINGLE_BUF`, `UI_DOUBLE_BUF`, `VIDEO` and `NONE`.
The application-owned LVGL 9.6 player now exports the same selector.

`DEFAULT` follows the D13x external-render profile and keeps ordinary LVGL
image composition. `UI_SINGLE_BUF` now enables the producer preservation gate,
so an unconsumed published frame applies backpressure until LVGL releases it.
`UI_DOUBLE_BUF` keeps the latest-wins mailbox policy and therefore does not
claim a second SDK-owned buffer. Both choices use the same immutable RGB/YUV
publication and reader lifetime path; the port does not import the SDK 9.1
private double-buffer archive. `VIDEO` selects the explicit video-plane
adapter and therefore requires the video-plane feature and an ARGB8888 display.
`NONE` consumes decoded frames immediately without changing the image source or
slave bindings, so audio-only and diagnostic playback can progress without
blocking a group publication barrier.

The selector is owner-thread only and applies to the next source, matching the
SDK's stored draw-layer field. A live backend is left unchanged until source
replacement or reopen. The explicit `lv_aic_player_set_video_plane()` API
continues to provide an immediate checked admission path and keeps the stored
layer consistent.

Host coverage exercises no-output frame release, the single-buffer preservation
gate, double-buffer latest-wins behavior, UI composition, and VIDEO selection
through the modeled plane adapter. This validates lifecycle and ownership
semantics; layer scanout, alpha programming, cache coherency and panel timing
remain **NOT_RUN**.

## Transform precision and size fallback stage (2026-10-05)

Player width/height requests now update the LVGL object geometry immediately,
including before source preparation or first-frame publication. If decoded
dimensions are not available yet, the native image scale is reset to unity and
the request remains pending; the first frame reapplies the closest Q8 scale
using nearest rounding. This keeps layout and hit testing deterministic while
avoiding a stale scale from a previous source.

Video-plane right-angle admission normalizes signed LVGL rotations in 0.1°
units before composing the display rotation. Negative quarter turns therefore
map to the same clockwise DE angles as their positive equivalents, while
non-quarter turns remain rejected by the D13x SDK capability boundary.

The player widget host contracts cover pre-open geometry, paused/native-plane
resize, source replacement persistence and signed right-angle rotations. GE
and plane pixel/cache/panel behavior remains **NOT_RUN**.
