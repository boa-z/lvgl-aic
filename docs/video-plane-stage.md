# Explicit video-plane sessions (2026-10-04)

`AIC_LVGL_USE_VIDEO_PLANE` provides an opt-in application-owned session over
SDK `AICFB_LAYER_TYPE_VIDEO`. Open refuses a second managed owner or an already
enabled SDK layer. No device is opened automatically. Unmanaged SDK player,
camera or direct framebuffer writers must not use this layer concurrently.
Framebuffer open/close must also remain serialized with the UI owner.

`lv_aic_video_plane_present` accepts immutable RGB/YUV image descriptors from
the existing native publication APIs, including player/camera frames. It
acquires a native reader and keeps the producer's frame pinned while scanout
uses it. The producer must provide DMA-accessible, CPU-coherent padded rows.
The adapter validates address spans and performs cache maintenance before
submission. RGB565/RGB888/XRGB8888/ARGB8888 and the existing linear YUV mappings
are accepted; actual DE format/scaler support remains board-dependent.

Rectangles use physical screen coordinates and must fit completely on screen.
The DE performs scaling. Rotation, implicit clipping, automatic UI alpha
changes and automatic widget-to-plane binding are not implemented. Use the
existing video-window API and an alpha-capable UI plane when composing UI over
video. RGB565 global alpha policy remains the application's responsibility.
This is a callable scanout session, not completion of SDK player auto-layer parity.

Normal replacement retains old and new readers through two VSync waits before
retiring the old frame. An update error is treated as potentially submitted,
because SDK layer configuration may partially alter hardware on failure.
Either VSync failure quarantines both frames and rejects further presents.
Only hide/close retries can recover: disable the plane, wait successfully, then
release all readers. A failed close leaves the handle and exclusive reservation
alive. Do not force free it or stop its producer while readers are retained.
The waits block the calling owner thread; asynchronous scanout is not supplied.

Host tests use real RGB/YUV native reader lifetimes and mock framebuffer IOCTLs:
exclusive/open-busy checks, bounds before submission, normal replacement,
update failure, first/second VSync failure, repeated failed close, recovery,
YUV mapping and balanced producer release. These prove resource policy, not DE
register latching, cache coherency or physical scanout. Strict E907 compilation
uses real SDK headers. Physical acceptance remains **NOT_RUN**.


Typical owner-thread usage: initialize native RGB/YUV decoders, open a plane,
poll an explicitly started playback session, and pass
`lv_aic_player_image_source(&image)` to `lv_aic_video_plane_present`. Destroy
the polled image owner afterward; the plane retains its own native reader even
when submission fails. If `lv_aic_video_plane_faulted` is true, stop submitting
and retry hide/close. After successful hide/close, close and drain playback.
Do not call the player polling API independently on a session owned by a widget.
An automatic widget binding remains a separate integration stage.
