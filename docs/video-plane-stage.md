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
The DE performs scaling. Rotation uses the explicit rotated API described below;
implicit clipping is unsupported. UI alpha changes require the explicit alpha lease. The player binding below is explicit; use the
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

## Bounded GE video-plane rotation (2026-10-04)

`lv_aic_video_plane_present_rotated` adds explicit clockwise 90/180/270-degree
rotation of native RGB/YUV frames through GE into ARGB8888 CMA storage. Zero
rotation keeps direct scanout. DE scales the rotated result to the requested
physical rectangle. Source dimensions must be at least 8x8; the caller supplies
an independent CMA budget covering both current and replacement copies. Budget,
allocation and geometry rejection leave the previous scanout intact. Builds
without GE reject nonzero angles without faulting the session.

Successful GE synchronization retires the native source reader; the copy stays
owned until verified scanout replacement/disable. Uncertain GE submission,
emission or synchronization permanently quarantines source/copies/session until
reboot: no reset/quiescence guarantee exists to permit safe reclamation. DE
errors after completed GE remain recoverable through hide/close retries.

Host **49/49 PASS**, including disabled-GE and three GE failure injection cases.
Strict E907 compile **PASS**. Tests verify descriptors, budget and ownership,
not pixels or hardware DMA. Player widget image/display rotation is still
unsupported; its geometry and transparent-window mapping need separate work.
Firmware evidence below must be updated after a clean-source build. Board
**NOT_RUN**.

Rotation firmware boot/app/static/image/manifest **PASS**, including the live
`lv_aic_video_plane_present_rotated` symbol. Clean build source identities:

- sdk: `64de6953a3fe2750ddcbd18ce924cf25895d7ff0`.
- lvgl-aic: `c6698d7477f9945a659d0d434fcbacdc7fdf9eee`.
- lvgl: `80ca777e37a2b176770726a02e07a6fb79ef0b39`.
- Image SHA256: `4c8c3394cdda93f1274bb5e91a385b2d527a2bdeebdf48a7652794fc04f26781`.
- ELF SHA256: `0e004b39e8ad5a130c852180bf5e6db24019edf7748d575c833805ffda2c971c`.

The combined-profile manifest now records this build. Board **NOT_RUN**.

## Player display rotation (2026-10-04)

Explicit player plane mode now maps the logical transparent-window rectangle
through `lv_display_rotate_area`, and submits clockwise GE rotation equal to
360 minus the LVGL display rotation. Configure the independent peak CMA budget
with `lv_aic_player_set_video_plane_rotation_budget` before opening a source;
zero defaults to refusing rotated display output. Decoder budgets are unchanged.
The window remains in logical coordinates, while DE receives physical bounds.
Paused display rotation triggers resubmission even when the frame is unchanged.
LVGL 9.6's mirrored offset getters are normalized before rejecting offsets.

Host **49/49 PASS**, strict E907 compile **PASS**. Widget mocks cover all three
angles, physical bounds, paused transitions and refusal to change a live budget.
Raw session tests separately enforce memory budgets and GE failure lifetime.
Native image rotation/pivots, style transforms and partial clipping remain
unsupported. This stage does not establish rotated alpha pixels or physical
scanout acceptance; firmware validation is pending and board is **NOT_RUN**.

Player display-rotation firmware boot/app/static/image/manifest PASS.
The rotation-budget API is live in the final ELF. Board NOT_RUN.
sdk: 956bd27fe3c14b22632167cdced7dae2079b9790
lvgl-aic: db07e82e39a3a0970954915fc0a60386aad0ee83
lvgl: 80ca777e37a2b176770726a02e07a6fb79ef0b39
images/d13x.elf SHA256: 906cc2bb7bbe8107724fbd24dfe9b7533f36cabf3324bc32f15a0eaa37327e81
images/d13x_D50T-2-Lite_page_2k_block_128k_v1.0.0.img SHA256: d79c0024ca065124738d316b20e0da274df24836a2f481df42419b89ca2d7262

Display-rotation preflight follow-up: missing rotation budget now rejects before
opening a plane or acquiring UI alpha. Non-square 16x12 host geometry verifies
all three angles; nine missing-budget/X-offset/Y-offset cases verify failure
without alpha mutation and balanced cleanup. Full host **49/49 PASS**, strict
E907 **PASS**. This follow-up has no new firmware image; the previous manifest
remains the last packaged build. Physical rotated-window acceptance is NOT_RUN.

## Player image rotation and pivot (2026-10-04)

Plane players now accept image right-angle rotation with bounded pivots. The
transparent fake source keeps the logical object dimensions; its transformed
rectangle and plane geometry share `lv_image_buf_get_transformed_area`. Pivot
resolution happens after window source update, so percentage pivots use current
window dimensions. The final clockwise GE angle is image angle minus display
angle, matching the SDK convention. Ancestor clipping is checked against the
transformed rectangle, and auto-alignment changes to image rotation/scale are
rejected. Image scale, arbitrary angles and partial clipping remain unsupported.

