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
changes are opt-in through the alpha lease described below. The player binding below is explicit; use the
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
Use the explicit player output mode below instead of polling a widget-owned backend.

## Verified firmware

Host **44/44 PASS**, strict E907 compile **PASS**. Full clean rebuild
boot/app/static/image/manifest **PASS**, including all five video-plane API roots.
The SDK incremental build initially reused the old link output; `scons -c`
and a full rebuild were required after adding the linker roots.

- SDK: `0a4f929868c811999e7535ce654a8f6084a53c1b`.
- lvgl-aic: `7048a8e72f1837735ce06128b8c4dbc2053cb901`.
- LVGL: `80ca777e37a2b176770726a02e07a6fb79ef0b39`.
- Image SHA256: `d586ca642184688e8ec08067fa8e01d5b48ffd747126b779408f218a71e2cec4`.
- ELF SHA256: `403a700f9b0c3f75160daa0c4d80daf1901b629d0ecc390b170ef7b936497f77`.

Evidence: SDK `output/lvgl-evidence/ge2d-fonts-gif-widgets-aicp-player-apng/manifest.json`.
Physical board **NOT_RUN**.


## Explicit player output mode (2026-10-04)

Call `lv_aic_player_set_video_plane(player, true)` before `set_src`. Default
output remains normal LVGL image composition. The selected mode persists across
stop/start and source switches. The first native video/APNG frame opens the
plane; there is no automatic fallback or reservation before the first frame.
A competing owner is reported as player FAULT. Audio-only sources open no plane.

The player's object rectangle becomes the physical scanout rectangle and its
source becomes an alpha-zero `.fake` window. Native frames remain available to
slave image widgets, with their independent transforms. The MPP fake decoder
and GE replacement path must be initialized; the default display must use
ARGB8888. Explicit player plane mode acquires the UI pixel-alpha lease below;
normal image output and raw plane open do not change alpha. RGB565 output,
rotated/offset displays, native/style transforms, recolor, partial ancestor clipping,
rounded ancestors and non-opaque styles are outside this initial profile and
report FAULT. This is not yet SDK automatic layer selection or rotated-plane parity.

Position and size changes are detected on owner timer passes, including while
paused. Hidden ancestors or inactive screens disable scanout while retaining
the latest widget frame; becoming visible resubmits it. Window repaint and DE
updates are not atomic and need board acceptance during motion/page changes.
Only the default display is mapped to the SDK framebuffer. Active screen, top,
system and bottom roots can submit; inactive screens stop scanout.

Stop, close, source replacement, seek and object deletion retire the scanout
reader before releasing widget image ownership. A failed disable/VSync keeps
the backend and cleanup timer alive. Keep pumping timers until
`lv_aic_player_pending_cleanup()==0`; never force-free retained objects.
Unsupported runtime geometry or submission failure latches FAULT until an
explicit stop/close/source replacement. Slaves are detached through the normal
shared-frame cleanup path.

Host **45/45 PASS** and strict E907 compile **PASS**. Widget tests use the real
LVGL object/timer/native image lifetime code, mocked playback/plane operations,
and a metadata-only fake-window decoder. They cover movement, paused resize,
hide/show, native slaves, seek, replacement, deletion and fault recovery. They
do not prove transparent pixels, physical placement, DMA or scanout. The prior
session firmware evidence above remains historical until a new manifest is recorded.


Player binding firmware boot/app/static/image/manifest **PASS**, including the
linked `lv_aic_player_set_video_plane` API. Source identities:

- sdk: `5f8e627198025b54a4a2189430fad77924e29443`.
- lvgl-aic: `83eac9071bec7278a759ea3eb9ed314ca2eacea6`.
- lvgl: `80ca777e37a2b176770726a02e07a6fb79ef0b39`.
- Image SHA256: `fa68573db421119936dbf811ff7a1fcb34f1fa6ae20a164a4c4871631f0124fe`.
- ELF SHA256: `4c90d9075b8b68517adcd0ec9b85d538154f0b9bae5ddf0e669c3d681b5c11c6`.

The combined-profile evidence directory now contains this build. Board **NOT_RUN**.


## UI alpha lease and overlay roots (2026-10-04)

`lv_aic_video_plane_enable_ui_alpha` is an explicit pre-presentation operation:
it checks ARGB8888 UI format, queries/saves the exact SDK UI alpha configuration,
applies enabled pixel alpha, and waits for synchronization. Repeated calls are
idempotent. `hide` retains the lease; successful `close` restores the original
configuration after video scanout has been disabled. Applications must not use
unmanaged alpha writers during the lease.

The snapshot is retained before the update attempt: a failed update can still
have partially changed SDK state. Apply/restore update or VSync failure latches
an alpha fault, refuses new frames, and retains the session until close retries
succeed. Hiding alone cannot clear an alpha fault. Video readers can be released
after a verified disable even if alpha restoration subsequently fails; the
framebuffer reference/ownership remains until restoration succeeds.

Explicit player plane mode now acquires this lease automatically on its first
visible frame and restores it through existing deferred teardown. Top/system/
bottom LVGL overlay roots are recognized as visible; only inactive screens and
hidden ancestors suppress output. This fixes the prior false hidden state for
players parented under `lv_layer_top()`.

Host **45/45 PASS**, strict E907 **PASS**. Real session tests inject partial
alpha writes, both VSync failures, failed restoration, retry and exact restore;
RGB565/query failure rejects before mutation. Widget tests cover all overlay
roots and inactive screens. Physical alpha compositing remains **NOT_RUN**.

Alpha/overlay firmware boot/app/static/image/manifest **PASS**. The alpha lease
API is verified live in the final ELF. Sources:

- sdk: `b3dd9496d88bb4f2ac248e4a32bcbe5509e063f3`.
- lvgl-aic: `4e9bdba1c3476d8f994e4b3fe5a5e47b0a8fb96f`.
- lvgl: `80ca777e37a2b176770726a02e07a6fb79ef0b39`.
- Image SHA256: `e49c4c7953c9b69d4dc6c94604726c2d5fbbcfae4a88c39d22f11f0f6fe335db`.
- ELF SHA256: `baa27813071f5976e305a3f45539451e1a650d0eee6da2be5cbcd394e81d821c`.

Latest combined-profile manifest records this build. Board **NOT_RUN**.