Host **49/49 PASS**, strict E907 **PASS**. Explicit expected rectangles cover a
non-central pivot at all three angles and combined image/display cancellation.
These are geometry/lifecycle tests; actual rotated alpha pixels and DMA remain
**NOT_RUN**. No new combined firmware image has been built for this increment.

## Image/pivot combined firmware evidence (2026-10-04)

Boot/app/static/image/manifest **PASS**; host **49/49 PASS**.
Clean source identities and SHA256 values:

- sdk: a6261816161fd4c6eb39b1a62387f02649f52fa4
- lvgl-aic: b35177cd70404281d0f9b3a581fbadc5500f16c6
- lvgl: 80ca777e37a2b176770726a02e07a6fb79ef0b39
- images/d13x.elf SHA256: e4059d1f7f98af5f482811e9c874e75fb449136ad893559c8eed49ccbbef4037
- images/d13x_D50T-2-Lite_page_2k_block_128k_v1.0.0.img SHA256: 0a72f308e9646cb126b67b51320bcd957a490c5a764f752b9642ee08d0677864

Board **NOT_RUN**. Rotated alpha-window pixels, scanout timing and physical
multi-decoder behavior remain unverified. D13x reference image scale/arbitrary
angle restrictions are documented in the current capabilities inventory.

Pivot boundary host follow-up: percentage center pivots are checked before and
after a paused non-square resize. A fitting original object whose rotated
window crosses the root clip, and a pivot outside the fake-render +/-4096
limit, both fault without alpha acquisition or video submission; all owners and
readers drain. Focused plane widget contract **PASS**. Runtime source is unchanged
from the latest combined firmware manifest; these tests do not prove physical
pixels. Remaining board entry/scanout validation stays **NOT_RUN**.

## Manual plane entry firmware (2026-10-04)

Added lv_aic_plane_test UI-thread mailbox using the packaged APNG RGB fixture.
Boot/app/static/image/manifest PASS; host regression 49/49 PASS.
Final ELF nm confirms command registration plus poll/deinit symbols.
Host regression does not execute the RT-Thread shell mailbox. Board NOT_RUN.

- sdk: 19658be8cd06f8f75ba224e36cb5a03455d05ab0
- lvgl-aic: 15e03afc1caaef262aa87f2a777ac3266501df5f
- lvgl: 80ca777e37a2b176770726a02e07a6fb79ef0b39
- images/d13x.elf SHA256: fbeca977b839c08d82280c26fff8d608c049d4b2d9b28b8eb426b5aa7bffca13
- images/d13x_D50T-2-Lite_page_2k_block_128k_v1.0.0.img SHA256: 1a86228bdc4f27402ec81f741c3b365713683cbff2a9bd7411b1f2efcbbe844f

Manual entry gates now require live poll/deinit and shell registration symbols
in the combined player/APNG map. The prior packaged map passes; an isolated
copy with the shell registration removed fails at the expected symbol check.
Both checker runs used the explicit development dirty-source allowance because
only checker changes were pending; no new firmware provenance is claimed.
Strict E907 compilation now includes the real RT-Thread manual command source;
compile PASS and nm confirms its registration/poll/deinit are present (not an
empty disabled-feature object). These checks do not execute the shell or board.


Media test entry now accepts a copied native absolute path (127-byte limit)
through the existing UI mailbox. RGB/YUV decoder initialization is queried
before use; the new YUV query has host lifecycle coverage. Explicit media policy
is 4 MiB CMA, three extra frames and BT.601 limited, with an independent 4 MiB
rotation budget. APNG limits stay unchanged. Host 49/49 PASS and strict E907
including the actual command source PASS; this increment has not been packaged
or run on hardware. URI parsing/RT mailbox runtime and actual media remain
NOT_RUN; see manual README for use and codec applicability.

## Media-path firmware and subsequent offset increment (2026-10-04)

Media-path entry boot/app/static/image/manifest PASS. Board NOT_RUN.
- sdk: f56c2137692d3bb4496b93580d48c98e88d72dd3
- lvgl-aic: 2e29375e121a20fa7cc84e59c0fe65bab1281e45
- lvgl: 80ca777e37a2b176770726a02e07a6fb79ef0b39
- images/d13x.elf SHA256: 1931ba32cec212f904710e9d7904cb161d16900a94b0d6b05c0438f9d1ac0ff5
- images/d13x_D50T-2-Lite_page_2k_block_128k_v1.0.0.img SHA256: 19ed325e6a4b9eb31c8d243a87fb8e388ab9863a5cf732adabfcc1517a4bd566

Subsequent source increment: image offsets now shift the transformed video
rectangle to match native same-sized fake-window placement, including rotated
images. Offset values are bounded to +/-4096; nonzero offsets with tile/auto
alignment remain rejected. Ancestor clipping still rejects partial visibility.
Focused widget contract and strict E907 PASS. This offset change is NOT in the
firmware manifest above, and pixel/board validation remains NOT_RUN.
