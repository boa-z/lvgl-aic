# Validation record

## D50T-2-Lite board boot (2026-10-06)

Image `ge2d-fonts-widgets-aicp-player-apng-ge2d-yuv-layer-final3` (SDK
`1d4f04df`, component `86dab31`, LVGL `80ca777e`; SHA-256
`79129AD21EE77E9F07934702B749FA4E151E8BF2BB52C09E3122A466E4248442`) was
flashed to D50T-2-Lite and booted. Raw serial log:
`output/lvgl-evidence/board-2026-10-06-serial.log`. The boot banner reports
Luban-Lite 1.3.2, `Image version: 1.0.0`, tinySPL `Sep 30 2026 01:05:33` and
application `Built on Oct 5 2026 05:33:21` (reset reason `Command-Reboot`).
The application banner timestamp is shared with the earlier
`yuv-mask-transformed-tiles`, `recolor-colorkey` and `ge-source-admission`
images, so the probe set, not the timestamp, remains the identifying evidence.

- Fonts: 18/28/42 px latin+CJK probes, cache busy guard/selective purge/
  identical rebuild, native metrics/bitmap/fallback/churn: **PASS** on board.
  This closes the font candidate's pending board confirmation.
- MPP + resource stage: JPEG/PNG fixtures and unsupported/corrupt inputs,
  FILE/memory parity, 100-hit cache without new CMA (PNG, BMP, AICP), BMP
  RGB555/RGB565/24/32 probes, AICP `bird.aicp` (alpha fixture SKIP: requires
  V31), 1000 decode/close cycles `current=0 alloc=free=1000`: **PASS** on board.
- GE2D fill: 12 solid-fill, 3 fake-replace, video-window alpha-zero and 84
  ARGB solid/fake probes `guards=OK`: **PASS** on board.
- Raw GE RGB565 color-key measurement: sample 0 (black `key=000000`) **PASS**,
  sample 1 (white `key=f8fcf8`, LVGL's plain-shift expansion of `0xffff`)
  **FAIL** `got=255 want=165` at `y=0 byte=0`; the engine wrote blue `0xff`,
  so its own conversion does not truncate like LVGL's. The probe stopped and
  `lv_aic_ge2d_test_run()` aborted before the GE2D counters, refresh timing, YUV
  probes and video-window test (**NOT_EVALUATED**). The probe now records the
  engine conversion per sample and tries five encodings without stopping the
  later blocks; a board re-run with the new image is **NOT_RUN**. Production
  RGB565 color key remains disabled.
- Serial-log PASS items are not panel acceptance: visible pixels, touch,
  `lv_aic_capture`, rotation/scale/canvas/widget rendering, APNG/plane/GIF shell
  gates and GE/SPI timing remain **NOT_RUN**.

## RGB565 raw color-key measurement fix (2026-10-06)

The board probe run of 2026-10-06 (`79129AD2...`) matched the black color key
`0x000000` and rejected LVGL's key for white. `lv_color16_to_color()` expands
RGB565 with plain shifts (`red << 3`, `green << 2`, `blue << 3`), so the rejected
key was `0xf8fcf8`, and the keyed pixel the engine wrote reported blue `0xff`
at `y=0 byte=0`: the engine's own conversion does not truncate. `key565_probe()`
therefore no longer assumes one expansion. Per sample it now:

- records the engine's own unkeyed RGB565->RGB888 conversion (`base=...`) and
  classifies it against the eight shift/replicate channel combos (`class=...`);
- tries the engine value, the replicating expansion, LVGL's plain-shift
  expansion and both zero-extended 16-bit placements as `ck_value`;
- reports the matching encoding per sample (`PASS key565 sample=... enc=...`)
  and only fails a sample no tested encoding explains, with every candidate and
  the first mismatching byte in the log;
- requires one common encoding across all nine samples for an overall PASS and
  continues measuring the remaining samples instead of aborting.

`lv_aic_ge2d_fill_test_run()` returns `1` when only this measurement is
inconclusive, and `lv_aic_ge2d_test_run()` keeps evaluating the YUV,
video-window, counter and refresh blocks, failing at the end instead. A hard
fill failure (wrong pixels or uncertain DMA) still aborts.

- D13x target `build.ps1 -Phase ge2d -Jobs 8 -WithFonts -WithWidgets -WithAicp
  -WithPlayer -WithApng`: **PASS** for boot, app config/build, static checks,
  image checks and manifest. The pinned evidence tag and verified image SHA256
  are recorded in the SDK application validation record
  (`application/rt-thread/lvgl-aic-smoke/VALIDATION.md`) once the SDK pin commit
  is built.
- Board pixel behavior for the five encodings: **NOT_RUN**; production RGB565
  color key stays disabled until a board run establishes the comparator.

## GE YUV LAYER adapter (2026-10-05)

The application-owned port now closes the SDK's planar YUV LAYER source gap
through an explicit adapter. `lv_aic_yuv_layer_attach()` binds immutable
I420/I422/I444/I400 planes to a child layer only after layout/capacity checks;
the evaluator additionally checks the MPP address and stride contract before
claiming the task. The executor reuses the validated YUV GE submit path, while
ordinary unbound YUV layers remain software-owned.

- `lvgl_aic_ge2d_yuv_contract`: **PASS**; attached I420 LAYER reaches GE with
  the expected MPP format and physical plane address, then releases its
  producer lease on detach.
- `lvgl_aic_ge2d_scale_contract`: **PASS**; unbound YUV layers are declined,
  attached physical frames are admitted, and stale packed layer descriptors
  remain declined.
- Full configured host CTest: **71/71 PASS**; reduced no-GE profile:
  **35/35 PASS**.
- SDK `build.ps1 -Phase ge2d -Jobs 8 -WithFonts -WithWidgets -WithAicp
  -WithPlayer -WithApng -EvidenceTag ge2d-yuv-layer-final3`: **PASS** for boot,
  app config/build, static checks, image checks and manifest. Evidence:
  `output/lvgl-evidence/ge2d-fonts-widgets-aicp-player-apng-ge2d-yuv-layer-final3`.
- Verified image SHA256:
  `79129AD21EE77E9F07934702B749FA4E151E8BF2BB52C09E3122A466E4248442`.
- Board GE pixels, cache coherency, timing and panel acceptance: **NOT_RUN**.

See [GE YUV LAYER adapter](ge-layer-yuv-stage.md).

## IMAGE recolor plus color-key composition (2026-10-05)

The application-owned IMAGE/LAYER executor now composes an A8/L8 bitmap mask,
an LVGL inclusive color key and recolor in a fixed bounded order: mask alpha,
then key transparency, then recolor of surviving pixels. Rounded clips and
non-normal blend modes still decline before allocation. The SDK reference
continues to reject recolor and bitmap masks, so this is an application
extension; physical GE/key/filter/cache/panel acceptance remains **NOT_RUN**.

- `lvgl_aic_ge2d_multipass_contract`: **PASS**; keyed pixels remain
  transparent, nonmatching pixels receive the requested recolor, and a mask →
  key → recolor IMAGE chain preserves surviving mask alpha. Source and mask
  bytes remain unchanged, and a combined LAYER descriptor reaches the GE
  executor.
- Full configured host CTest (`build\\host`): **71/71 PASS**; companion GE
  profile (`build\\host-ge`): **71/71 PASS**; reduced profile
  (`build\\host-current`): **35/35 PASS**.
- SDK `build.ps1 -Phase ge2d -Jobs 8 -WithFonts -WithWidgets -WithAicp
  -WithPlayer -WithApng -EvidenceTag mask-key-recolor-final`: **PASS** for
  boot/app config and build, static checks, image checks and manifest.
  Evidence: `output/lvgl-evidence/ge2d-fonts-widgets-aicp-player-apng-mask-key-recolor-final`.
- The manifest records SDK `f05858b8dbc0dc96384f57333995d298b37d8bb2`,
  component implementation `9fdd078864f779281cfef87203e43e626a5e9c37`, and
  LVGL `80ca777e37a2b176770726a02e07a6fb79ef0b39`. Image SHA256:
  `CE5172937107F7AFCC3D7D2D93E105959001F0500CDC633CAFBDB010EAFF246D`.
- Physical GE/key/filter/cache/panel acceptance: **NOT_RUN**.

See [bounded recolor plus color-key preparation](ge-recolor-colorkey-stage.md).

## GE transformed tiled YUV bitmap masks (2026-10-05)

The application-owned YUV A8/L8 alpha tail now follows each repeated tile's
orthogonal rotation, independent X/Y scale and pivot. It resolves overlapping
transformed cells in LVGL draw order and leaves rotation gaps untouched. The
SDK GE block still supplies only CSC/scale/rotation; the mask remains a bounded
CPU composition step after GE completes.

- `lvgl_aic_ge2d_yuv_contract`: **PASS**; opaque GE references and masked
  transformed tiles cover 1.5x horizontal scaling with 0/90 degree rotation,
  overlap ordering, rotation holes, cache handoff and source lease lifetime.
- Full configured host CTest (`build\host`): **71/71 PASS**.
- Companion GE host profile (`build\host-ge`): **71/71 PASS**; reduced combined
  profile (`build\host-current`): **35/35 PASS**.
- SDK `build.ps1 -Phase ge2d -Jobs 8 -WithFonts -WithWidgets -WithAicp
  -WithPlayer -WithApng`: **PASS** for boot/app config and build, static checks,
  image checks and manifest. Evidence:
  `output/lvgl-evidence/ge2d-fonts-widgets-aicp-player-apng-yuv-mask-transformed-final3`.
- Manifest source pins: SDK `94dfa78b17cecd7a5febb8ac9469e83b067a1004`,
  component `13d98e949b8a75eb834399d2107d233aca5b0b09`, LVGL
  `80ca777e37a2b176770726a02e07a6fb79ef0b39`. Image SHA256:
  `DFF751A9F8EA9B03460E3F2FE5D632BC19CD9456EF4615C77BB553813BE64B7E`.
- Physical GE pixels, CSC/scaler phase, cache coherency, timing and panel
  acceptance: **NOT_RUN**.

See [bounded YUV masks](ge-yuv-mask-stage.md).

## GE orthogonal rotation and scale bounds (2026-10-05)

The inverse Q16 phase calculation now floors signed source-axis terms before
adding the pivot, covering negative offsets for all four orthogonal rotations.
GE IMAGE bitblts and rotated scale crops reject source or destination axes
outside the SDK 4..4096 range before cache, allocation or submission; a real
3-pixel source is verified to reach LVGL software with no GE command.

- `lvgl_aic_ge2d_scale_contract`: **PASS**, independent signed-floor phase
  oracle for 0/90/180/270 degrees, non-integral per-axis scales, source and
  destination limits, and public-executor software fallback.
- Full configured GE host CTest: **71/71 PASS**; the existing no-GE baseline
  remains covered by its previously recorded profile.
- SDK `build.ps1 -Phase ge2d -WithWidgets -WithPlayer -Jobs 8`: **PASS** for
  boot/app config and build, static checks, image checks and manifest. Evidence:
  `output/lvgl-evidence/ge2d-widgets-player-rotation-scale-final`.
- The manifest records the clean SDK/component commits and LVGL pin
  `80ca777e37a2b176770726a02e07a6fb79ef0b39`; image SHA256
  `56f9d379a11ed0813581a6dcbb7f08ca7a8369841ebe796fb2a59268a9942ef1`.
- Physical scaler phase, cache behavior and panel output: **NOT_RUN**.

See [GE rotation and scale bounds](ge-rotation-scale-stage.md).

## Bounded YUV color-key composition (2026-10-05)

The YUV GE path now applies LVGL's inclusive RGB color-key range after CSC in
the existing straight-ARGB staging surface. Direct images and repeated tiles
clear matching pixels to transparent before the native opaque-coverage blend;
nonmatching pixels still render and the source lease/cache handoff is shared
with the existing mask path.

- `lvgl_aic_ge2d_yuv_contract`: **PASS**; an exact I420 key preserves matching
  destination pixels for direct and repeated-tile draws while an adjacent CSC
  pixel renders normally.
- Full configured host CTest: **71/71 PASS**.
- SDK `build.ps1 -Phase ge2d -WithPlayer -WithWidgets`: **PASS** for boot/app
  config and build, GE static checks, image checks and manifest. Clean final
  evidence is `output/lvgl-evidence/ge2d-widgets-player-yuv-colorkey-final`;
  SDK and component commits are recorded cleanly in its manifest, LVGL
  `80ca777e`, image SHA256
  `b3d4c2e72b1d4c42274b497e4423e858d3b41aa870e86ddd353a905c800f2841`.
- Physical CSC/filter/cache/timing/panel acceptance: **NOT_RUN**.

See [bounded YUV color keys](ge-yuv-colorkey-stage.md).

## GE YUV rotated/scaled bitmap masks (2026-10-05)

The YUV bitmap-mask tail now covers direct IMAGE draws with A8/L8 masks for
all four orthogonal rotations combined with independent X/Y scaling. At 0
degrees it preserves the GE scaler's integer Q16 source step; the rotated
paths use signed mathematical floor for the inverse phase so negative crop
offsets select the same source pixels as the GE descriptor. This was the direct
image checkpoint; the subsequent transformed-tile stage is recorded above.

- `lvgl_aic_ge2d_yuv_contract`: **PASS**; direct 0/90/180/270 rotated/scaled
  masks preserve zero-mask destination bytes and render full-mask pixels,
  alongside the existing direct, tiled and non-rotated scaled cases. A
  non-zero-pivot rotated/scaled case confirms the inverse phase remains
  relative to the pivot.
- Full configured host CTest (`build\\host-ge`): **71/71 PASS**; the no-GE
  baseline remains covered by its existing profile.
- Physical CSC/scaler/cache/panel acceptance: **NOT_RUN**.

See [bounded YUV masks](ge-yuv-mask-stage.md).

## Bounded YUV bitmap masks (2026-10-05)

The GE YUV path now handles A8/L8 masks for straight ARGB targets, including
direct orthogonal rotations, repeated unscaled tiles and non-rotated scaling.
GE writes the immutable frame through the existing CSC path, then the
application-owned staging tail multiplies source alpha before LVGL's native
blend. The staging cache is invalidated before the mask read and again after
the CPU write. The scaled case reuses GE's integer Q16 source phase;
rotated/scaled tiles decline before any CMA or GE operation.

- `lvgl_aic_ge2d_yuv_contract`: **PASS**; alternating A8 columns preserve
  masked destination bytes and full-mask columns match the raw CSC model for
  direct, orthogonally rotated, repeated tiled and 1.5x non-rotated draws;
  rotated/scaled masks decline before allocation/submission.
- Full configured host CTest: **71/71 PASS**.
- SDK `build.ps1 -Phase ge2d -WithPlayer -WithWidgets`: **PASS** for boot/app
  config and build, GE static checks, image checks and manifest. Clean final
  evidence is `output/lvgl-evidence/ge2d-widgets-player-yuv-mask-rotate-tile-final`;
  physical CSC/cache/timing/panel acceptance remains **NOT_RUN**.

The refreshed clean candidate is
`output/lvgl-evidence/ge2d-widgets-player-yuv-mask-scale-final` with SDK
`1a969c8ed0e3dfaf2b206e995e668d3d6f7041ff`; the manifest records the clean
component commit and LVGL pin,
LVGL `80ca777e37a2b176770726a02e07a6fb79ef0b39`, and image SHA256
`320d2c297c63ac663564a5368cab206cbc80352e852aea253b0833028df75400`.

See [bounded YUV masks](ge-yuv-mask-stage.md).

## GE RGB buffer layout preflight (2026-10-05)

IMAGE, LAYER, whole-display rotation and destination acceptance now share a
packed-RGB descriptor check. Short rows, zero stride and allocations shorter
than `stride * height` are rejected before cache maintenance or GE submission;
valid padded buffers remain accepted. The existing address-window and lazy
child-layer rules are unchanged.

- `lvgl_aic_ge2d_scale_contract`: **PASS**, valid padded source/destination
  descriptors plus short-row, zero-stride and one-byte-short footprint cases.
- `lvgl_aic_ge2d_display_contract`: **PASS**, the same malformed source and
  destination cases decline before whole-frame rotation submission.
- Full GE host suite after the change: **71/71 PASS**; no-GE baseline: **35/35
  PASS**.
- SDK GE2D boot/app/static/image/manifest and physical GE cache/pixel/panel
  acceptance: **NOT_RUN** for this source-only preflight increment.

See [GE buffer layout preflight](ge-buffer-layout-stage.md).

## GE IMAGE color-key preparation (2026-10-04)

The application-owned IMAGE executor now stages LVGL inclusive color-key
ranges, RGB565 sources and filtered/scaled/rotated keys into straight ARGB8888
before GE composition. Simple RGB888/XRGB8888 single-value keys retain the
SDK comparator; recolor+key and YUV keys remain software.

- `lvgl_aic_ge2d_scale_contract`: **PASS**; range/transform acceptance,
  direct RGB565 safety decline and RGB565 range-to-alpha conversion.
- Full GE host CTest: **71/71 PASS**.
- No-GE host CTest: **35/35 PASS**.
- SDK `build.ps1 -Phase ge2d -Jobs 8 -AllowComponentDirty`: **PASS** for
  boot/app config and build, static checks, image checks and manifest.
- SDK commit `a9de7309640f51917872e76ab2e21b6b3f23cf6b`, component commit
  `d9e419d10f2a29efb8a695ffa930e5f1ea86a7bf`, LVGL
  `80ca777e37a2b176770726a02e07a6fb79ef0b39`; image SHA256
  `3782555f1e02a72046ac7c3acfd91afe6a7e535494c0b1f6a37a8037df919655`.
- Physical GE key pixels, cache behavior and panel output: **NOT_RUN**.

See [GE color-key stage](ge-colorkey-stage.md).

## GE2D linear FILL gradients (2026-10-04)

The application-owned FILL executor now accepts two-stop horizontal/vertical
`LV_GRAD_EXTEND_PAD` gradients with bounded axes up to 256 pixels. Perpendicular
and along-axis clips sample endpoint colors from LVGL's stop calculator so the
full-task ramp is preserved; complex gradients and translucent stops use LVGL
software fallback.

- `lvgl_aic_ge2d_fill_contract`: **PASS**, 13,824 existing ARGB scenes plus
  66 gradient pixel scenes, 70,778,880 channel/guard comparisons, maximum
  observed gradient error 2; 1..256 ramp lengths, perpendicular/along-axis
  clips, fallback and fault lifetime paths.
- Full GE host CTest: **71/71 PASS**.
- SDK `build.ps1 -Phase ge2d -AllowComponentDirty`: **PASS** for boot config,
  build, assets, app config/build, static checks, image checks and manifest.
- Physical GE gradient pixels, cache behavior and panel output: **NOT_RUN**.


## Video window image composition and extents (2026-10-04)

The application-owned video window now has a real LVGL redraw contract. The
alpha-zero `.fake` metadata reports an alpha-capable format when blended and an
opaque XRGB format when replacement is fully opaque. A hash-guarded LVGL image
widget correction preserves inclusive right/bottom bounds for rotated images.

- Full vector host profile **93/93 PASS**; the dedicated video-window contract
  performs 171 refreshes across GE unavailable, heap fallback and GE descriptor
  fill models. Existing contracts remain green.
- Pixel checks cover repeated redraw, movement, resize, hide/show, clipping,
  ARGB background restoration, source cover semantics and 0/90/180/270-degree
  16x8 and 8x16 bounds. The target manual runner adds twelve offscreen probes.
- Full D13x boot/app, static-link, image and manifest checks **PASS**. The
  evidence directory is `output/lvgl-evidence/ge2d-fonts-gif-widgets-aicp-player-apng-barcode-spi-demos-music-vector-svg-lottie-rotate90-video-window2`.
  The 32-file manifest has clean SDK/component/LVGL sources; hardware DE/panel
  acceptance remains **NOT_RUN**.
- Sources: SDK `63b64ebcefc6f50ad16ac9143531f6c17a3fc181`, component
  `d8e2373a1870b41a84fa462ab7cd9d2d84502af3`, LVGL
  `80ca777e37a2b176770726a02e07a6fb79ef0b39`.
- Image SHA256 `b33c08cab689573fcafa394cc919ccf3698267e01819f3c853967d2647af7efa`;
  ELF SHA256 `55b0592f305f50ec039772d24bf2fd96d7b43f3a59eeb85c6e810467c897656c`.
  Generated widget source SHA256 `2a687b0897964987d00c80efea5475d7137c72c817341695144a8ade373e524e`.
- The final ELF contains `lv_aic_video_window_test_run`,
  `lv_aic_video_window_create`, `lv_image_create`, `lv_aic_fake_image_parse`
  and `lv_draw_aic_ge2d_image`. Independent manifest/hash and live-symbol
  checks are recorded in `output/video-window-independent.log`.

## Native SVG group and element opacity (2026-10-04)

Roots, groups, use references, geometric leaves and outline text/spans now apply
opacity after their contents overlap. Explicit inherit follows the immediate
parent's computed opacity; the default remains non-inherited. Mixed vector/layer
intermediates use straight ARGB to avoid double interpretation of premultiplied
pixels. Cumulative isolation pixels are limited to 4 MiB by default; over-budget
or allocation-failed objects are skipped while siblings continue. See
[scope, limits and failure policy](svg-opacity-stage.md).

- Combined host **92/92 PASS** (47.95 s), disabled baseline **81/81 PASS**
  (22.30 s), minimal SVG without FreeType **8/8 PASS** (3.79 s).
  Final image-provider refresh: vector/SVG **8/8 PASS** (1.50 s), minimal
  **8/8 PASS** (1.36 s).
- 22 shared analytic fixtures / **440 direct and decoded-image scenes**;
  the actual **44 board probes** also pass **440 host canvas dispatch scenes**.
  Includes overlap, nested opacity, explicit/default inheritance, endpoints,
  order, transformed groups/uses, fill opacity and root viewBox isolation.
- Additional alpha-target/outer-image checks, zero/half/full text span opacity
  with following glyph positions, image-provider attribute order, **30 injected
  layer/buffer/descriptor failures**, 256-layer cumulative budget boundary,
  group-depth rejection, independent-render recovery and existing reference/
  dash lifetimes pass. Native layer lists/memory and tracked LVGL heap balance.
- Full D13x boot/app, final-link/static, image and clean-source manifest **PASS**.
  **31 artifact hashes**, all three clean source pins and SDK gitlink independently
  checked. Live SVG renderer, opacity scope and board probe symbols plus allocated
  new case/budget/depth markers verified. Hardware **NOT_RUN**; no flashing.
- Implementation: `7aa1810a5ed6fe6af2ebde3c8430ac9407c49b87` (all stage fixes folded in).
- Clean SDK build: `e9cb4367e89c28e6072604062705eacd2f9d05a9`, retained at
  `refs/lvgl-evidence/svg-opacity`. SDK gitlink commits stay local.
- LVGL: `80ca777e37a2b176770726a02e07a6fb79ef0b39`.
- Evidence: SDK `output/lvgl-evidence/ge2d-fonts-gif-widgets-aicp-player-apng-barcode-spi-demos-music-vector-svg-lottie-rotate90-svg-opacity`.
- Image: `images/d13x_D50T-2-Lite_page_2k_block_128k_v1.0.0.img`,
  SHA256 `eb4679856513c1c62c8c6add6fe0c8baf8de9c0be3575e767f97e022f47ff2ca`.
- ELF SHA256: `8255ec0e7fa62931aac89924859b4a4c80ea8ad9d88a4fa145d72df38f91c882`.
- Generated SVG renderer SHA256:
  `cdad4e7100bbc6f19d1adbb19f15ca4ed4fbdf1ae0999ba2b42d5062133c9fd6`.
- Generated SVG image draw SHA256:
  `a33c1975870683ed7344818a9a75af41b671a8efaf679a3a0adeeaabbc806f0b`.
- Logs: component `output/svg-opacity-{before,build,focused,vector-build,vector-tests,ge-build,ge-tests,svg-minimal-build,svg-minimal-tests,final-build,final-tests,minimal-final-build,minimal-final-tests,firmware,independent}.log`.
- Full SVG gradients/paint inheritance, nested viewports, typography, renderer-
  internal allocation failures and physical memory/cache/performance remain open.
  This software SVG extension is not an omitted SDK GE hardware operation.

## Native vector geometry coverage and remaining modes (2026-10-04)

SRC_IN, DST_IN and NONE now use independent A8 geometry coverage so transparent
paint clears covered pixels while holes and outside pixels remain unchanged.
Pattern coverage follows the same image sampler, including transformed extents.
Pinned ThorVG push/clip failure ownership is honored. All nine native vector
enum values have numerical coverage in the documented scope; this does not
claim GE acceleration or full SVG group/document semantics.
See [coverage formulas, budget and failure ownership](vector-coverage-stage.md).

- Combined host **91/91 PASS** (45.71 s), disabled baseline **81/81 PASS**
  (21.97 s), minimal SVG **7/7 PASS** (3.25 s). After adding fill/stroke overlap,
  refreshed vector/SVG **7/7 PASS** (1.66 s) and minimal **7/7 PASS** (1.45 s).
- 7,084 rectangular scenes, 972 geometric renders (324 source/coverage/operator
  triples), **11,556,400** channel comparisons, **1,083** injected failures and
  13 invalid/empty/budget cases. Includes nine modes, seven target encodings,
  zero task opacity, split scissors, ordered subtasks, transparent paint/holes,
  strokes, fill/stroke overlap and transformed image coverage.
- The actual **217 board probes** pass 2,170 host canvas/task scenes with balanced
  native layer lists/memory. The firmware links all probes, including exact
  opaque erasure and transparent-fill/hole preservation. Hardware NOT_RUN.
- Full D13x boot/app, final-link/static, image and clean-source manifest **PASS**.
  All 31 file hashes, three clean source pins and SDK gitlink independently
  verified. Live renderer/probe symbols and allocated 217-probe/hole/failure
  markers verified. Nine image CRCs and 41 fixture/provenance hashes pass.
- Implementation: `88dc75b272b2b364c112fe13f2de82a3f7238c3e` (stage fixes folded in).
- Clean SDK build: `072c1a5bd5e05a1de8f76ee9bc46357e0d28686a`,
  retained locally at `refs/lvgl-evidence/vector-coverage`.
- LVGL: `80ca777e37a2b176770726a02e07a6fb79ef0b39`.
- Evidence: SDK `output/lvgl-evidence/ge2d-fonts-gif-widgets-aicp-player-apng-barcode-spi-demos-music-vector-svg-lottie-rotate90-vector-coverage`.
- Image: `images/d13x_D50T-2-Lite_page_2k_block_128k_v1.0.0.img`,
  SHA256 `b3b597e8005bc46682289954b8c9db1ea8a31ff50d7a5d7cd664a755ca77cce3`.
- ELF SHA256: `85cc1606a8fdb45794fe73589071c45b870391d32b0c9947452110964682d030`.
- Generated vector SHA256: `020de71de3345e853f0b65372d2a30801f27e425c1a8cc87994b5dedc5cf161c`.
- Logs: component `output/vector-coverage-{before,build,focused,tests,baseline-build,baseline-tests,minimal-build,minimal-tests,final-build,final-tests,minimal-final-build,minimal-final-tests,firmware,independent}.log`.
- General SVG group/document semantics, renderer-internal allocation failures,
  physical pixels/memory pressure and performance remain open. Hardware
  **NOT_RUN**; no flashing. SDK commits remain local.

## Native vector destination-over, additive and erasure (2026-10-04)

DST_OVER, ADDITIVE and vector SUBTRACTIVE now use explicit premultiplied
composition. Subtractive follows LVGL's VG-Lite destination-out mapping, not
image RGB subtraction. Opacity products preserve the 255 endpoint; alpha-less
targets retain reduced-alpha RGB against black. This is software vector
correctness. See [formulas, coverage and open modes](vector-operators-stage.md).

- Combined host **91/91 PASS** (46.39 s); disabled baseline **81/81 PASS**
  (21.72 s); minimal SVG without FreeType **7/7 PASS** (2.95 s).
- 4,543 rectangular scenes plus 192 geometric renders (96 source/operator
  pairs), **6,953,308 channel comparisons**, 462 injected allocation/canvas
  failures and nine invalid/budget cases. Six modes, seven target encodings,
  split scissors, opacity products and ordered subtasks pass. Geometric cases
  add fractional triangles, rounded holes, alpha/color gradients and patterns
  containing transparent pixels. Opaque erasure requires exact zero.
- Actual board-probe source passes 1,330 host scenes over ten lifetimes with
  balanced native layer lists/memory. **133** probes, including seven exact
  opaque erasures, are linked in the target. Hardware execution remains NOT_RUN.
- Full D13x boot/app, final-link/static, image and clean-source manifest **PASS**.
  All **31** hashes, three clean source pins and SDK gitlink independently
  checked; live renderer/probe symbols and allocated probe/erase markers verified.
  Nine image CRCs and 41 fixture/provenance hashes pass.
- Implementation: `69c2871da0f7025b6d3a3ddac1f4026c0f2ea624` (all stage fixes folded in).
- Clean SDK build: `05cc7b58632e72934d2619cd45a8fb22ace62af1`,
  retained locally at `refs/lvgl-evidence/vector-operators`.
- LVGL: `80ca777e37a2b176770726a02e07a6fb79ef0b39`.
- Evidence: SDK `output/lvgl-evidence/ge2d-fonts-gif-widgets-aicp-player-apng-barcode-spi-demos-music-vector-svg-lottie-rotate90-vector-operators`.
- Image: `images/d13x_D50T-2-Lite_page_2k_block_128k_v1.0.0.img`,
  SHA256 `d90138d28f6e48c75deb3a7e35a290d31aad6412c1eda02343603d86e264321b`.
- ELF SHA256: `b5db9bfe48a7ae50878cba096d547f18059340219e1d359343f6b17bfb0306f3`.
- Generated vector SHA256: `b63f31b5a6e49b73c1d6853c5e71cbdfaa9535231c461928f00ffd239d2c6ada`.
- Logs: component `output/vector-operators-{before,build,focused,tests,baseline-build,baseline-tests,minimal-build,minimal-tests,firmware,independent}.log`.
- At this historical pin SRC_IN/DST_IN remained source-over fallbacks and
  replacement coverage was unverified; the later coverage stage above resolves
  those modes. SVG group/document semantics, internal allocation failures and
  target performance remain open. Physical validation **NOT_RUN**. No flashing;
  SDK commits remain local.

## Native vector translucent blends and paint opacity (2026-10-04)

Multiply/screen now preserve source and backdrop alpha; gradient fill/stroke and
image-pattern opacity participate. Two clipped surfaces share the configured
budget, and checked failures preserve the original target. This corrects native
software rendering without adding GE vector acceleration.
See [scope and remaining blend/document limits](vector-blend-stage.md).

- Combined host **91/91 PASS** (44.18 s); disabled baseline **81/81 PASS**
  (21.34 s); minimal SVG without FreeType **7/7 PASS** (2.73 s).
- Real ThorVG: 2,254 analytic scenes, 3,254,776 channel comparisons, 210 injected
  allocation/canvas failures and nine invalid/budget cases. All seven target
  encodings, paint/task opacity, five additional paint styles, split scissors
  and normal/special/normal ordering are covered.
- Actual board-probe source runs 630 host scenes over ten lifetimes with
  balanced native layer lists/memory. All **63** solid/linear/radial probes are
  linked in target firmware; they have not run on hardware.
- Full D13x boot/app, final-link/static, image and clean-source manifest **PASS**.
  All 31 artifact hashes, three clean source pins and SDK gitlink independently
  verified. The generated renderer and probe runner are live in ELF; the
  63-probe and failure markers reside in allocated `.rodata`.
  Nine image payload CRCs and 41 fixture/provenance hashes pass.
- Implementation: `69ff9a7810cfaf4ae5e0de197ceed2ef991f9f7b` (all stage fixes folded in).
- Clean SDK build: `333a0b951ddb0d497041ff670e059bd1b10d959b`,
  retained locally at `refs/lvgl-evidence/vector-blend`.
- LVGL: `80ca777e37a2b176770726a02e07a6fb79ef0b39`.
- Evidence: SDK `output/lvgl-evidence/ge2d-fonts-gif-widgets-aicp-player-apng-barcode-spi-demos-music-vector-svg-lottie-rotate90-vector-blend`.
- Image: `images/d13x_D50T-2-Lite_page_2k_block_128k_v1.0.0.img`,
  SHA256 `467a5c9004bbe86bbaab6b953db6e60a5389c0b9533e5bbfdb90ef3210ac1555`.
- ELF SHA256: `8df4c9207247d4e97ef076e9d9797db6e9047feba1471f51454eae84c933feaa`.
- Generated vector SHA256: `e3af59aa1000a5121831765163437d98ac9ad1496d10ce20ff30967e60bc42a5`.
- Logs: component `output/vector-blend-{build,focused,tests,baseline-build,baseline-tests,minimal-build,minimal-tests,firmware,independent}.log`.
- The original translucent multiply failure is now a passing regression.
  Other vector blend modes, general SVG document semantics, target memory
  pressure and throughput remain open. Physical validation: **NOT_RUN**.
  No flashing; no SDK publication.

## Native vector target alpha and clipping (2026-10-04)

Native vector drawing now stages a clipped premultiplied surface seeded from the
actual target and restores the original target representation. Per-subtask
scissors stay independent and checked allocation/canvas failures preserve the
original pixels. This corrects canvas/native vector behavior; it does not add
GE vector acceleration. See [formats, ownership and remaining limits](vector-surface-stage.md).

- Combined host **91/91 PASS** (43.59 s); disabled baseline **81/81 PASS** (21.81 s).
  Refreshed vector/SVG **7/7 PASS** (0.34 s); minimal SVG without FreeType
  **7/7 PASS** (2.60 s).
- 182 real-raster analytic scenes, 262,808 channel comparisons, seven buffer
  allocation failures, 35 checked canvas failures and eight rejected views.
  Seven compiled board probes also run through native canvas/task dispatch on
  host over ten lifetimes (70 scenes), with balanced layer lists/memory.
- Full D13x boot/app, final-link/static, packaged-image and clean-source manifest
  **PASS**. All 31 artifact hashes, three clean source pins and SDK gitlink were
  independently verified. The generated renderer owns the live final symbol;
  probe and failure markers reside in allocated ELF `.rodata`. Nine image CRCs
  and 41 packaged fixture/provenance hashes pass.
- Implementation: `0d65f3eedd5ea694bc93a2959ff7717e9e196199`.
- Clean SDK build pin: `4a50366dedc29b1d47a56519e57fe99a85a582a2`, retained at
  local `refs/lvgl-evidence/vector-surface`.
- LVGL pin: `80ca777e37a2b176770726a02e07a6fb79ef0b39`.
- Evidence: SDK `output/lvgl-evidence/ge2d-fonts-gif-widgets-aicp-player-apng-barcode-spi-demos-music-vector-svg-lottie-rotate90-vector-surface`.
- Image: `images/d13x_D50T-2-Lite_page_2k_block_128k_v1.0.0.img`, SHA256
  `91dd4b54294a9565ef771b2cbbcbdb49cf6a58bd4e0337e6bdce96222d4cf800`.
- ELF SHA256: `2e2bb74c10f493142a88e7bedfa8d12c53269c54a0b0f035c1a07ea389403975`.
- Generated vector SHA256: `6a922d8ba853af3e6989ed7969e54d0c9551097d3fd9ce185c4aff661d79d24a`.
- Logs: component `output/vector-surface-{build,tests,baseline-build,baseline-tests,final-build,final-tests,minimal-build,minimal-tests,firmware,independent}.log`.
- Original outer-clip failure: `output/vector-surface-before.log`. The explicit
  `translucent-blend` reproducer was a known **FAIL** at this historical pin;
  log `output/vector-translucent-blend-gap.log`. The later blend stage above
  fixes it and includes translucent cases in default coverage.
- Physical pixels/cache, target memory pressure and performance: **NOT_RUN**.
  No flashing occurred. Serial subtask rasterization adds work; throughput is not
  inferred from correctness gates. General SVG group/gradient/document semantics
  remain open; translucent multiply/screen arithmetic was corrected later.

## Bounded native IMAGE masks before GE (2026-10-04)

The application-owned GE path now accepts ordinary decoded RGB/ARGB IMAGE
bitmap masks. It converts RGB565, RGB888, XRGB8888, straight ARGB8888 and
premultiplied/flagged ARGB8888 sources into a bounded private ARGB buffer,
applies the reviewed A8/L8 mask kernel, then reuses the existing GE transform,
alpha-target and quarantine paths. The ArtInChip SDK reference still rejects
bitmap masks; this is an application extension. At this historical stage,
scaled and rotated-tile YUV masks remained software fallback; the later direct
rotated/scaled extension is recorded at the top of this file.

- GE host build and complete CTest matrix: **76/76 PASS** (33.27 s), including
  581 mask scenes in the focused multipass contract: 576 LAYER scenes plus five
  decoded IMAGE source formats. Source and mask immutability, software parity,
  split refresh and existing GE failure handling pass.
- Full D13x boot/app, static/link checks, packaged image and clean manifest:
  **PASS**. `check_integration.py --phase ge2d` with fonts/GIF/widgets/player/
  APNG/barcode/SPI/demos/music/vector/SVG/Lottie/AICP and rotation 90 reports
  all live-symbol and GE static gates **PASS**.
- The manual GE2D scale runner now includes ten ordinary IMAGE mask probes
  (five decoded source formats × opaque/straight-alpha targets), in addition to
  the existing 36 LAYER mask probes. These probes require ENGINE output, source
  immutability and clip guards; physical execution remains **NOT_RUN**.
- SDK evidence:
  `output/lvgl-evidence/ge2d-fonts-gif-widgets-aicp-player-apng-barcode-spi-demos-music-vector-svg-lottie-rotate90-image-mask3`.
  Image SHA256:
  `10253e92690eb7955b3b157e83d1dedc0fac8ee6cf0e2f284f0cc29fb879a26f`;
  ELF SHA256:
  `be5834e63033f189afc2619ab5c1409008a82edb2d3c7aa5d58d9667c847f567`;
  generated `lvgl-sw-image.c` SHA256:
  `7eb6e7b86ddfd71fc24c108e5556a622c9c63e011cfc4ea6d2557a667900fac6`.
  Manifest contains 32 hashed artifacts and records SDK gitlink `b389086f`,
  component `38e3bf4`, and LVGL `80ca777e`.
- Physical mask pixels/cache coherency, DMA behavior, panel composition and
  throughput: **NOT_RUN**. No flashing occurred in this phase.

## Tiled IMAGE bitmap masks (2026-10-04)

The bounded IMAGE A8/L8 mask path now accepts repeated `tile` draws. A focused
GE executor contract covers the default tile origin, one source-sized ARGB
staging copy, 128-level A8 coverage, two-pass preflight and six submissions for
three columns by two rows. The source remains unchanged and staging is released
after the synchronous run.

- `lvgl_aic_ge2d_scale_contract`: **PASS** after the tiled mask increment;
  the complete GE contract includes the existing transform, address, alpha,
  decoder lifetime and DMA fault cases.
- Physical tiled mask pixels, cache coherency, GE filtering and panel output:
  **NOT_RUN**. No flashing occurred in this phase.
- Implementation is in the component branch `codex/sdk-basic-capabilities`;
  see [tiled IMAGE masks](ge-image-mask-tile-stage.md).

## Bounded native layer masks before GE (2026-10-04)

ARGB IMAGE and LAYER tasks now stage native A8/L8 masks on a private CMA source
copy before GE transformations/composition. Ordinary IMAGE sources are first
converted to a bounded private ARGB buffer; the accepted GE path keeps the
original source unchanged across partial refresh. This extends the SDK baseline,
whose GE path rejects masks; scaled and rotated-tile YUV masks remain software
while the bounded direct-orthogonal and unscaled-tiled straight-ARGB YUV path is
documented separately. See
[formats, memory and fallback boundaries](ge-layer-mask-stage.md).

- Combined FreeType/vector/SVG/Lottie/manual-preview host **89/89 PASS** (46.42 s).
  Disabled baseline **81/81 PASS** (33.08 s). Additional unsupported-effect
  assertions pass refreshed focused contracts in both profiles. After the target
  compiler's possible-uninitialized warning was fixed, all ten affected GE and
  premultiplied host contracts pass again (**10/10**, 4.27 s).
- 576 native/model LAYER scenes, 4,855,032 channel comparisons, plus five
  decoded IMAGE source-format scenes, maximum layer error 7;
  transformed comparison is limited to smooth interiors. Exact split refresh,
  immutable source/mask, six preparation failures, nine DMA fault positions,
  three effect software boundaries, IMAGE format staging and malformed-mask
  policy pass.
- Full D13x boot/app, final-link/static, packaged image and clean-source manifest
  **PASS**. Thirty file hashes, three clean source pins and SDK gitlink were
  independently verified. Mask bridge and scale probe runner are live in ELF;
  new pass/fault strings reside in allocated `.rodata`. Nine image payload CRCs
  and 41 packaged fixture/provenance hashes pass.
- Thirty-six numerical board probes compiled/linked: three ARGB encodings,
  A8/L8, native/scale/arbitrary-plus-scale, opaque/alpha targets. They require
  ENGINE output, preserve source and clip guards, and log `PASS mask`.
- Implementation: `c0f31c03bee2f64ea796186eec6260cdb75d1e47` (stage fixes folded in).
- Clean SDK build pin: `1adff892d6ace29d4729c1b725a10115ecf5f3c7`, retained at
  local `refs/lvgl-evidence/ge-layer-mask`.
- LVGL pin: `80ca777e37a2b176770726a02e07a6fb79ef0b39`.
- Evidence: SDK `output/lvgl-evidence/ge2d-fonts-gif-widgets-aicp-player-apng-barcode-spi-demos-music-vector-svg-lottie-rotate90-ge-layer-mask`.
- Image: `images/d13x_D50T-2-Lite_page_2k_block_128k_v1.0.0.img`, SHA256
  `5bc888697bec4e3ed651a43e85507bca519a9f7d6fdae850e44573bce480dbc7`.
- ELF SHA256: `98dbb23587b179d3370834fc5426dcfee22fdbd4021429e40b86bf1efe9197dc`.
- Logs: component `output/ge-mask-{build,tests,baseline-build,baseline-tests,focused,baseline-focused,final-build,final-tests,firmware,independent}.log`.
- Physical pixels/cache/DMA/performance: **NOT_RUN**. No flashing occurred.

## Image roller pointer ownership and continuous snap (2026-10-04)

Pointer drags supersede an old no-op selection; loop origin changes rebase the
native scrolling animation instead of canceling it. Synchronous selection stops
traversal after a VALUE_CHANGED callback may delete the widget. The manual page
now has overflowing equal-sized cards and a real click-then-drag regression.
See [behavior and boundaries](image-roller-input-stage.md).

- Combined FreeType/vector/SVG/Lottie plus manual-preview host **89/89 PASS**
  (45.35 s); disabled baseline **81/81 PASS** (26.66 s).
- 48 drags after no-op selection, four drags taking over an in-flight selection,
  40 consecutive loop drags and 80 explicit selections. All 172 scenes check
  centered active IDs and exact RGB565 source colors. Unrelated owner animation
  values remain intact. Eight callback deletion cases verify no post-delete
  child traversal. Previous failures were reproduced separately in focused logs.
- The real manual page passes three lifecycle/pointer sequences; its 800x480
  rendered preview was visually inspected (`output/roller-input-preview.png`).
- Full D13x boot/app, final-link/static, packaged image and clean-source manifest
  **PASS**. Thirty artifact hashes and three source pins independently verified.
  New page text is in allocated ELF `.rodata`; nine image payload CRCs and 41
  packaged fixture/provenance hashes pass.
- Implementation: `975ccb496f96911a5de00d4e4d4e3b4472fa4183`.
- Clean build SDK pin: `1c5e75fbf4e0dc5d59ad8af36abbc25e927ad8de`, preserved at
  local `refs/lvgl-evidence/roller-input`.
- LVGL pin: `80ca777e37a2b176770726a02e07a6fb79ef0b39`.
- Evidence: SDK `output/lvgl-evidence/ge2d-fonts-gif-widgets-aicp-player-apng-barcode-spi-demos-music-vector-svg-lottie-rotate90-roller-input`.
- Image: `images/d13x_D50T-2-Lite_page_2k_block_128k_v1.0.0.img`, SHA256
  `fdeda9e8f38bfa7c401568c898a96c8b5f4e6ed9edcfca0a7befe53ad9959d95`.
- ELF SHA256: `174bcf599b4d4c3e951ccc7a7c35d75442ae005981795ddec0aaad5f3293f310`.
- Logs: component `output/roller-input-{build,tests,baseline-build,baseline-tests,firmware}.log`;
  reproductions `output/roller-input-before.log`, `output/roller-snap-before.log`,
  `output/roller-delete-before.log`.
- Physical input/pixels, touch feel, GE/cache and rendering speed: **NOT_RUN**.
  No hardware acceptance is inferred from host or firmware success.

## SDK widget child ownership and loop continuity (2026-10-04)

Swipe unregisters reparented images, removes only its own event callback,
releases source tables and cancels the original transition. Replacement from
begin/end callbacks completes exactly once. Image roller loop compensation now
includes the flex gap, preserving retained images' screen positions. See
[scope, ownership and test details](widget-lifecycle-stage.md).

- Combined FreeType/vector/SVG/Lottie host **88/88 PASS** (22.45 s).
- Disabled baseline host **80/80 PASS** (20.88 s).
- Eighty detach lifetimes, twenty callback replacements, cross-widget/return
  behavior, 160 real pointer clicks and 240 exact RGB565 resource pixel checks.
  Five complete LVGL lifetimes return tracked LVGL heap bytes to zero.
- Twelve loop-wrap scenes across both directions/ends and gaps 0/7/19 preserve
  48 retained-child screen positions. Both original bugs failed before correction.
- Full D13x boot/app, final-link/static, image and clean-source manifest **PASS**.
  Thirty artifact hashes and three clean source pins independently verified.
  New swipe event code is live in the final allocated map. Nine image payload
  CRCs and 41 packaged fixture/provenance hashes pass.
- Implementation: `f76c539be1afbb8c90da52c930f9cf9feadaf9a4`.
- Clean build SDK pin: `1a963aa9dd9ad2afca17565c8eda45b9ea9bbaa4`, retained at
  local `refs/lvgl-evidence/widget-lifecycle`.
- LVGL: `80ca777e37a2b176770726a02e07a6fb79ef0b39`.
- Evidence: SDK `output/lvgl-evidence/ge2d-fonts-gif-widgets-aicp-player-apng-barcode-spi-demos-music-vector-svg-lottie-rotate90-widget-lifecycle`.
- Image: `images/d13x_D50T-2-Lite_page_2k_block_128k_v1.0.0.img`, SHA256
  `6a5379d4435672440826ad7f93bb0ef173893e7d753bafc6fb7656e42589a23b`.
- ELF SHA256: `8af1f27082ffe1caf2bc568ebf683afb4d97682859198157c4c8c3f759d79167`.
- Logs: component `output/widget-lifecycle-{build,tests,baseline-build,baseline-tests,firmware}.log`;
  original failures: `output/swipe-ownership-before.log`, `output/roller-gap-before.log`.
- Physical input/pixels, target heap, DMA/cache and performance: **NOT_RUN**.
  Host/model and firmware success do not establish board acceptance.

## SVG document references and grouped stroke ownership (2026-10-04)

Native SVG now restores missing-image transforms, positions nested `<use>`
references before subtree transforms, preserves explicit referenced colors over
inherited use paint, and frees owned dash arrays at builder/group/span exits.
Missing href/ID, active reference cycles and chains beyond 32 active uses skip
the affected branch. See [scope and lifecycle](svg-documents-stage.md).

- Combined FreeType/vector/SVG/Lottie host **87/87 PASS** (43.30 s).
- Disabled baseline **79/79 PASS** (20.94 s).
- Minimal SVG-without-FreeType build: **4/4 SVG contracts PASS** (1.69 s).
- Final refreshed host build: affected **7/7 contracts PASS** (1.73 s).
- 200 direct/decoded-image scenes with analytic samples, independent reference
  bounds traversal, missing images/references, explicit/inherited colors,
  nested/scaled/rotated uses and direct/mutual cycles. Reference-chain lengths
  31/32/33/41 check the exact 32-use boundary and subsequent-render recovery.
- One hundred grouped dashed-path lifetimes and twenty FreeType outline-span
  lifetimes leave tracked LVGL heap unchanged; all tracked allocations return
  to zero after teardown. This excludes ThorVG's C++ heap and broad OOM coverage.
- Four pre-correction reproductions failed: missing-image placement, scaled-use
  placement, explicit referenced color and grouped-dash heap growth. Logs are
  `output/svg-document-before-*.log`.
- Full D13x boot/app, final-link/static, image and clean-source manifest **PASS**.
  The generated SVG renderer owns the final live symbol. All 30 file hashes and
  three source pins were independently verified; probe markers are in allocated
  ELF `.rodata`. Twenty additional offscreen board probes compiled/linked.
- Physical pixels, target memory stress, cache/CMA and timing **NOT_RUN**. This
  corrects native SVG semantics; it does not add GE vector rasterization.

Clean build identities (documentation commits are not build pins):

- SDK: `4b2d17b5eabf978fa66574b75dbf74fe08b7a783`.
- lvgl-aic: `595bbe96e9388c48014a1e9ba64eacea738c8c92`.
- LVGL: `80ca777e37a2b176770726a02e07a6fb79ef0b39`.
- Image SHA256: `a5245480d0f0e861429efe9162b38303b9a684783fc55af49f02520cb002f019`.
- ELF SHA256: `243862b2ca4df7da2b6ab6e229c894cc1beb6b1ffa00651eb906f91dda180468`.
- Generated renderer SHA256: `59746e1695ad4f9c7103f053844c2a32773bb838d985f9aaf213057f789e4dc5`.
- Manifest: SDK `output/lvgl-evidence/ge2d-fonts-gif-widgets-aicp-player-apng-barcode-spi-demos-music-vector-svg-lottie-rotate90-svg-documents/manifest.json`.
- Component logs: `output/svg-document-build.log`, `output/svg-document-tests.log`,
  `output/svg-document-baseline-build.log`, `output/svg-document-baseline-tests.log`,
  `output/svg-document-minimal-{config,build,tests}.log`,
  `output/svg-document-final-focused.log`, `output/svg-document-firmware.log`.

The SDK build pin is retained at `refs/lvgl-evidence/svg-documents` before
amending its local gitlink to the evidence documentation commit. The four fixes,
reference guards and tests are consolidated in one correction commit, followed
by this evidence commit. Only the component branch is pushed with boa-z
credentials. SDK/core source checkouts remain untouched; no board was flashed.

## Straight ARGB solid and pseudo-fill composition (2026-10-04)

Partial solid FILL and SDK blended `.fake` rectangles now run bounded opaque GE
fill followed by native XRGB composition. The existing 2 MiB alpha budget is
reused; original target bytes remain untouched through GE completion. XRGB
preserves native low-opacity thresholds without a second alpha quantization.
See [execution, fallback and ownership](ge-fill-argb-stage.md).

- Combined FreeType/vector/SVG/Lottie host **86/86 PASS** (21.80 s).
- Disabled baseline **79/79 PASS** (20.54 s).
- 13,824 scenes cover all 256 global opacities, six destination alphas, three
  colors and three clips. All 70,778,880 channel/guard byte comparisons match
  native solid fill exactly (maximum error 0); full/split refresh is exact.
- Real `.fake` image dispatch adds 24 exact-oracle scenes. Budget/allocation/
  address failures preserve pre-DMA fallback; invalid metadata/flags are rejected.
  Scheduler and fake submit/emit/sync failures retain scratch/task, leave target
  unchanged, latch shared fault and prevent replay. RGB/YUV regression passes.
- Full D13x boot/app, final-link/static, image and clean-source manifest **PASS**;
  all 29 file hashes and three source pins independently verified. Both new
  probe markers were verified inside allocated ELF `.rodata`.
- 84 additional offscreen probes compiled/linked, including ordinary and pseudo
  fills, near-transparent/opaque thresholds, all RGBA/guard bytes and immediate
  target invalidation after the CPU tail. Physical execution **NOT_RUN**.

Clean build identities (documentation commits are not build pins):

- SDK: `cd832cd85c6e918271664267a2206bc4c19558c6`.
- lvgl-aic: `eb58d3ac4340bdade6a0eb442669f4427bb5070a`.
- LVGL: `80ca777e37a2b176770726a02e07a6fb79ef0b39`.
- Image SHA256: `72ac664a2799e23502b175e8b9c53659a80c28030f9c6556bf511e46c260f494`.
- ELF SHA256: `cd48359323d2df55d97ac6333913f36848efa718de0374a209561086f696d458`.
- Manifest: SDK `output/lvgl-evidence/ge2d-fonts-gif-widgets-aicp-player-apng-barcode-spi-demos-music-vector-svg-lottie-rotate90-ge-fill-argb/manifest.json`.
- Component logs: `output/ge-fill-alpha-build.log`, `output/ge-fill-alpha-tests.log`,
  `output/ge-fill-alpha-baseline-build.log`, `output/ge-fill-alpha-baseline-tests.log`,
  `output/ge-fill-alpha-firmware.log`.

The SDK build pin is retained at `refs/lvgl-evidence/ge-fill-argb` before amending
the local gitlink to the evidence documentation commit. Implementation/fixes
form one feature commit followed by this evidence commit; only the component
branch is pushed using boa-z credentials. No board was flashed. Direct hardware
transparent-target blending and performance are not established by this hybrid
path or its host model.

## YUV straight ARGB target composition (2026-10-04)

All twelve published YUV layouts now use bounded GE conversion/geometry followed
by native CPU target composition. RGB and YUV share the private staging helper;
rotated YUV tile gaps skip unwritten spans so even hidden target RGB survives.
See [ownership, precision and board probes](ge-yuv-argb-stage.md).

- Combined FreeType/vector/SVG/Lottie host **86/86 PASS** (23.07 s).
- FreeType/vector/SVG/Lottie-disabled baseline **79/79 PASS** (20.80 s).
- 432 scenes across twelve layouts, four rotations, three scale/opacity values;
  221,184 visible RGBA pixels checked against independent inverse coordinates
  and source-over arithmetic, maximum deviation 3. Source-Y split is exact;
  source-X split maximum deviation 1. Rotated tile gap guards/split pixels pass.
- Allocation/address rejection, premultiplied-target rejection, late tile
  preflight, producer retirement, six two-strip DMA faults and a later tile
  fault preserve planes/scratch and the original target. Cache order and the
  existing 1,680-scene RGB matrix pass against the shared helper.
- Full D13x boot/app, final-link/static, image and clean-source manifest **PASS**;
  all 29 file hashes and three clean source pins independently verified.
- Forty-eight additional offscreen probes compiled/linked. Physical CSC/filter,
  alpha/cache/CMA, memory pressure, performance and panel acceptance **NOT_RUN**.
  Host ramps model BT.709 full-range descriptors; this is not hardware CSC proof.

Clean build identities (documentation commits are not build pins):

- SDK: `027558b6b46390fc3562c183ee3054622d700b8b`.
- lvgl-aic: `aeaa2d9d2f78de0b538eb3770945dc0e5bfb8fe2`.
- LVGL: `80ca777e37a2b176770726a02e07a6fb79ef0b39`.
- Image SHA256: `49c89426a56851a0c41b453bdd1b0885f4e3f35c8cb85f9fb1d7aa9d27697d60`.
- ELF SHA256: `9c64840177f71a9f4379a55ffcd1268e2cdd07b151f5fe0843d5fcc24809584b`.
- Manifest: SDK `output/lvgl-evidence/ge2d-fonts-gif-widgets-aicp-player-apng-barcode-spi-demos-music-vector-svg-lottie-rotate90-ge-yuv-argb/manifest.json`.
- Component logs: `output/ge-yuv-alpha-build.log`, `output/ge-yuv-alpha-tests.log`,
  `output/ge-yuv-alpha-baseline-build.log`, `output/ge-yuv-alpha-baseline-tests.log`,
  `output/ge-yuv-alpha-firmware.log`.

The clean SDK build pin is retained at `refs/lvgl-evidence/ge-yuv-argb` before
amending the local gitlink to the evidence documentation commit. Stage fixes are
consolidated into one feature commit followed by one evidence commit. Only the
component branch is pushed using boa-z credentials; no board was flashed.

## Straight ARGB targets with GE geometry and native blend (2026-10-04)

Ordinary RGB IMAGE/LAYER tasks targeting straight ARGB now use bounded raw GE
staging followed by LVGL native CPU composition. This preserves native target
alpha without assuming the direct hardware blender's transparent-target output.
See [format, memory, cache and precision boundaries](ge-argb-target-stage.md).

- Final combined FreeType/vector/SVG/Lottie host **86/86 PASS** (22.01 s).
- Final FreeType/vector/SVG/Lottie-disabled baseline **79/79 PASS** (20.26 s).
- 1,680 source/opacity/effect/geometry/target scenes; 14,820,456 native SW channel
  comparisons. ARGB maximum deviations: native size 2, transformed constant
  interiors 4, transformed ramps 14; alpha errors capped separately at 3.
  Filter/quantization differences and low-alpha amplification remain explicit.
- Entire/split model refreshes match exactly. Nineteen staged copy/scale/
  multipass/late-tile DMA faults retain buffers with the original target
  unchanged; four allocation failures, address/budget rejection and nine-tile
  sharing pass. Cache checks require intermediate invalidation before native
  blend and target clean after the CPU writes. Existing SDK controls still pass.
- Full D13x boot/app, final-link/static, image and clean-source manifest **PASS**;
  all 29 hashes and three clean source pins independently verified.
- Eighteen additional straight-ARGB offscreen probes compiled and linked.
  Hardware pixels/cache/CMA, precision, memory peaks, timing and panel acceptance
  remain **NOT_RUN**. Premultiplied destinations, YUV target composition and
  partial-ARGB hardware FILL are outside this stage.

Clean build identities (documentation commits are not build pins):

- SDK: `5bc79e535fe56acf4988e844e7f96a145781acd4`.
- lvgl-aic: `eee6366fd5bccba4f10e9a97f76db33a263b746c`.
- LVGL: `80ca777e37a2b176770726a02e07a6fb79ef0b39`.
- Image SHA256: `faa070ddb7a2113b40e4d0eca70655f59a32ec93d014853a8658392bf9a3cec3`.
- ELF SHA256: `064d8412e264a4c795d27817de732fdbd4cb04598884673bd9a20a831777f21b`.
- Manifest: SDK `output/lvgl-evidence/ge2d-fonts-gif-widgets-aicp-player-apng-barcode-spi-demos-music-vector-svg-lottie-rotate90-ge-argb-target/manifest.json`.
- Component logs: `output/ge-alpha-build.log`, `output/ge-alpha-tests.log`,
  `output/ge-alpha-baseline-build.log`, `output/ge-alpha-baseline-tests.log`,
  `output/ge-alpha-firmware.log`.

The clean SDK build pin is retained at `refs/lvgl-evidence/ge-argb-target` before
amending its gitlink to the evidence documentation commit. Stage fixes are
consolidated in one feature commit followed by one evidence commit. Only the
component branch is published using boa-z credentials. No board was flashed.

## CPU recolor plus GE composition (2026-10-04)

Bounded native CPU source preparation now feeds the existing GE IMAGE/LAYER
copy, scale, rotation and tile paths. SDK reference rejects recolor; this is an
extension. See [format, budget and evidence limits](ge-recolor-stage.md).

- Combined FreeType/vector/SVG/Lottie host **86/86 PASS** (40.30 s).
- FreeType/vector/SVG/Lottie-disabled baseline **79/79 PASS** (31.67 s).
- 432 IMAGE/LAYER scenes; 4,580,280 native SW channel comparisons. Maximum
  deviation 3 for native-size/constant interiors and 9 for transformed ramps.
  Filter order and RGB565 quantization differ; arbitrary edge equivalence and
  hardware filter arithmetic are not claimed.
- Source immutability, exact model split refreshes, nine-tile staging reuse,
  late-cell preflight, bridge bounds/padding, allocation/address rejection,
  direct effect fallback and 18 DMA failure/quarantine points **PASS**.
- Full D13x boot/app, final-link/static, image and clean-source manifest **PASS**;
  all 29 file hashes and three clean source pins independently verified.
- Eighteen offscreen recolor probes compiled and linked. Physical pixels,
  cache/CMA pressure, memory peaks, timing and panel acceptance **NOT_RUN**.

Clean build identities (documentation commits are not build pins):

- SDK: `0222ecfd3b07129283c901dcc44d9f06a24186a8`.
- lvgl-aic: `e57dd38564e32bf8e78320da89aaf10c25f9bf14`.
- LVGL: `80ca777e37a2b176770726a02e07a6fb79ef0b39`.
- Image SHA256: `ebb7e42935b7e667fa826c029a01b90cadac2cfd57d1a2e2999b57cf03a21a83`.
- ELF SHA256: `09f740015f02c8b68bc718e8dc6c5af6a45846e5c66a081087db96437a1a6f6f`.
- Manifest: SDK `output/lvgl-evidence/ge2d-fonts-gif-widgets-aicp-player-apng-barcode-spi-demos-music-vector-svg-lottie-rotate90-ge-recolor/manifest.json`.
- Component logs: `output/ge-recolor-build.log`, `output/ge-recolor-tests.log`,
  `output/ge-recolor-baseline-build.log`, `output/ge-recolor-baseline-tests.log`,
  `output/ge-recolor-firmware.log`.

The SDK build pin is retained locally at `refs/lvgl-evidence/ge-recolor` before
amending its gitlink to the following evidence documentation commit. The stage
has one consolidated feature commit and one evidence commit; only the component
branch is published, using boa-z credentials. No board was flashed.

## Native dynamic widget validation (2026-10-04)

A final optional manual page now exercises six previously uncovered native
widget families. The native LVGL implementations already existed; this closes
integration verification and board-control gaps. See [page behavior and
coverage](native-widgets-stage.md).

- Combined FreeType/vector/SVG/Lottie host **86/86 PASS** (22.03 s).
- FreeType/vector/SVG/Lottie-disabled baseline **79/79 PASS** (31.96 s).
- Final feature-guard/header cleanup: focused platform/manual/native tests
  **3/3 PASS**, plus strict syntax compilation with widget features absent.
- Six full native init/deinit lifecycles across 800x480 and 480x800; all animated
  frames, spinner pixels, stable paused renders, needle and image-button state
  pixels, span updates, 120 pointer-scrolled windows, interception, page hide /
  re-entry and animation/child teardown. Both layout previews inspected.
- Full D13x boot/app, final-link/static, image and clean-source manifest **PASS**;
  all 29 file hashes and three clean source pins independently verified.
- Physical GE/font/widget/touch and panel acceptance remain **NOT_RUN**.

Clean build identities (documentation commits are not build pins):

- SDK: `4cf59e1dea5a31110340485cd2a80b3c29d200d6`.
- lvgl-aic: `b90bd671f0357a1fafeac002f9efb1b17a9bb223`.
- LVGL: `80ca777e37a2b176770726a02e07a6fb79ef0b39`.
- Image SHA256: `ffe1392a0e13ffdc0250b981c43346038da641ac44909ac8aa7cd93eeb442a9e`.
- ELF SHA256: `83196120c2746d50e3facbf232908b4a33cf5232108a40f3d35464eb26e9ff74`.
- Manifest: SDK `output/lvgl-evidence/ge2d-fonts-gif-widgets-aicp-player-apng-barcode-spi-demos-music-vector-svg-lottie-rotate90-native-widgets/manifest.json`.
- Component logs: `output/native-widgets-build.log`, `output/native-widgets-tests.log`,
  `output/native-widgets-baseline-tests.log`, `output/native-widgets-focused-tests.log`,
  `output/native-widgets-firmware.log`.
- Software previews: component `output/native-widgets-800x480.png` and
  `output/native-widgets-480x800.png`. They show the page area beneath the reserved
  64-pixel navigation header, without the rest of the integrated smoke UI.

The SDK build pin is retained locally at `refs/lvgl-evidence/native-widgets`
before amending its gitlink to the following evidence documentation commit.

## FreeType cache controls (2026-10-04)

The reserved cache feature now supplies SDK-compatible statistics and selective
purge on native LVGL 9.6, with reference preflight and failure-preserving plans.
See [API, ownership and boundaries](ft-cache-stage.md).

- Combined FreeType/vector/SVG/Lottie host **85/85 PASS** (39.19 s).
- FreeType/vector/SVG/Lottie-disabled baseline **78/78 PASS** (15.25 s).
- Three native font lifecycles; shared faces, Latin/CJK bitmap reconstruction,
  filters, glyph/draw separation and held-entry rejection. All 45 injected
  component allocation failures preserve caches. Three manual size/style cache
  probes also execute in each host font UI lifecycle.
- The previous 83-test combined host profile had FreeType tests disabled; it
  is not retrospectively evidence for these two font contracts. Combined target
  firmware already included native fonts, and now also includes cache controls.
- Full D13x boot/app, final-link/static, image and clean-source manifest **PASS**;
  all 29 file hashes and three clean source pins independently verified.
- Physical cache probes, font/display behavior and heap/performance acceptance
  remain **NOT_RUN**. L1-enabled cache builds and a global byte budget are outside
  this validated stage. No board was flashed.

Clean build identities (documentation commits are not build pins):

- SDK: `f539dfe742a8a4672f1d0e3f71f3483ea4dfdf57`.
- lvgl-aic: `8eff10b2c634bc0ef43fcc9ed3caebe6c7d1aa97`.
- LVGL: `80ca777e37a2b176770726a02e07a6fb79ef0b39`.
- Image SHA256: `1334351ebb69d9c2f46f23d21fc82c5fb7d18cf25988464f29e9a761f0e4fe84`.
- ELF SHA256: `36bd7e87950b2494bfc1f9a00dc3592128dfcd98941853940795a8bf974bb74e`.
- Manifest: SDK `output/lvgl-evidence/ge2d-fonts-gif-widgets-aicp-player-apng-barcode-spi-demos-music-vector-svg-lottie-rotate90-ft-cache/manifest.json`.
- Component logs: `output/ft-cache-build.log`, `output/ft-cache-tests.log`,
  `output/ft-cache-baseline-tests.log`, `output/ft-cache-firmware.log`.

The SDK build pin is retained locally at `refs/lvgl-evidence/ft-cache` before
amending its gitlink to the following evidence documentation commit. The stage
uses one consolidated feature commit and one evidence commit; SDK pins remain
local and the component branch is published using boa-z credentials.

## YUV stripes and chroma-safe partial refresh (2026-10-04)

The shared planner now covers all twelve YUV layouts, preserving aligned crops,
both scaler channels and source leases. Scaled odd crop origins retain their
sampling coordinate through phase compensation. Chroma filter extents also
correct the rotated partial-refresh edge-clamping defect. See [implementation
and boundaries](yuv-stripes-stage.md).

- Combined host **83/83 PASS** (17.61 s); vector/SVG/Lottie-disabled baseline
  **78/78 PASS** (17.31 s).
- 7,616 YUV descriptor plans: exact luma/chroma inverse coordinates, aligned
  extents, preserved CSC/plane storage, complete coverage and invalid-channel
  preflight. The existing 864 RGB stripe plans still pass.
- Real YUV executor model: 48 colored-plane scenes / 24,576 independently mapped
  pixels, both refresh split axes and odd source-Y clipping. Source-Y splits
  are byte-identical; source-X/odd-crop comparisons allow one RGB value for
  existing Q16 per-clip rounding. Owner retirement, tiled preflight and all six
  two-strip DMA failures preserve the frame lease and block replay.
- Full D13x boot/app, final-link/static, image and clean-source manifest checks
  **PASS**. All 29 file hashes and three clean source pins independently verified.
- Thirty-two colored I420 board probes check 512 pixels each, both opacities,
  all orthogonal rotations, two ratios and whole/split refresh. Hardware
  filtering, CSC precision, clip seams and performance remain **NOT_RUN**.

Clean build identities (documentation commits are not build pins):

- SDK: `af408e8df250d5d28c8ca2ba905133506c8159d1`.
- lvgl-aic: `3b853674e2cec1491b190985fc50bc50f7e3fb2e`.
- LVGL: `80ca777e37a2b176770726a02e07a6fb79ef0b39`.
- Image SHA256: `155450dc389ee707a39756b6c5a2dbabbd8b667b467ecac4776c5f0d1a982083`.
- ELF SHA256: `b30a50d6b0f387c48dc7e9c63147db5073629d9bfda36156d8908dba9bd9304f`.
- Manifest: SDK `output/lvgl-evidence/ge2d-fonts-gif-widgets-aicp-player-apng-barcode-spi-demos-music-vector-svg-lottie-rotate90-yuv-stripes/manifest.json`.
- Component logs: `output/yuv-stripes-build.log`, `output/yuv-stripes-tests.log`,
  `output/yuv-stripes-baseline-tests.log`, `output/yuv-stripes-firmware.log`.

The SDK build commit is retained locally at `refs/lvgl-evidence/yuv-stripes`
before amending its branch gitlink to the following evidence documentation commit.

## SPI RGB565 scaler stripes (2026-10-04)

SPI GE conversion now uses the shared RGB strip planner in the near-unity
interval. Explicit phases follow SDK normal/CMDQ rules; a copied final source
pixel supplies the last filter tap. See [SPI stripe geometry, budget and
lifetime](spi-stripes-stage.md).

- Combined host **83/83 PASS** (17.68 s); vector/SVG/Lottie-disabled baseline
  **78/78 PASS** (16.33 s).
- Real converter CPU model: 44 scenes, 93,328 RGB565 pixels, all four rotations,
  byte swaps, exact per-pixel coverage, edge replication and 4095-to-4096 width.
  All nine three-strip DMA failure points retain staging and leave CPU output
  unchanged. Real session late-strip failure suppresses SPI and retains claims.
- Full D13x boot/app, final-link/static checks, image and clean-source manifest
  gates **PASS**; all 29 file hashes and three clean source pins independently
  verified.
- Eight offscreen board probes are compiled into the combined image. Each
  checks 512 converted pixels plus output guards without opening a SPI bus or
  panel. Physical GE filtering, panel output and throughput remain **NOT_RUN**.

Clean build identities (documentation commits are not build pins):

- SDK: `8b12b0f706e205d2aa3ed09f06a3dd24f531b2ea`.
- lvgl-aic: `8728db765cda340adf969f853afa79b4d141876a`.
- LVGL: `80ca777e37a2b176770726a02e07a6fb79ef0b39`.
- Image SHA256: `746fa0a6dc5a5fb0603006fc128a06f92c3efa339aa94bb291f897d079b63550`.
- ELF SHA256: `af73b2380ab29fa8c84032416fb7ff1fa6b1f464fbd8b40abbf919c0dfc2a35f`.
- Manifest: SDK `output/lvgl-evidence/ge2d-fonts-gif-widgets-aicp-player-apng-barcode-spi-demos-music-vector-svg-lottie-rotate90-spi-stripes/manifest.json`.
- Component logs: `output/spi-stripes-build.log`, `output/spi-stripes-tests.log`,
  `output/spi-stripes-baseline-tests.log`, `output/spi-stripes-firmware.log`.

The SDK build pin is retained locally at `refs/lvgl-evidence/spi-stripes` before
amending its branch gitlink to the following evidence documentation commit.

## Near-unity RGB GE stripes (2026-10-04)

Balanced RGB commands remove the whole-task near-unity scaler fallback for
ordinary and orthogonal IMAGE/LAYER, tiled requests and multipass preparation.
All descriptors are preflighted and retain the original sampling phase. See
[stripe geometry, ownership and limitations](ge-stripes-stage.md).

- Combined host **82/82 PASS** (17.38 s); vector/SVG/Lottie-disabled baseline
  **77/77 PASS** (16.61 s).
- 864 independent descriptor plans, including widths through 4096, and every
  submit/emit/sync failure in a 133-strip plan. Late geometry failure submits
  nothing. Real executor CPU model: 36 direct scenes / 90,528 ramp pixels and
  432 multipass scenes / 138,576 interior pixels; exact full/partial refresh,
  shared tiled preparation and failure quarantine all pass.
- Full D13x boot/app compilation, live stripe preflight/runner linkage, image
  and clean-source manifest gates **PASS**. The private descriptor helper is
  compiler-partitioned as `lv_aic_ge2d_stripe.part.0`; the linkage gate checks
  its live public callers rather than requiring the discarded wrapper symbol.
- Forty new board probes are compiled in: 32 direct and eight multipass stripe
  cases. Physical filtering, seams, clip guards, alpha and timing **NOT_RUN**.
- YUV and the separate SPI GE conversion planner keep their split-risk fallback.

Clean build identities (the following documentation commit is not a build pin):

- SDK: `771b22ab0966d10bbf133137008fd770ddb04f23`.
- lvgl-aic: `6df093902e132499ff77d1c7ec9ca1ecaba4bb47`.
- LVGL: `80ca777e37a2b176770726a02e07a6fb79ef0b39`.
- Image SHA256: `2e0caa7b9f306c197a71cb37e12555c8d9166da3481b1f57f86e2a3281a5f392`.
- ELF SHA256: `e281f0f286ddf86d4ff11fd862ba8fd178dc3d2993a97a2b33d06ecd53e060e6`.
- Manifest: SDK `output/lvgl-evidence/ge2d-fonts-gif-widgets-aicp-player-apng-barcode-spi-demos-music-vector-svg-lottie-rotate90-ge-stripes/manifest.json`.
- Component logs: `output/ge-stripes-build.log`, `output/ge-stripes-tests.log`,
  `output/ge-stripes-baseline-tests.log`, `output/ge-stripes-firmware.log`.

All 29 manifest file hashes and three clean source pins were independently
verified. Board identity is d13x/d50t-2-lite, scratch budget 2097152 bytes.
The SDK build commit is retained locally at `refs/lvgl-evidence/ge-stripes`
before amending its branch gitlink to the evidence documentation commit.

## GE arbitrary-angle scaled rotation (2026-10-04)

The RGB executor now combines padded premultiplied copy, raw-channel scaling and
rotation/composition with bounded shared scratch storage. Tiling validates every
cell before commands and shares preparation. See [multipass implementation and
limits](ge-multipass-stage.md).

- Combined host **81/81 PASS** (16.29 s); vector/SVG/Lottie-disabled baseline
  **76/76 PASS** (15.75 s).
- New real-executor CPU model: 288 IMAGE/LAYER scenes, 104,520 independent
  interior pixels, full/partial equality, nine tiles sharing one preparation,
  late-cell preflight rejection, allocation/address failures and nine uncertain
  DMA failure points. Both scaled-buffer borders remain transparent.
- Actual SDK normal/CMDQ alpha helpers pass preparation/raw-scale/final-opacity
  control-state checks. Neither test is hardware arithmetic evidence.
- Full D13x boot/app build, live planner/executor linkage, image and clean-source
  manifest checks **PASS**. Effective scratch budget verified as **2097152 bytes**.
- Eight additional board ramp/clip probes are compiled into the smoke image.
  Physical pixels, alpha/filtering edges and performance remain **NOT_RUN**.

Clean build identities (subsequent documentation commits are not build pins):

- SDK: `efe13454366fb835a219d1f2add4e39d8c2104da`.
- lvgl-aic: `dfa662ee33be52de572f34021537e358dc6de2e7`.
- LVGL: `80ca777e37a2b176770726a02e07a6fb79ef0b39`.
- Image SHA256: `f5780df98c1fc7183f2d79586b6fc6f858b5b9fde2f0e8fcf527e9ec822252c9`.
- ELF SHA256: `e288a9264d1b4e5e972b012aa2c7af98fd5f98a66014aec62fa1b3a8590a66d0`.
- Manifest: SDK `output/lvgl-evidence/ge2d-fonts-gif-widgets-aicp-player-apng-barcode-spi-demos-music-vector-svg-lottie-rotate90-ge-multipass/manifest.json`.
- Component logs: `output/ge-multipass-focus.log`, `output/ge-multipass-tests.log`,
  `output/ge-multipass-baseline-tests.log`, `output/ge-multipass-firmware.log`.

Use the combined build profile with `-EvidenceTag ge-multipass`. Image and ELF
hashes were independently verified against the manifest. The SDK build commit
is retained locally at `refs/lvgl-evidence/ge-multipass` when its branch gitlink
is amended to the following documentation commit.

## Software rotation precision (2026-10-04)

Matched Q15 forward geometry and inverse software sampling correct the remote
pivot fallback reproducers. Source-fingerprinted generated copies retain the
upstream checkout and SDK source boundary. See [implementation and numerical
limits](sw-rotation-stage.md).

- Combined host **80/80 PASS** (33.32 s); SVG/vector/Lottie-disabled baseline
  **75/75 PASS** (28.18 s).
- Focused test: 420 ARGB8888 scenes, 161,734 independent interior pixels,
  byte-identical full/partial rendering, 108,000 full-circle geometry probes
  with a maximum 3.707-pixel error in the declared tested domain.
- Real GE executor IMAGE/LAYER handoff passes for both 0.1-degree and 45-degree
  remote-pivot cases, without preceding cache preparation or hardware command.
- D13x boot/app build, generated-source bytes/live-symbol ownership, image and
  clean-source manifest gates **PASS**. Physical execution and target timing
  **NOT_RUN**.

Clean build identities (the following documentation-only commits are not build pins):

- SDK: `ec28f9c927991adeba576be2f06ce972bd60a2ab`.
- lvgl-aic: `dcfba3466596690a308d1f4113e200051d1693a3`.
- LVGL: `80ca777e37a2b176770726a02e07a6fb79ef0b39`.
- Image SHA256: `f3a4c72047db69a2380fea5712a96ba0e59d4d04adb953b7ebfd3cb07556ef87`.
- ELF SHA256: `accd7e009de2ff862e2d6a3757d3098181e455a3ed91943fe25158f4004e03e9`.
- Manifest: SDK `output/lvgl-evidence/ge2d-fonts-gif-widgets-aicp-player-apng-barcode-spi-demos-music-vector-svg-lottie-rotate90-sw-rotation/manifest.json`.
- Component logs: `output/sw-rotation-focus.log`, `output/sw-rotation-tests.log`,
  `output/sw-rotation-baseline-tests.log`, `output/sw-rotation-firmware.log`.

Use the combined build profile with `-EvidenceTag sw-rotation`. Both generated
rotation sources are included in the manifest and were independently hashed.
The GE hardware center domain and arbitrary-angle plus scale limitation are
unchanged; this stage improves native software fallback, not GE capability or
physical acceptance.

## GE ROTATE center bounds (2026-10-04)

Combined host regression **79/79 PASS** (16.43 seconds). The new center contract
covers 1,210 boundary/large-translation combinations, null arguments and
unchanged outputs on rejection. The real executor rejects unrepresentable
centers before cache work or submission in both preflight and execution.
A translated remote-pivot IMAGE/LAYER case verifies software outcome and an
interior pixel without GE calls. The initial rejection assertion failed against
the old executor, which would silently truncate the command center.

The combined D13x 90-degree GE/fonts/GIF/widgets/AICP/player/APNG/barcode/SPI/
widgets-demo/benchmark/music/vector/SVG/Lottie firmware passes boot/app,
final-link/static, image and clean-source manifest gates. The new rotation
center helper is required as a live final-link symbol.

- lvgl-aic: `f7e4f0b1e7c01c8765508e68b0e1f34331741a9c` (clean).
- sdk: `b1ba69c8e6e65f3121297b3dc72068d09183874a` (clean).
- lvgl: `80ca777e37a2b176770726a02e07a6fb79ef0b39` (clean).
- Image SHA256: `341516520f9a99dade09d8d5417247ccde0f68f2c4c733b2c7e63b23516e4f8d` (independently verified).
- Manifest: SDK `output/lvgl-evidence/ge2d-fonts-gif-widgets-aicp-player-apng-barcode-spi-demos-music-vector-svg-lottie-rotate90-ge-center/manifest.json`.
- Component logs: `output/ge-center-before.log`, `output/ge-center-focus.log`, `output/ge-center-tests.log`, `output/ge-center-firmware.log`.

Use the combined build profile and `-EvidenceTag ge-center`. The 74-test
disabled baseline was last run in the preceding SVG-composition stage.
SDK/LVGL sources remain unchanged. Signed 14-bit center bounds are a conservative
port policy; register endpoints are not physically accepted by these tests.
Exploratory software rendering also exposed native Q10 far-pivot precision
limits, documented as an open gap. Arbitrary-angle plus scale remains
unsupported and needs a multi-pass implementation. Camera stays disabled and
no SPI panel is bound. Hardware **NOT_RUN**; nothing was flashed.
See [center bounds and the remaining software precision gap](ge-center-stage.md).

## SVG composition and premultiplied masks (2026-10-04)

Combined SVG/vector/Lottie host regression **79/79 PASS** (34.90 seconds);
disabled baseline **74/74 PASS** (27.21 seconds). The new real-renderer contract
checks nine image-effect scenes across 20 draw lifetimes, independent composed
pixel arithmetic, nested image/parent opacity, straight-alpha destinations and
allocation-failure cleanup. It reproduces the old ignored-opacity result
(expected RGB 136/16/24, got 254/0/0) and confirms the correction. Existing
48-case SVG geometry checks remain green.

A shared software-mask regression independently verifies centered mask coverage
0/64/128/255 for straight, explicitly premultiplied and flagged-premultiplied
layers. Premultiplied RGB and alpha now scale together. Every SVG temporary
layer is released with its allocation accounting restored after drawing.

The combined D13x 90-degree GE/fonts/GIF/widgets/AICP/player/APNG/barcode/SPI/
widgets-demo/benchmark/music/vector/SVG/Lottie firmware passes boot/app,
final-link/static, image and clean-source manifest gates. Generated SVG and
software-image corrections are verified by source contents and live owners.

- lvgl-aic: `53d76aa5a22e48590735381cc017165a24d160f3` (clean).
- sdk: `66c9ac9e0738baf1902511356ee8502accc665a6` (clean).
- lvgl: `80ca777e37a2b176770726a02e07a6fb79ef0b39` (clean).
- Image SHA256: `6077767e7f6a3ccdfa23ccbcc7c28c43d84bf3b0e9e9ff6b5b614e3473647b4a` (independently verified).
- Manifest: SDK `output/lvgl-evidence/ge2d-fonts-gif-widgets-aicp-player-apng-barcode-spi-demos-music-vector-svg-lottie-rotate90-svg-compose/manifest.json`.
- Component logs: `output/svg-effect-before.log`, `output/svg-effect-focus.log`, `output/svg-effect-tests.log`, `output/svg-effect-baseline-tests.log`, `output/svg-effect-firmware.log`.

Build with the combined profile and `-EvidenceTag svg-compose`. This introduces
a temporary premultiplied image for SVG composition: ordinary images allocate
the clipped visible area; tiled images allocate intrinsic size. Native sampler
semantics apply to transformed tiles; blend-mode and combined mask/transform
coverage is not exhaustive. SDK/LVGL sources remain unchanged. Camera stays
disabled, no SPI panel is bound and nothing was flashed. Physical rendering,
memory peak and performance **NOT_RUN**.
See [composition behavior and resource limits](svg-composition-stage.md).

## SVG image transforms and clipped child layers (2026-10-04)

Combined SVG/vector/Lottie host regression **78/78 PASS** (32.41 seconds).
A new independent inverse-transform oracle validates 48 scenes covering scale,
rotation, pivot, clip guards, canvas without an object, translated child layers,
real image widgets and parent opacity layers. The native code failed the initial
canvas case with 436 incorrect interior pixels. Existing FILE/VARIABLE, native
vector, Lottie and GE/software image regressions remain green. Logs retain both
the reproducer failure and corrected result.

The combined D13x 90-degree GE/fonts/GIF/widgets/AICP/player/APNG/barcode/SPI/
widgets-demo/benchmark/music/vector/SVG/Lottie firmware passes boot/app,
final-link/static, image and clean-source manifest gates. The checker validates
both generated source contents and live symbol ownership for the corrected
custom-image dispatcher and SVG decoder.

- lvgl-aic: `de530680d25943d823d823c70a1de3cd9d00fb14` (clean).
- sdk: `02fcc4e35aca10620ed7dd3a2ec5a2d107b49668` (clean).
- lvgl: `80ca777e37a2b176770726a02e07a6fb79ef0b39` (clean).
- Image SHA256: `450ecc659f293bcc4307cd3a681850a8f3c1d0ade4e21ca6be02baf1a482ae67` (independently verified).
- Manifest: SDK `output/lvgl-evidence/ge2d-fonts-gif-widgets-aicp-player-apng-barcode-spi-demos-music-vector-svg-lottie-rotate90-svg-transform/manifest.json`.
- Component logs: `output/svg-transform-before.log`, `output/svg-transform-focus.log`, `output/svg-transform-tests.log`, `output/svg-transform-firmware.log`.

Build using the combined profile and `-EvidenceTag svg-transform`. SDK/LVGL
sources stay unchanged; generated copies remain in the build/evidence directory.
Native image-specific opacity/recolor/tiling/rounded clipping and richer SVG
documents remain separate gaps. The older disabled baseline was not rerun for
this SVG-only implementation. Camera stays disabled and no SPI panel is bound.
Physical SVG rendering/performance **NOT_RUN**; no flashing.
See [SVG transform correction](svg-transforms-stage.md).

## Checked Lottie source loading (2026-10-04)

Combined SVG/vector/Lottie host regression **77/77 PASS** (15.38 seconds).
Across 20 widget lifetimes, the real native parser/renderer verifies successful
replacement, first-frame pixel equality, continued animation and preserved
pixels/timing/pause on errors. Failure cases cover malformed/truncated JSON,
embedded NUL, zero-duration metadata, source/staging limits, forced staging
allocation failure, deleted native animation and LVGL filesystem open/seek/read/
early-EOF/oversize/close failures. Seven-byte progressing reads succeed and file
opens/closes remain balanced. The disabled 74-test baseline was last run in the
preceding premultiplied stage.

The combined D13x 90-degree GE/fonts/GIF/widgets/AICP/player/APNG/barcode/SPI/
widgets-demo/benchmark/music/vector/SVG/Lottie firmware passes boot/app,
final-link/static, image and clean-source manifest gates. Both checked loading
APIs survive the final link.

- lvgl-aic: `78caf64b756b90dc56f966dfd0488382b5f35d7e` (clean).
- sdk: `cad06a29b41e93d6528a75fc3c5d0557f216d40f` (clean).
- lvgl: `80ca777e37a2b176770726a02e07a6fb79ef0b39` (clean).
- Image SHA256: `1056ec039e86163165a6829f0ff3783af092aec4e0cd5630c5ccb76de67be49c` (independently verified).
- Manifest: SDK `output/lvgl-evidence/ge2d-fonts-gif-widgets-aicp-player-apng-barcode-spi-demos-music-vector-svg-lottie-rotate90-lottie-load/manifest.json`.
- Component logs: `output/lottie-resource-tests.log`, `output/lottie-resource-focus.log`, `output/lottie-resource-firmware.log`.

Build with the combined profile and `-EvidenceTag lottie-load`; prior evidence
images are retained. SDK/LVGL sources remain unchanged with no leaked LVGL
objects. Limits cover encoded/staging payloads, not the complete renderer heap;
internal allocation assertions remain an upstream limitation. Camera stays
disabled and no SPI panel is bound. Physical file access, heap pressure and
long-duration playback **NOT_RUN**; nothing was flashed.
See [checked Lottie resources](lottie-resources-stage.md).

## Premultiplied GE sources and software fallback (2026-10-04)

Combined SVG/vector/Lottie regression **77/77 PASS**; baseline **74/74 PASS**.
The real decoder/GE dispatch contract captures correct source flags and alpha
controls for explicit/flagged premultiplication across image, layer, scaled
orthogonal rotation, arbitrary rotation and tiling at three global opacities.
Unaddressable sources retain correct native software pixels. These engine mocks
prove routing and commands, not hardware pixel arithmetic.

A native software canvas test compares explicit/flagged pixels for nine plain,
opacity, rounded-clip, recolour, transformed and colour-key scenes. Actual SDK
normal/CMDQ alpha helpers are compiled and exercised at all 256 global opacities;
upstream source drift is rejected by the generated software-correction guard.

The combined D13x 90-degree GE/fonts/GIF/widgets/AICP/player/APNG/barcode/SPI/
widgets-demo/benchmark/music/vector/SVG/Lottie firmware passes boot/app,
final-link/static, image and clean-source manifest gates. The linked software
image symbol is verified to come from the generated correction. Six new real
GE-versus-software alpha/clip probes and expanded scale/tile/rotation probes are
compiled into the smoke image; their physical execution remains **NOT_RUN**.

- lvgl-aic: `90b02dc24c7219655c104b52888c8a8215a73343` (clean).
- sdk: `366fd8d6acd8578b91a40962aa3d03382df5c60c` (clean).
- lvgl: `80ca777e37a2b176770726a02e07a6fb79ef0b39` (clean).
- Image SHA256: `479c5a65fc5d585480cbc5f9b7a0a7525ade1e8e86df01dbf54c6859baa709dc` (independently verified).
- Manifest: SDK `output/lvgl-evidence/ge2d-fonts-gif-widgets-aicp-player-apng-barcode-spi-demos-music-vector-svg-lottie-rotate90-premult/manifest.json`.
- Component logs: `output/premult-tests.log`, `output/premult-baseline-tests.log`, `output/premult-firmware.log`.

Build with the combined profile and `-EvidenceTag premult`; the preceding Lottie
image is retained separately. SDK and LVGL source checkouts remain unmodified,
with no leaked objects. Premultiplied destinations and premultiplied colour-key
GE comparison remain outside this increment. Camera stays disabled, no SPI panel
is bound and nothing was flashed. See [premultiplied source stage](ge-premult-stage.md).

## Native Lottie animation and premultiplied composition (2026-10-04)

Combined SVG/vector/Lottie host suite **75/75 PASS**; disabled baseline **72/72
PASS**. After extending the GE pixel regression, its focused rerun and the full
disabled baseline both pass. The Lottie test uses the real native widget/parser/
renderer for FILE/data parity, raw and draw-buffer APIs, copied source lifetime,
two-second timing, timer progression, pause/resume/reset, half-opacity screen
pixels and 20 lifecycle cycles. GE submits no DMA for the explicit premultiplied
canvas and software preserves its expected pixel value.

Two real alpha defects were fixed within one feature commit: the pinned ThorVG
solid layer applied opacity twice, and the native Lottie draw-buffer API marked
ordinary ARGB8888 storage without changing the color format used by software
blending. A source-fingerprinted generated builder corrects the first; a small
application wrapper normalizes canvas metadata before caching for the second.
The target gate verifies the generated source, live builder section (including
GCC's inlined/partitioned helper) and the canvas wrapper's owning object.

Combined D13x 90-degree GE/fonts/GIF/widgets/AICP/player/APNG/barcode/SPI/
widgets-demo/benchmark/music/vector/SVG/Lottie firmware passes boot/app,
final-link/static, image and clean-source manifest gates.

- lvgl-aic: `015f01063bd2da83d1190f20a784c6d2bbc0b354` (clean).
- sdk: `f024c4fa4dcb11043a94f4ce0c673e0b9a82af11` (clean).
- lvgl: `80ca777e37a2b176770726a02e07a6fb79ef0b39` (clean).
- Image SHA256: `d6fa3e0122f995cf729635839f4c96f2389f09ee0528852840110873f3e13dc8` (independently verified).
- Manifest: SDK `output/lvgl-evidence/ge2d-fonts-gif-widgets-aicp-player-apng-barcode-spi-demos-music-vector-svg-lottie-rotate90/manifest.json`.
- Component logs: `output/lottie-tests.log`, `output/lottie-ge-test.log`, `output/lottie-disabled-tests.log`, `output/lottie-firmware.log`.

SDK and LVGL source trees remain unmodified and contain no leaked LVGL objects.
This is native `lv_lottie`, not compatibility with the SDK's old `lv_rlottie` API.
Checked malformed/resource-failure handling, expressions, external assets and
allocation budgets remain open. Camera stays disabled and no SPI panel is bound.
Physical animation timing, heap pressure, rendering and GE coexistence **NOT_RUN**;
no flashing. See [Lottie configuration and limits](lottie-stage.md).

## Native SVG resources and floating-point formatting (2026-10-04)

SVG/vector-enabled GE/widget host suite **74/74 PASS**. Real image-widget
contracts compare FILE and VARIABLE SVG output across 20 lifecycle/cache-drop
cycles, intrinsic dimensions, custom drawing, pixel rounding and file ownership.
The native vector test also verifies builtin floating-point formatting, replacing
the default RT-Thread formatter for vector/SVG configurations.

The combined D13x 90-degree GE/fonts/GIF/widgets/AICP/player/APNG/barcode/SPI/
widgets-demo/benchmark/music/vector/SVG firmware passes boot/app, final-link/
live-symbol/static, image and manifest gates. The formatter correction was folded
into the feature commit before this clean build; no standalone fix commit remains.

- lvgl-aic: `02f00e3020488ec29e44ca5fff416b08b685c0ea` (clean).
- sdk: `ee18f481e68eb8b54de6786c20fa078f4a7ec0b7` (clean).
- lvgl: `80ca777e37a2b176770726a02e07a6fb79ef0b39` (clean).
- Image SHA256: `7580d5f2837e818b0d5384e07f3076ca2b926cfc237031639b016a7fdb0e569d` (independently verified).
- Manifest: SDK `output/lvgl-evidence/ge2d-fonts-gif-widgets-aicp-player-apng-barcode-spi-demos-music-vector-svg-rotate90/manifest.json`.
- Component logs: `output/svg-build.log`, `output/svg-tests.log`, `output/svg-firmware.log`.

Source trees contain no LVGL build objects or SDK source modifications. This uses
LVGL 9.6 native SVG decoding over software vectors; ThorVG SVG/Lottie loaders
remain excluded. The earlier 72-test vector-disabled baseline was not rerun in
this stage. Camera remains disabled and no SPI panel is bound. Richer SVG
transforms/documents, embedded image/font callbacks and animation need further
coverage. Physical rendering, allocation pressure and performance **NOT_RUN**;
no flashing. See [SVG configuration and limits](svg-stage.md).

## Native vector/ThorVG combined firmware (2026-10-04)

Vector-enabled GE/widget host suite **73/73 PASS**; vector-disabled baseline
**72/72 PASS**. Native canvas pixel contracts cover paths, even-odd fill, opacity,
gradients, transformed clipping and repeated lifecycle. The combined D13x
90-degree GE/fonts/GIF/widgets/AICP/player/APNG/barcode/SPI/widgets-demo/benchmark/
music/vector firmware passes boot/app, final-link/live-symbol/static, image and
manifest gates. A live C++ configuration probe checks the application custom OS
bridge, matrix/float/vector settings and absence of loader/worker-thread features.

- lvgl-aic: `c4112b5012414ddea18bcca31224a5898838380c` (clean).
- sdk: `837ec005cc5a60f1af101d7d6eff794733b02d1a` (clean).
- lvgl: `80ca777e37a2b176770726a02e07a6fb79ef0b39` (clean).
- Image SHA256: `5a2c3b905e2c8dd0961327668ec0456f9b2e4f7b7e6a5b8073e6a4ca8e427237` (independently verified).
- Manifest: SDK `output/lvgl-evidence/ge2d-fonts-gif-widgets-aicp-player-apng-barcode-spi-demos-music-vector-rotate90/manifest.json`.
- Component logs: `output/vector-all-tests.log`, `output/vector-baseline-tests.log`, `output/vector-firmware.log`.

This enables native software vector drawing and selects the SDK's existing C++
runtime through Kconfig; no SDK or upstream LVGL source edits. No Lottie/SVG
loader, automatic vector demo or new GE vector acceleration is claimed. Camera
is disabled and no SPI panel is bound. Physical vector rendering, allocation
pressure, throughput and panel coexistence **NOT_RUN**. No flashing.
See [vector configuration and limits](vector-stage.md).

## SPI display overlap and combined demos firmware (2026-10-04)

**72/72 host PASS**, strict D13x component compilation/partial link **PASS**.
Full 90-degree GE/fonts/GIF/widgets/AICP/player/APNG/barcode/SPI/widgets-demo/
benchmark/music configuration passes boot/app, final-link/live-symbol/static,
image and manifest gates. The opt-in display pipeline, previous-DMA receipt,
worker lifecycle and idle-poll APIs are retained in the final ELF.

The three implementation commits were consolidated into one coherent stage,
`557a2ee`, with an identical Git source tree before/after history cleanup. The
full build was then rerun using the consolidated commit and a clean SDK gitlink.

- lvgl-aic: `557a2ee5ea0bf0705a5192873eb4de657a3942e7` (clean).
- sdk: `7cc0503a48ec341c89c6f35a6a1aa9aeea40d591` (clean).
- lvgl: `80ca777e37a2b176770726a02e07a6fb79ef0b39` (clean).
- Image SHA256: `1827fd9d7b163cc2b1d38f6c1ccc0cd504b82789d4705c7f112fef9e70124198` (independently verified).
- Manifest: SDK `output/lvgl-evidence/ge2d-fonts-gif-widgets-aicp-player-apng-barcode-spi-demos-music-rotate90/manifest.json`.
- Component logs: `output/spi-display-overlap-{build,tests,target,firmware}.log`.

Camera remains disabled and no SPI panel is bound. The image starts the smoke
UI; upstream demos need application selection, and music remains a visual UI.
Physical DMA/cache/GE concurrency, panel output and measured throughput are
**NOT_RUN**. No flashing. This supersedes the earlier pending final-firmware
notes for SPI session/worker/display overlap and the combined demo profile.

## Independent target music UI firmware (2026-10-04)

Music host render/track/animation/cleanup contract **PASS**; unchanged baseline
**70/70 PASS**. Strict E907 wrapper compile **PASS**. Independent music target
configuration (widgets/benchmark off) passes boot/app, final-link/static, image
and provenance gates. Six native entry/control APIs are live. Includes the GE
wide-coordinate crop correction; no source-tree objects or SDK source changes.

- lvgl-aic: `5c898239919c97d66bf5934e54e6cc76d8a1dbdd` (clean).
- sdk: `955f1bec8763701a3bf7851674cbaaf846e33750` (clean).
- lvgl: `80ca777e37a2b176770726a02e07a6fb79ef0b39` (clean).
- Image SHA256: `673f0a1d2c6ae2937f16f8854e3b13a7b1675fc182ef2babe81aa055ee27c77c` (independently verified).
- Manifest: SDK `output/lvgl-evidence/ge2d-fonts-gif-widgets-aicp-player-apng-barcode-spi-music-rotate90/manifest.json`.

The image links the demo and keeps smoke UI startup. Upstream music is a visual
interface with generated animation, not an audio decoder/player integration.
Hardware **NOT_RUN**. See [target demos](target-demos-stage.md) for activation,
configuration and evidence limits.

## Optional target widgets and benchmark demos (2026-10-04)

Baseline host regression **70/70 PASS**. Both new demo contracts **PASS**: widgets renders and cleans its screen;
benchmark completes all 16 official scenes with per-scene measurement samples.
These use software rendering and virtual time, not hardware performance data.
Strict E907 demo-wrapper compilation and live entry symbols **PASS**.

Combined 90-degree GE/font/GIF/widget/AICP/player/APNG/barcode/SPI/demo firmware
passes boot/app, final-link/static, image and manifest checks. Source checkouts
were clean; no objects remain in the LVGL source tree. This is a separate
`-demos` profile, preserving the prior no-demo evidence directory.

- lvgl-aic: `2652b94ffe54239d0ba42b7387b9eb07586b8e5f`.
- sdk: `2f86ec32e17f00d8598987d229ff3c625b455327`.
- lvgl: `80ca777e37a2b176770726a02e07a6fb79ef0b39`.
- Image SHA256: `8a64743d82a81373f9da84ba27b3f12937d45cee50dc8b74f01eaf97598fd3c4` (independently rechecked).
- Manifest: SDK `output/lvgl-evidence/ge2d-fonts-gif-widgets-aicp-player-apng-barcode-spi-demos-rotate90/manifest.json`.

The image does not auto-launch upstream demos; application startup selects them.
Physical validation **NOT_RUN**. See [target demo use and boundaries](target-demos-stage.md).

## Native-plane destination resize firmware (2026-10-04)

Clean combined 90-degree GE/font/GIF/widget/AICP/player/APNG/barcode/SPI
firmware: boot/app, final-link/static gates, image verification and manifest
**PASS**. Includes decoded-frame dimension correction, native-plane width/height
requests using DE destination scaling, and UI-thread `size WIDTH HEIGHT` controls.

- lvgl-aic: `5f6f8a036f19a213639d72fc5ea4db1b507563c7` (clean).
- sdk: `f3864b79c73d536efe3aef5a3e46d9aa0bc8d4b2` (clean).
- lvgl: `80ca777e37a2b176770726a02e07a6fb79ef0b39` (clean).
- Image SHA256: `556b01d11855dd64eb3e6d077e5d948de4855450b7b73c3029d11832aa79b325`.
- ELF SHA256: `701395d22ea1cd1959a482f6e2c85c76a48acf9d41408715edf62ea7f17b09bf`.
- Manifest: SDK `output/lvgl-evidence/ge2d-fonts-gif-widgets-aicp-player-apng-barcode-spi-rotate90/manifest.json`.

Image hash independently rechecked. All 18 player transform/size APIs and the
plane manual command remain live in the final ELF. The player implementation
passed **70/70 host tests**; the new shell source passed strict E907 compilation
with its registration, poll/deinit definitions and both size API references
verified. Shell mailbox execution is not covered by those host tests.

Physical execution **NOT_RUN**. Camera is disabled and no SPI panel is bound.
This supersedes pending full-firmware notes for the frame-dimension/native-plane
size changes. See [manual controls](../tests/manual/README.md) for deferred board
acceptance. Evidence directories are reused per profile; older hashes below
refer to historical images, not the current files in that directory.

## Player transform/size final-link evidence (2026-10-04)

Combined 90-degree GE/font/GIF/widget/AICP/player/APNG/barcode/SPI firmware:
boot/app builds, static gates, final-link checks, image verification and manifest
**PASS**. All 18 new player transform/size APIs are retained and live in the final
ELF. Includes deferred width/height requests and swipe generation protection.

- lvgl-aic: `ad478d0184508128846b0eb4b676e74471926519` (clean).
- sdk: `37ff276b12e85d1dd6e6343984143e0494309fa4` (clean).
- lvgl: `80ca777e37a2b176770726a02e07a6fb79ef0b39` (clean).
- Image SHA256: `965105a800aaa8e91988fc614c68e3320b1df7c33af5593a6698ea5f3c6c3e3e`.
- Manifest: SDK `output/lvgl-evidence/ge2d-fonts-gif-widgets-aicp-player-apng-barcode-spi-rotate90/manifest.json`.

The saved image hash was independently verified. Host regression at the current
implementation is **70/70 PASS**. Physical execution **NOT_RUN**; no flashing,
media playback, camera opening or SPI panel initialization was performed.
This closes pending full-link gates for the player accessors/sizing and swipe
reentry fix; it does not replace media/GE/video-plane board acceptance.

## Image roller and SPI GE combined firmware (2026-10-04)

Current clean 90-degree GE/font/GIF/widget/AICP/player/APNG/barcode/SPI profile:
boot/app, static/live-link, image and provenance gates **PASS**. Includes pending
image-roller target removal protection, SPI scale bounds/split-risk checks and
GE session integration. Host composed CPU/GE pipelines: **70/70 PASS**.

- lvgl-aic: `eab1b4045d16359da773b324a20eda10c0a28454` (clean).
- sdk: `30ec4fa2d9d7e630ea2b09b2b8a60c025d02397a` (clean).
- lvgl: `80ca777e37a2b176770726a02e07a6fb79ef0b39` (clean).
- Image SHA256: `dc8bfd58a19a4b391e296081905b7a44c67fbb459d1d0528829bf1cc6c9bc89f`.
- Manifest: SDK `output/lvgl-evidence/ge2d-fonts-gif-widgets-aicp-player-apng-barcode-spi-rotate90/manifest.json`.

The image hash was independently rechecked against the saved artifact. No SPI
panel is instantiated. Physical board execution **NOT_RUN**; no flashing.
This supersedes pending full-firmware notes for the roller child lifecycle and
SPI ratio/split guard. Test and source history below is retained as milestones.

## SPI GE session firmware (2026-10-04)

Combined GE/font/GIF/widget/AICP/player/APNG/barcode/SPI with rotation 90:
boot/app build, static/live-symbol gates, image checks and manifest **PASS**.
Includes the shared GE-fault display teardown fix and opt-in SPI GE session.
All converter entry points are live in the final ELF. No SPI panel is created.

- Component: `7ba23542a1fe8c3f2a9c87249cb8f25a4562f1c8` (clean).
- SDK: `d9db7d5349d1d45442b3034b65c5467a44f9948f` (clean).
- LVGL: `80ca777e37a2b176770726a02e07a6fb79ef0b39` (clean).
- Image SHA256: `299e055accd141198412183b81c274e8c9bd438755f90f9f0560cf0d7f06d822`.
- Manifest: SDK `output/lvgl-evidence/ge2d-fonts-gif-widgets-aicp-player-apng-barcode-spi-rotate90/manifest.json`.

Host **69/69 PASS**. Physical execution **NOT_RUN**, no flashing.
Subsequent SPI scale-ratio/split-risk guard is host/partial-link tested but not
included in this image; see [SPI stage](spi-stage.md). The evidence directory is
reused for the same profile; older manifests/hashes below are historical records.

## Rotated combined firmware evidence (2026-10-04)

The combined GE/widget/media/SPI profile now also has a clean 90-degree firmware build:
boot/app build, static/live-map gates, image checks and manifest **PASS**.
Includes panel lifecycle hooks, worker stage timing and rotation DMA quarantine.

- Component: `c257ec759e3fc96b9f7a9c2672433c861b5209bc` (clean).
- SDK: `4ee81ce25b578d8a08addb4dd2cd2d45c4969016` (clean).
- LVGL: `80ca777e37a2b176770726a02e07a6fb79ef0b39` (clean).
- Image SHA256: `874a62d195e88da8e4810ea138f1c09b149f30c48851766670638563bbb30c4c`.
- Manifest: SDK `output/lvgl-evidence/ge2d-fonts-gif-widgets-aicp-player-apng-barcode-spi-rotate90/manifest.json`.
- Config: `AIC_LVGL_DISPLAY_ROTATION=1`, `AIC_LVGL_USE_SPI_SDK=y`.

Host suite at this revision: **68/68 PASS**. Hardware **NOT_RUN**; no SPI panel
is instantiated. Subsequent shared-fault teardown protection is not included
in this firmware; its validation is recorded in display-rotation-stage.md.

## Current combined GE/widget/SPI regression (2026-10-04)

The latest combined profile enables GE2D, FreeType, GIF, widgets, AICP, player,
APNG, barcode and SPI. Host contracts **67/67 PASS**, including composed SPI
renderer/worker/driver lifetime and GE DMA-window boundaries. Bootloader and
application build, static integration/live-symbol checks, image verification and
provenance generation **PASS**. This includes the direct-blit/statistics APIs and
the complete GE draw-buffer address-span fix; earlier pending final-link notes
for those increments are superseded by this record.

- Component: `7f578cc509ed94e660d0dfe567bb13d6822a8a8c` (clean).
- SDK: `62587ac5434615eb282735093549b04ee4b0ac88` (clean).
- LVGL: `80ca777e37a2b176770726a02e07a6fb79ef0b39` (clean).
- Image SHA256: `1f4eb864d64148ed5960772dc882b76a2e217e41843720552f9b9b820aca61c4`.
- Manifest: SDK `output/lvgl-evidence/ge2d-fonts-gif-widgets-aicp-player-apng-barcode-spi/manifest.json`.
- Image: same evidence directory, `images/d13x_D50T-2-Lite_page_2k_block_128k_v1.0.0.img`.

SPI final-map gates cover display, worker, panel, session and SDK transport.
Independent ELF inspection confirms `claim_blit`, `blit`, `blit_take` and `stats`.
This smoke image does not instantiate an SPI panel or redirect the NAND bus.
Physical board execution is **NOT_RUN** and no flashing was performed. Historical
operator UI acceptance does not establish acceptance of this image.

Remaining implementation/acceptance scope includes explicit panel power/TE and
initialization binding, stage-specific transport timing and GE/DMA pipelining,
multi-display execution, and the documented GE/media/camera/widget board gates.
The YUV audit confirms existing per-plane 32-bit bounds, 16-bit stride and full
row capacity checks in `lv_aic_yuv_to_mpp`, plus cache-end guards before GE
submission; the new RGB draw-buffer fix does not replace those checks.


## Current status index (2026-10-01)

Earlier paths, kernel patches, scope and pending statements below describe
historical revisions, not the current integration. Current instructions are in
[integration-luban-lite.md](integration-luban-lite.md); feature inventory is in
[capabilities.md](capabilities.md). Historical sections are preserved as evidence.

- Completed history consolidated in d26f0b0 and pushed to
  codex/sdk-basic-capabilities. The operator accepts the current UI layout and
  bidirectional page switching. This does not close every numeric board gate.
- Native GIF candidate: GIF + FreeType + SDK headers 10/10, features OFF 8/8
  host tests PASS. GE2D + FreeType + GIF build/link/image gates PASS. Board GIF
  acceptance remains NOT_RUN; see [GIF stage](gif-stage.md). Navigation tests
  now use actual top-layer coordinates and have no synthetic event bypass.
- Historical native font-stage implementation 3b7e090: fonts ON 9/9 and OFF 8/8 host
  tests PASS. Clean-source Gate 1, GE2D baseline and GE2D+FreeType firmware
  build/link/image gates PASS. Board validation remains NOT_RUN. The immutable
  font-stage-3b7e090 archive indexes 72 evidence files; see
  [font handoff](font-stage.md) for image hash and panel/log acceptance criteria.
- Resource-stage development is complete at a5b5bd4: 8/8 host contracts and
  Gate 1 / MPP / GE2D build/link/image gates PASS. Board validation is PARTIAL:
  supplied serial probes and operator visual confirmation PASS; see the latest
  board feedback below for missing evidence and logging defects.
- OS is now LV_OS_CUSTOM with RT events; no SDK semaphore patch is required.
- Dependencies live under application/rt-thread/<app>/third_party/.
- IMAGE scale/right-angle/combined transforms are implemented; later
  [transform records](phase3c-transform.md) supersede earlier NOT_STARTED entries.
- Board observations apply only to their identified builds.
- SDK 08b9f5f0 / component 2294faf: six host contracts and three firmware
  build/link/image gates passed; the current board feedback supersedes the
  earlier NOT_RUN status for the checks actually observed.
- Bounded timing code exists. Paired GE ON/OFF board timing remains open.


## Resource-stage board feedback (2026-09-30)

The operator reported flashing the delivered image, supplied a serial excerpt,
and subsequently confirmed: "界面验证正常" (the interface verification is normal).
The expected candidate is component a5b5bd4, GE2D image SHA256
 e997674a26c1187c648d03c3c919f8fcd672a080d646552875964622820af3e6.
The running image hash was not read back; attribution is based on the operator's
statement, not a device identity measurement. The archived build manifests retain
their original NOT_RUN values as build-time provenance.

| Check | Observed result | Evidence |
|---|---|---|
| Wrapper CMA stress | PASS | current=0, alloc=1000; the free count is interrupted by a logging warning, but the explicit lifecycle PASS requires alloc==free and alloc>=1000 |
| Solid fill | PASS | All 12 RGB565/RGB888/XRGB8888 opacity probes pass, 90 pixels each, guards=OK; maximum errors are 5/2/2/5 for RGB565 and 0 for the other formats |
| GE scheduling | PASS for reported refresh | 21 fill and 19 image engine executions, 0 image SW fallbacks, 1 SW layer, 7 declined tasks, errors=0 |
| Blend probes | PASS in supplied excerpt | Three reported deviations are 0/255; the deliberately wrong premultiplied rule differs by 100/255; some line tails and the PASS summary are truncated |
| Scale probes | PASS for six supplied cases | RGB/ARGB at 0.5x, 1.5x and 2x; max_error 0/1/0, clip_guard=OK and engine=1 |
| First frame / scheduler | PASS at startup | first frame presented; scheduler is still progressing |
| Interface appearance | PASS, operator confirmed | User reports the interface verification is normal following the three-page checklist; no framebuffer measurement was supplied |
| Memory/cache resource stage | Indirect success; direct log pending | On this candidate the resource test must return success before the 1000-cycle loop can run; its earlier parity/cache PASS lines are absent from the supplied excerpt |
| Touch-specific and timed endurance checks | Not separately recorded | The brief interface confirmation does not specify down/move/release behavior or a five-minute observation duration |

Two logging issues remain: async log buffer exhaustion can lose/interleave
records, and the timing line prints "us=%lu ge2d_ready=47437" even though the
source uses %llu followed by a boolean %u. That line cannot establish elapsed
time or ready state; format compatibility must be corrected before timing
acceptance. This is separate from the GE numeric probes that already passed.

The outstanding evidence is the earlier resource-stage serial block, explicit
touch/timed-running observations, and device/image identification if available.
The current statement closes visual acceptance, not every hardware gate. No
firmware change, new build or flashing was performed to record this feedback.

## Resource-stage development complete (2026-09-30)

Branch: codex/sdk-basic-capabilities.
Tested component: a5b5bd41ab0f5c5efe5a4149a0ceda9b195cc780 (clean).
SDK: 08b9f5f09bab4dea199f36b2bfefec3f773e2b6a.
LVGL: 80ca777e37a2b176770726a02e07a6fb79ef0b39 (v9.6.0, clean).
Remote main was rechecked at 2294fafdbe36916d60ac311f49a1e32283cefbb8,
and is an ancestor of the tested component. This follow-up record is docs-only.

The [resource stage](resource-stage.md) adds borrowed RAW/RAW_ALPHA JPEG/PNG
sources and a component-owned 512 KiB / 16-entry decoded-image LRU. Keys include
source and decode options. Invalidation defers frees for active readers, cache
pressure evicts idle entries, no_cache bypasses retention, and pending readers
refuse platform teardown until closed. No SDK source, upstream LVGL core or
global allocator/cache handler changes are required. It includes the earlier
partial-opacity FILL candidate, but does not claim its board gate is closed.

| Gate | Result | Evidence boundary |
|---|---|---|
| Host build / CTest | PASS, 8/8 | GCC 16.1 UCRT64 Debug; production code with mocked BSP/MPP/GE |
| Memory/cache contract | PASS | Header/CRC/stream bounds; decode-option keys; FILE key ownership; byte/entry limits and LRU; active invalidation; allocation/decode failure; pressure retry; reset and reinit |
| Gate 1 target | PASS | Build, live symbols, ELF ABI, packaging and nine payload CRCs |
| MPP target | PASS | Same gates; resource/cache live symbols; 26 fixture/provenance hashes |
| GE2D target | PASS | Integrated resource and fill/transform probes; all static/image gates |
| Archived manifests | PASS | 18/19/19 referenced files rehashed; 65 archived files indexed with SHA256 |
| Board resource/fill probes | NOT_RUN | Actual pixels, cache coherency, CMA lifetime and engine behavior need the exact candidate on D50T-2-Lite |
| App/OS display/touch regression | NOT_RUN | Prior builds do not certify this application-owned integration |

The build commands are the same as the preceding candidate, with all three
profiles run sequentially in the isolated SDK checkout: gate1, mpp, ge2d.
Host commands are documented in tests/host/README.md. The SDK active config now
selects the GE2D smoke profile. Existing short-argument/pywin32 environment
warnings remain; no new component compiler warnings were found.

The immutable local handoff is in:

    C:/Users/JCSH/.codex/worktrees/d50t-lvgl-port/luban-lite-jc-d50t-rev/output/lvgl-evidence/resource-stage-a5b5bd4/

It contains gate1/, mpp/, ge2d/, host/ and stage-index.json. Each target folder
contains its manifest, exact config/headers, logs, source provenance, ELF/map
and image. The host folder preserves CTest LastTest.log and its CMake metadata.
Each image has the filename d13x_D50T-2-Lite_page_2k_block_128k_v1.0.0.img:

| Profile | Bytes | SHA256 |
|---|---:|---|
| Gate 1 | 1550848 | 0416180742741369d8edc55b4b0c327ea1c6293a9500a77e79ac3e42f1b1ac2e |
| MPP | 1794560 | 8b152ea44b88178b7a12d6331a7ac25f38365aad27d5fbf3cc236203759c9af9 |
| GE2D | 1825280 | e997674a26c1187c648d03c3c919f8fcd672a080d646552875964622820af3e6 |

Use the archived GE2D image for integrated board validation; MPP is the
software-rendering control and Gate 1 is the display/touch baseline. Confirm
the image hash and retain the full serial log. Required resource evidence:
three file-memory parity PASS lines, two 100-cache-hit PASS lines, resource-stage
PASS, and the existing 1000 uncached cycles/CMA balance. GE2D must also pass the
12 solid-fill numerical probes and existing image/transform checks. Inspect
all three pages and touch down/move/release; panel acceptance is separate from
file-memory hash equality. Detailed serial signatures are in resource-stage.md.

The Hangcha checkout was checked clean on codex/hangcha-zc-202620085 with
CONFIG_PRJ_APP="forklift-meter-platform". The parent port checkout differs only
in the local component gitlink; it is not promoted to an unpublished commit.
No flashing or push was performed. Development is complete for this scope;
board acceptance remains open. Other SDK gaps (YUV/AICP/BMP, GE display rotation,
font/input/media integrations and paired timing) are not part of this stage.

## Solid-fill capability candidate (2026-09-30)

Branch: codex/sdk-basic-capabilities. Documentation alignment: 832322e.
Tested component: 1623ddafa1fb9efa099d1c7d3ede6dd25cf996f2 (clean).
SDK: 08b9f5f09bab4dea199f36b2bfefec3f773e2b6a.
LVGL: 80ca777e37a2b176770726a02e07a6fb79ef0b39 (v9.6.0, clean).
Remote main was checked at 2294fafdbe36916d60ac311f49a1e32283cefbb8.

This increment adds partial-opacity solid FILL on RGB565/RGB888/XRGB8888,
with task/clip/layer intersection, buffer-size/stride/address guards and
checked fillrect/emit/sync. Opaque ARGB8888 is retained; partial ARGB8888,
rounded fills and gradients remain software tasks. It changes no SDK source
or upstream LVGL core. Two existing manual-test type/format warnings were fixed.

| Check | Result | Boundary |
|---|---|---|
| Host configure/build + CTest | PASS, 7/7 | GCC 16.1 UCRT64, Debug; real LVGL and real evaluator/executors, mocked hardware |
| New fill contract | PASS | Opacity thresholds, all four RGB-family formats, alpha rules, padded stride, origin/clip, invalid buffers, three engine failure stages |
| GE2D target build/link/image | PASS | RISC-V ELF ABI, component object provenance including the new fill probe, nine payload CRCs, 26 fixture/provenance hashes |
| Gate 1 software-only build/link/image | PASS | GE-disabled profile still compiles, links and packages |
| New board pixel probes | NOT_RUN | 12 cases: three formats x four opacities; no flashing performed |
| App/OS refactor panel/touch regression | NOT_RUN | Earlier hardware observations do not certify these images |

Commands from the isolated SDK worktree:

    cmake --build output/lvgl-host-app -j8
    ctest --test-dir output/lvgl-host-app --output-on-failure
    & ./application/rt-thread/lvgl-aic-smoke/third_party/lvgl-aic/tools/sdk/build.ps1 -Phase ge2d -Jobs 8 -AllowComponentDirty
    & ./application/rt-thread/lvgl-aic-smoke/third_party/lvgl-aic/tools/sdk/build.ps1 -Phase gate1 -Jobs 8 -AllowComponentDirty

The candidate component commit differs from the SDK's committed submodule pin;
AllowComponentDirty is explicit for this local development integration. Both
manifests record a clean component at 1623dda. The SDK gitlink is not promoted
to an unpublished component commit. No Hangcha checkout/config changes occurred.

Evidence is under output/lvgl-evidence/ge2d and output/lvgl-evidence/gate1:
manifest.json, source patches, configuration, build logs, ELF/map and images.
Each image has the standard d13x_D50T-2-Lite_page_2k_block_128k_v1.0.0.img name.

- GE2D, 1823232 bytes: SHA256 3c872f8566608049862c8aae2188308273146196d2f4878c7f6e23274b857b52.
- Gate 1, 1550848 bytes: SHA256 872b1ba9eef05e35823f6e7fd360dbb6ad952ca8e9a53e3ec4d61ac68ccab460.

Builds still print existing tool-environment warnings (short argument and missing
pywin32); both complete successfully. No new component compiler warnings remain.
The host contract validates descriptors/failure behavior, not real blend pixels
or cache coherency. Partial ARGB destination composition and paired performance
remain open; subsequent priority is decoded-image cache and memory inputs.

## Historical status

This file is intentionally explicit about unverified work.

| Date | Commit | SDK commit | LVGL commit | Board | Result | Notes |
|---|---|---|---|---|---|---|
| 2026-09-24 | `531cb8138b0ae60445814ba671a26c2607a7f3fb` | `c5807f9e7d18292f920dafaa018b8174635085c4` | `80ca777e37a2b176770726a02e07a6fb79ef0b39` | none | partial | External LVGL 9.6 host configure/build and CTest smoke test PASS; real D13x target build and board validation pending |
| 2026-09-24 | `e59ca1f1f750e445741acdeb5297cd70052b8616` | `c5807f9e7d18292f920dafaa018b8174635085c4` | `80ca777e37a2b176770726a02e07a6fb79ef0b39` | none | partial | Optional `lvgl-aic-sdl-smoke` SDL2 host target and mouse self-test PASS; this is not hardware evidence |
| 2026-09-24 | `cb1691519ccb7aa377a2f43938b1a423dc5ab837` | `c5807f9e7d18292f920dafaa018b8174635085c4` | `80ca777e37a2b176770726a02e07a6fb79ef0b39` | none | partial | Official LVGL 9.6 widgets/benchmark/stress/music/keypad demos added and passed in the 800x480 SDL host; no hardware claim |
| 2026-09-24 | `d1492bf7377b056c66656e166847f4d81b2ec7b4` | `c5807f9e7d18292f920dafaa018b8174635085c4` | `80ca777e37a2b176770726a02e07a6fb79ef0b39` | D133ECS / D50T-2-Lite | baseline PASS (provisional) | User-flashed image `05DDBA327C6026E50C23445B48EDE29EBAE3BD0EF55D4DCDB29F8670A3690EE1`; semaphore/lifecycle/first-frame logs observed; 800x480 software-rendered page and GT911 manual touch confirmed. RGB mirror was explicitly disabled. Raw-coordinate, VSync/PAN counter, cache-stress, rotation-variant, and long-run evidence is deferred to the next phase. |
| 2026-09-26 | `c151970` + working tree (see Phase 2B closeout) | `3b135eda4eaf91e6d0607d37ca24551b836ca725` | `80ca777e37a2b176770726a02e07a6fb79ef0b39` | none | partial | Phase 2B closeout: CMA lifecycle counters and an alpha-observable manual page. Host 9/9 CTest PASS; target build plus both static gates PASS on image `a5e1275b4c23811a0da0bf2c619f29f1adf23c765279b37ba2389d922a122d2c`. Board confirmation of items 1.1/1.2 and the Gate 1 display/touch regression check are still pending, so Gate 2 stays open. |
| 2026-09-27 | `cce04be` + working tree (see Phase 3A closeout) | `4d065e442de54a011b17208468ebcf6eae348297` | `80ca777e37a2b176770726a02e07a6fb79ef0b39` | D133ECS / D50T-2-Lite | fail | Phase 3A board run 1 of image `31ae0db5c965c99df9d195adc1d59d7fa664e4d13042eb4265f0e8a1384a630f`: lifecycle, all MPP fixtures, 1000 decode/close cycles and balanced CMA PASS, but the GE2D unit declined every task. Root-caused to a deinit/init bug in `lv_draw_aic_ge2d_init()` (the `g_ge2d_registered` guard skipped `mpp_ge_open()` after the first deinit), not to the driver. Fixed and rebuilt as `f79f5531c3b4b1c5637773fb4d5130af2d4b2cc60473d03609147125bc657416`; both static gates and the 9/9 host suite pass again. Criteria 6 and 8 confirmed on hardware, 2/3/4/7 pending the re-flash. Gate 3 stays open. |
| 2026-09-27 | `a7002a3` | `312ec07a` | `80ca777e37a2b176770726a02e07a6fb79ef0b39` | D133ECS / D50T-2-Lite | **PASS** | Phase 3A board run 2 of image `f79f5531c3b4b1c5637773fb4d5130af2d4b2cc60473d03609147125bc657416`: `ge2d fill accepted=10 completed=10 fallback=5 errors=0` and `PASS GE2D opaque fill: 10 rectangles accelerated, 5 fell back to software`. Criteria 1, 2, 3, 4, 6, 7 and 8 are board-confirmed. Criteria 5 (no screen corruption) and 9 (touch interaction) still need a human at the panel, so Gate 3 stays open. |
| 2026-09-27 | `a7002a3` + docs | `bf56a184` | `80ca777e37a2b176770726a02e07a6fb79ef0b39` | D133ECS / D50T-2-Lite | **PASS (closed)** | Phase 3A closeout. The operator confirmed the two panel-only criteria after run 2: no screen corruption (5) and touch interaction still works (9). **All nine completion criteria are board-confirmed** and Phase 3A is closed. Criteria 5 and 9 are operator judgements, not measurements - no pixel diff was captured. Phase 3B (IMAGE + LAYER) is planned separately. |
| 2026-09-27 | `3efc79d` + docs (see Phase 3B closeout) | `15e67f67` | `80ca777e37a2b176770726a02e07a6fb79ef0b39` | none | partial | Phase 3B IMAGE + LAYER: the GE2D unit blits untransformed images and composites layers through the same code path. Host 9/9 CTest PASS (13.56 s); target build plus both static gates PASS on image `ad32c8540a640c71c25bae6ddf06b1835eec773937c4bf9dae997203e0e5f18b`. **No board run.** The IMAGE half is expected to be engine-drawn; the LAYER composite is expected to fall back to software, because LVGL allocates layer buffers from the RT-Thread system heap at `0x30040000`, below the GE address window. See the Phase 3B closeout. |
| 2026-09-27 | `020d944` | `761015b9` | `80ca777e37a2b176770726a02e07a6fb79ef0b39` | D133ECS / D50T-2-Lite | partial | Phase 3B board run of image `ad32c8540a640c71c25bae6ddf06b1835eec773937c4bf9dae997203e0e5f18b`: `ge2d accepted fill=11 image=3 layer=1 \| engine fill=11 image=3 layer=0` and `PASS GE2D: 11 fills and 3 images drawn by the engine; 1 layer task(s) claimed, 0 drawn by the engine`. **Criteria 1-9 are board-confirmed**, including criterion 4 - every claimed IMAGE task was drawn by the engine, so the ARGB8888 `GE_PD_SRC_OVER` blend really ran on the GE2D and not merely in software. The single LAYER composite fell back to software exactly as predicted. Criteria 10 and 11 are panel questions and are still open. Both log lines were captured truncated; the PASS verdict implies `errors=0` and `declined>=1`, because the check prints FAIL and returns non-zero otherwise. |
| 2026-09-27 | `9483d98` + docs | `089806a5` | `80ca777e37a2b176770726a02e07a6fb79ef0b39` | D133ECS / D50T-2-Lite | **PASS (closed)** | Phase 3B closeout. The operator confirmed the two panel-only criteria after the board run: `c.png`'s alpha ramp shows the white swatch through its transparent corner and blue at the opaque corner, `b.png` has correct channel order, the 50% layer rectangle is a uniform dark slate blue, and touch still works. **All eleven completion criteria are board-confirmed** and Phase 3B is closed. Criteria 10 and 11 are operator judgements, not measurements - no pixel diff was captured. The LAYER composite is board-confirmed as claimed-and-composited-in-software, never as engine-drawn. |
| 2026-09-27 | `e83758e` | `fb3cbdbf` | `80ca777e37a2b176770726a02e07a6fb79ef0b39` | D133ECS / D50T-2-Lite | **PASS** | Phase 3C1 board run 2 of image `0ebd4b7d6be789befe7103aa469922e25d907a8c7507561a4cd8c44bf48d17ee`: `ge2d accepted fill=13 image=9 layer=1 \| engine fill=13 image=9 layer=0` and `PASS GE2D: 13 fills and 9 images drawn by the engine; 1 layer task(s) claimed, 0 drawn by the engine`. The blend probe matched LVGL's arithmetic at **0/255** on all three straight-alpha cases, and the must-fail premultiplied cross-check missed by **100/255** - exactly the predicted amount, 50x the tolerance. Criteria 1-4 are board-confirmed; criterion 5's display half is confirmed by the log, its touch half still needs the operator. |

## Required Phase 1 evidence

- [x] LVGL 9.6 compile
- [x] LVGL 9.6 link
- [x] framebuffer display — user-confirmed 800x480 baseline
- [x] 800x480 software-rendered surface — user-confirmed baseline
- [x] touch read callback — user-confirmed GT911 manual interaction
- [x] continuous refresh — moving marker remained live
- [x] VSync/PAN behavior — port path exercised; formal counters deferred
- [x] rotation behavior or documented limitation — 0° baseline; angle variants deferred
- [x] cache coherency check — moving-marker baseline; stress measurement deferred
- [x] no legacy `lvgl-ui` dependency
- [x] no LVGL upstream modification

The closeout treats the user-confirmed board smoke as the Phase 1 baseline.
Items marked as deferred are not claimed as independent measurements; they are
intentionally carried into the next phase.

## Host build evidence

The following checks were run against an external temporary checkout of the
pinned LVGL source; no LVGL source is stored in this repository. The
reusable command is documented in [`tests/host/README.md`](../tests/host/README.md):

- LVGL 9.6.0 CMake configure/build: PASS (external checkout, 2026-09-24);
- `lvgl_aic` public API and Phase 1 port sources: PASS;
- platform-only manual smoke page: PASS;
- CTest `lvgl_aic_platform_smoke`: PASS (1/1);
- optional `lvgl-aic-sdl-smoke` SDL2 build: PASS (MSYS2 UCRT64, SDL2 2.32.10);
- SDL2 CTest `lvgl_aic_sdl_smoke_self`: PASS (2/2 total with the headless smoke);
- SDL2 screenshot and injected mouse click verified the shared 800x480 manual page;
- official LVGL 9.6 demo modes `widgets`, `benchmark`, `stress`, `music`, and
  `keypad_encoder`: PASS; bounded CTest coverage is 7/7 with
  `AIC_BUILD_SDL_DEMOS=ON`;
- 800x480 screenshots captured the upstream widgets, benchmark, stress, music,
  and keypad/encoder layouts; vector/GLTF and legacy ArtInChip demos remain
  intentionally excluded;
- host component targets use `-Wall -Wextra -Werror` on GCC/Clang;
- `.github/workflows/host.yml` checks out the pinned LVGL commit and runs the
  same host smoke test;
- compile-time rejection of the repository's LVGL 9.1 header: PASS;
- AIC BSP target compile: PASS with the real D13x toolchain and board smoke
  described below; host stubs are not counted as target evidence.

The full LVGL upstream build emits existing MSVC code-page and enum warnings;
those warnings are not attributed to `lvgl-aic`. The component sources were
additionally checked with `-Wall -Wextra -Werror` using the target-facing
stubs.

## Phase 1.5 target build evidence

The real target build was run in the isolated Luban-Lite integration checkout,
not in the parent working tree with its unrelated uncommitted changes:

- SDK baseline: `c5807f9e7d18292f920dafaa018b8174635085c4`;
- LVGL: `80ca777e37a2b176770726a02e07a6fb79ef0b39` (`v9.6.0`);
- component: `d1492bf7377b056c66656e166847f4d81b2ec7b4`;
- application defconfig:
  `d13x_d50t-2-lite_rt-thread_lvgl-aic-smoke_defconfig`;
- bootloader prerequisite defconfig:
  `d13x_d50t-2-lite_baremetal_bootloader_defconfig`;
- toolchain: Xuantie-900 GCC V2.6.1 B-20220906, GCC 10.2.0;
- bootloader `d13x.elf`: PASS;
- application `d13x.elf`: PASS, 9,030,912 bytes;
- application image generation: PASS;
- image: `output/d13x_d50t-2-lite_rt-thread_lvgl-aic-smoke/images/d13x_D50T-2-Lite_page_2k_block_128k_v1.0.0.img`, 1,505,792 bytes;
- image SHA256: `05DDBA327C6026E50C23445B48EDE29EBAE3BD0EF55D4DCDB29F8670A3690EE1`;
- RGB panel alignment: both D50T smoke and bootloader defconfigs explicitly set
  `# CONFIG_RGB_DATA_MIRROT is not set` and `CONFIG_AIC_RGB_DATA_MIRROR=0`;
- static link-map check: PASS (`lv_init`, `lv_display_create`, and
  `lv_obj_create` resolve to `packages/third-party/lvgl/src`; no
  `packages/artinchip/lvgl-ui` or `lvgl_v9/lvgl` object appears);
- compile-time `lv_conf.h` marker probe: PASS;
- Kconfig touch symbol: generated as
  `#define AIC_LVGL_TOUCH_DEVICE "gt911"`;
- forbidden Phase 1 symbols: `AIC_LVGL_USE_GE2D`,
  `AIC_LVGL_USE_MPP_DEC`, and `AIC_LVGL_USE_FT_CACHE` remain disabled.

The target build enables `LPKG_MPP` only for framebuffer/BSP infrastructure;
it does not add an LVGL MPP image decoder or GE2D implementation. The build
uses the platform-only smoke application, not the D50T product UI.

The only compiler diagnostics observed in the LVGL OSAL are pre-existing
`%d`/`rt_err_t` format warnings from upstream `lv_rtthread.c`; the build is
not warning-clean because the pinned upstream source is intentionally not
modified.

## Hardware closeout (2026-09-24)

A D133ECS / D50T-2-Lite board was flashed with the mirror-disabled image:

```text
output/d13x_d50t-2-lite_rt-thread_lvgl-aic-smoke/images/d13x_D50T-2-Lite_page_2k_block_128k_v1.0.0.img
SHA256: 05DDBA327C6026E50C23445B48EDE29EBAE3BD0EF55D4DCDB29F8670A3690EE1
```

The board produced the semaphore vlimit self-test pass, three lifecycle passes,
and the first software-rendered frame log. The user additionally confirmed the
800x480 smoke page, corrected colors, live marker animation, and GT911 manual
button interaction. The `/sdcard` mount warning is unrelated to this display
and touch path.

This closes the current smoke baseline provisionally. The following independent
measurements were not performed in this closeout and are deferred to the next
phase rather than claimed as PASS:

- raw and scaled DOWN/MOVE/UP coordinate records;
- four-corner/direct-fill color-block evidence;
- PAN/VSync counters or logic-analyzer timing;
- cache-stress animation and framebuffer coherency evidence;
- 90/180/270-degree rotation variants;
- 30–60 minute long-run, heap/PSRAM/thread stability measurements.

## Hardware policy

For this closeout, report:

```text
Build validation: PASS
Hardware baseline: PASS (user-confirmed, provisional)
Extended diagnostics: DEFERRED
```

Do not convert the deferred items into independent hardware claims.

## Gate 1 closeout revisions (Phase 2 entry)

- `lvgl-aic` Gate 1 code: `d1492bf7377b056c66656e166847f4d81b2ec7b4`
- `lvgl-aic` Gate 1 closeout docs: `f90f5e067e8606ce82c7a542eb566827d510a8b9`
- `lvgl-aic` tag: `v0.1.0` on `f90f5e0` (pushed to `boa-z/lvgl-aic` on
  2026-09-24 with deploy token; `f90f5e0` == `origin/main` at tag time)
- parent/superproject: `f7572509111d1c70962e6347e5ad7bc87b77fbba`
  (`codex/d50t-meter-adaptation`, pushed to `boa-w/luban-lite-jc-d50t-rev`
  on 2026-09-24)
- LVGL: `80ca777e37a2b176770726a02e07a6fb79ef0b39` (`v9.6.0`)
- Luban-Lite SDK baseline: `c5807f9e7d18292f920dafaa018b8174635085c4`
- verified image SHA256:
  `05DDBA327C6026E50C23445B48EDE29EBAE3BD0EF55D4DCDB29F8670A3690EE1`

Phase 2 work starts from `phase2-mpp` branched at `v0.1.0`. Gate 1 SW
baseline (`GE2D=OFF`, `MPP_DEC=OFF`, `FT_CACHE=OFF`) must remain intact
until Phase 2 gates replace it.

## Phase 2A status (code complete, board validation pending)

Implemented on `phase2-mpp` after `v0.1.0`:

- `feat(image): establish AIC MPP decoder boundary`
- `feat(image): add MPP JPEG decoding for LVGL 9.6`
- `feat(image): add MPP PNG and alpha decoding`
- `fix(image): harden MPP buffer ownership and failure cleanup`
- `test(image): add MPP decoder lifecycle and board tests`

Design (enforced by construction, not just review):

- `GE2D` remains `OFF`; `image/mpp` includes no GE2D header and the MPP
  link map contains no `ge2d` object.
- Format mapping lives in `lv_aic_mpp_format.*` (`RGB565/RGB888/ARGB8888`
  only; YUV/BGR variants rejected instead of mislabeled).
- Decoder owns one `lv_image_decoder_t` via `lv_image_decoder_create`;
  `lv_aic_init` order is display -> input -> decoder with reverse teardown.
- `FILE` (`.jpg/.jpeg/.png`) only; `VARIABLE/AICP/BMP/fake` return
  `LV_RESULT_INVALID`.
- Buffers use `lv_draw_buf_init()` (valid `data/unaligned_data/handlers/
  stride/data_size`); CMA `allocation_base` is freed from the base pointer,
  and PNG post-process heap replacements are tracked as `heap_buf`.
- No custom image cache (`lv_drop_one_cached_image()` returns false).
- Decoder private structs come only via `compat/lvgl_aic_private.h`.

Verification so far:

- Host `lvgl_aic_platform_smoke`: PASS (MPP stubs, SW baseline unregressed).
- Target `d13x_d50t-2-lite_rt-thread_lvgl-aic-smoke` with `MPP_DEC=0`: PASS.
- Target same config with temporary `MPP_DEC=1`: PASS (link map shows
  `lv_aic_mpp_{decoder,format,stream}` objects, zero `ge2d` hits).
- Phase 2 validation config keeps `AIC_LVGL_USE_GE2D=n`.

Still required on hardware before any Gate 2 claim:

- Flash an `MPP_DEC=1` image and load `/data/mpp_test/{a.jpg,b.png,c.png}`.
- Confirm JPEG RGB, PNG RGB, and PNG RGBA render correctly under the SW
  renderer (color, alpha, stride, repeated refresh).
- Run corrupt/missing/zero-byte/unsupported-format cases (safe `INVALID`).
- Run A/B/C multi-image plus >= 1000 decode/release cycles and
  `lv_aic_init/deinit` repeats with heap/CMA sampled.
- Record `decode_time_ms`/buffer sizes from `lv_aic_mpp_decoder_last_stats()`.
- Re-confirm the Gate 1 display/touch baseline did not regress.

## Phase 2A review and user board log (2026-09-26)

The user supplied a D50T-2-Lite log reporting semaphore self-test, three
lifecycle cycles, all listed acceptance cases, 1000 decode/close cycles and
first software frame presented. No image SHA256 was supplied; this evidence
cannot be bound to the newly reviewed build. RT heap before=119708,
after=115396, peak=144900 is not proof of no CMA leak. The odd-width JPEG
reports 801x479, stride 2448, CMA 1175040 bytes. Visual color/alpha correctness
and touch regression still need confirmation. The /sdcard mount failure is
separate from the /data fixture filesystem.

Review retained PNG chunk CRC checks and expected rejection of 4-bit palette
files: the SDK accepts only 8-bit PNG input. No vendor decoder changes were
made. Fixed a CRC walker bounds error (chunk framing needs 12 bytes, not 8),
validated packet signature and empty IEND, and added every truncated prefix
of a valid PNG to the production-code host regression test. CRC validity is
separate from decoder format support. Adler-32 is not checked; the SDK still
swallows some PNG hardware errors, so arbitrary corrupt-stream rejection is
not established by these fixtures.

The external allocator uses SDK-provided byte stride and padded height,
preserves frame metadata, and places its allocator interface first. Disabled
feature guards use numeric values; RT-Thread empty Kconfig defines are
normalized. Removed unused session fields, consolidated fixture preconditions
and shortened stress logs to avoid ULOG truncation.

Build/evidence tools live in this repository's `tools/sdk/`. The component
SConscript tracks staged fixtures so asset changes trigger link/packing.
See `tools/sdk/README.md`. Fresh build manifests mark board validation
NOT_RUN. Gate 2 remains open pending image-bound visual, touch and CMA evidence.

Review verification: 9/9 host CTests and 22 fixture header decisions plus
PNG CRC checks passed. The Windows MPP build passed static integration,
ELF ABI, 9 firmware payload CRCs and 26 packaged fixture/provenance hashes.
An asset-only README change triggered relink/repack without recompiling C.
Detailed logs and source snapshots are in SDK build/lvgl-evidence/.

## Phase 2B closeout (code complete, board confirmation pending)

Phase 2's remaining scope was deliberately limited to two items. No additional
MPP test surface was added, and no large image corpus was introduced.

### 1.1 Real-chain display confirmation

`tests/manual/lv_aic_manual_test.c` renders the three existing fixtures through
the real chain `lv_image -> MPP decoder -> LVGL SW renderer -> framebuffer`:

| Fixture | Alias | Format | Presentation |
|---|---|---|---|
| `a.jpg` | `aic_160x120.jpg` | JPEG | 160x120, native scale |
| `b.png` | `basn2c08.png` | PNG RGB | 32x32, 4x scale |
| `c.png` | `basn6a08.png` | PNG RGBA | 32x32, 4x scale, on a 128x128 white swatch |

`c.png` was inspected offline: its IDAT carries a genuine 32-step 0..255 alpha
ramp (px(0,0)=RGBA(255,0,8,0) through px(31,31)=(0,32,255,255)), so alpha is
observable without a new fixture. On the white swatch the transparent corner
stays white while the opaque corner keeps its own colour, which makes blending
provable instead of blending into the dark page background.

Both 32x32 PNGs use `lv_image_set_pivot(img, 0, 0)` with
`lv_image_set_scale(img, 1024)`. In LVGL 9.6 `scale_update()` refreshes only the
extended draw size; the object's own size stays 32x32, so the scaled image grows
right/down from the object's top-left. The swatch and the image therefore share
`lv_obj_set_pos(..., 24 + i * 220, 240)` and line up exactly.

### 1.2 CMA lifecycle counters

The RT heap was already healthy; the open question was whether the decoder's CMA
buffers leak. Instead of a general memory profiler, the wrapper now accounts for
its own `MEM_CMA` traffic in `image/mpp/lv_aic_mpp_decoder.c`:

- one symmetric helper pair, `lv_aic_mpp_cma_alloc()` / `lv_aic_mpp_cma_free()`,
  through which every `MEM_CMA` site now routes: `lv_aic_mpp_alloc_ext_frame`,
  `lv_aic_mpp_session_release`, and the PNG post-process heap-swap path;
- `lv_aic_mpp_cma_stats_t { current_cma_bytes, peak_cma_bytes, alloc_count,
  free_count }`, exposed as `lv_aic_mpp_cma_stats()` /
  `lv_aic_mpp_cma_stats_reset()` in `lv_aic_mpp_decoder.h`.

Because both directions go through the same pair, `alloc_count == free_count`
holds by construction and `current_cma_bytes` is exact for the wrapper's own
buffers. The heap-swap path now frees before clearing `session->cma_size`, so the
counter sees the real size. The counters are debug-only and do not change
allocation behaviour.

Board assertion (`tests/manual/lv_aic_mpp_test.c`): the stress loop resets the
counters first, so the verdict covers exactly the 1000 decode/close cycles, then
requires `current_cma_bytes == 0`, `alloc_count == free_count`, and
`alloc_count >= 1000`. The last guard is anti-vacuity: a bypassed allocator would
otherwise make the balance check trivially true. The RT heap before/after/peak
log stays diagnostic only.

Scope caveat: these counters cover only this wrapper's own `MEM_CMA` buffers. CMA
held inside the SDK MPP engine is not visible here, and this is not a general
memory profiler.

### Verification so far (host and target, no board claim)

Host, `build/lvgl-host` (external LVGL checkout, prior cache parameters):

- `ctest`: 9/9 PASS (12.35 s);
- `lvgl_aic_mpp_contract` compiles the production decoder with
  `-Wall -Wextra -Werror` and now asserts the CMA counters: modes 0 and 2 each
  allocate once and release once, while the failed-allocation and rejected-stride
  modes must not move the counters at all, so `alloc_count == 2 == free_count`,
  `current_cma_bytes == 0`, `peak_cma_bytes == 2448 * 480`, and a reset returns
  every field to zero. Final line: `PASS: MPP allocator ABI, padded JPEG
  stride/height, failure cleanup, CMA lifecycle counters, PNG CRC gate`;
- `gcc -fsyntax-only -Wall -Wextra -Werror` on `lv_aic_manual_test.c` with
  `AIC_LVGL_USE_MPP_DEC=1 AIC_LVGL_BSP_MPP=1`: PASS (the MPP block is excluded
  from the host build);
- `tests/data/mpp/check_headers.py`: all 22 header decisions match
  `lv_aic_mpp_decoder.c`, and all 3 corrupt-CRC fixtures are still corrupt.

Target, built with the project's own entry point
`packages/custom/lvgl-aic/tools/sdk/build.sh` (`PHASE=mpp`,
`ALLOW_COMPONENT_DIRTY=1`):

- `mpp static checks: PASS (not board validation)`, all 10 required symbols
  resolving to their required objects;
- `verify_image.py`: application and bootloader ELF32 RISC-V double-float ABI
  PASS, 9 payload CRCs PASS, 26 packaged MPP fixture/provenance hashes PASS;
- link map contains 0 `ge2d` hits; the only MPP objects are
  `lv_aic_mpp_{decoder,format,stream}.o`;
- `.config` keeps `CONFIG_AIC_LVGL_USE_MPP_DEC=y` and
  `# CONFIG_AIC_LVGL_USE_GE2D is not set`; `rtconfig.h` defines
  `AIC_LVGL_USE_MPP_DEC` and no GE2D or FT_CACHE symbol;
- image
  `output/d13x_d50t-2-lite_rt-thread_lvgl-aic-mpp/images/d13x_D50T-2-Lite_page_2k_block_128k_v1.0.0.img`,
  1,786,368 bytes, SHA256
  `a5e1275b4c23811a0da0bf2c619f29f1adf23c765279b37ba2389d922a122d2c`.

Both gates were re-run against that exact artifact after the build; the results
above come from that re-run, not from the earlier session.

Closeout diff over `c151970` (+139/-12): `image/mpp/lv_aic_mpp_decoder.c`,
`image/mpp/lv_aic_mpp_decoder.h`, `tests/host/mpp_contract.c`,
`tests/manual/lv_aic_mpp_test.c`, `tests/manual/lv_aic_manual_test.c`,
`tests/manual/README.md`.

### Still required on hardware (not claimed)

- Flash the image above and load `/data/mpp_test/{a.jpg,b.png,c.png}`.
- 1.1: JPEG displays normally, RGB PNG colours normal, RGBA PNG alpha normal
  against the white swatch.
- 1.2: the board log shows `CMA current=0` with `alloc == free >= 1000` after the
  stress loop.
- The Gate 1 display/touch baseline did not regress.

Gate 2 remains open until those board results exist. They cannot be derived from
the host, the link map, or the image CRCs.

### Release checklist (only after the board run passes)

1. Bump `include/lvgl_aic_version.h` to `0.2.0` and commit the closeout diff on
   `phase2-mpp`. That header is not `#include`d anywhere, so it cannot change the
   image or its SHA256.
2. Pin the new component commit in the superproject. `check_integration.py` line
   130 compares the component HEAD with the superproject gitlink, so committing
   the component without updating the pin fails the static gate.
3. Regenerate the evidence bundle in one run of the Windows-native entry
   `tools/sdk/build.ps1 -Phase mpp`, so that
   `build/lvgl-evidence/mpp/{images,manifest.json,*.log}` all describe the same
   build. The bundle currently on disk binds to the earlier clean `c151970`
   build (image `0a5d62ff01fad7f88bb06c545e981e2e5876892fc95c9484c9d3634ca5470061`)
   and does not describe the artifact above.
4. Merge `phase2-mpp` into `main` (`origin/main` is at `f90f5e0`) and tag
   `v0.2.0`.

## Phase 3A closeout (closed: all nine criteria board-confirmed)

Branch `phase3-ge2d`, cut from the Phase 2 closeout. Scope is exactly one task
type: `LV_DRAW_TASK_TYPE_FILL`, opaque, `radius == 0`, no gradient, supported
destination format, GE-addressable buffer. Everything else is left to the
software renderer on purpose. IMAGE, LAYER, scale, rotation and an asynchronous
render thread are out of scope and were not started.

### What changed

New:

- `common/lv_aic_pixel_format.{c,h}` - the single LVGL <-> MPP format
  translation. The decoder and the GE2D unit both need it, so it belongs to
  neither. `lv_aic_pixel_format_is_ge2d_dst()` holds the GE2D destination
  policy;
- `draw/ge2d/lv_draw_aic_ge2d.{c,h}` - the draw unit: registration, `evaluate`,
  `dispatch`, `delete`, counters. `lv_draw_aic_ge2d_unit_t` is an independent
  struct (`base_unit` + `task_act`); it does **not** reuse `lv_draw_sw_unit_t`,
  and it never touches `LV_DRAW_TASK_STATE_READY` or a saved
  `target_layer`/`clip_area` on the unit;
- `draw/ge2d/lv_draw_aic_ge2d_fill.c` - one opaque `ge_fillrect` ->
  `mpp_ge_emit` -> `mpp_ge_sync`, all three return codes checked;
- `draw/ge2d/lv_draw_aic_ge2d_utils.{c,h}` - GE address-window check
  (`>= 0x40000000` on D13x/G73x), destination format check, and explicit
  destination cache preparation for the touched region only;
- `tests/manual/lv_aic_ge2d_test.c` - finite board check, `lv_aic_ge2d_test_run()`.

Changed:

- `image/mpp/lv_aic_mpp_format.c` now delegates to the shared mapper. Its
  accepted set is unchanged: `to_lvgl()` still rejects XRGB8888 and the BGR
  family, so Phase 2 decoder behaviour is byte-identical;
- `port/lv_aic.c` calls `lv_draw_aic_ge2d_init()` last (so no earlier failure
  can leave the GE device open) and `lv_draw_aic_ge2d_deinit()` before the
  display goes away;
- `tests/manual/lv_aic_manual_test.{c,h}` add the Phase 3A page: one large, one
  medium and six small opaque rectangles (GE2D), plus one rounded rectangle that
  must fall back;
- `lv_conf.h` - see the trap below;
- `SConscript`, `tests/host/CMakeLists.txt`, `tools/sdk/*` and
  `target/configs/d13x_d50t-2-lite_rt-thread_lvgl-aic-ge2d_defconfig` wire the
  new sources and a `ge2d` build phase into the existing entry points.

### Design points worth keeping

- `evaluate()` returns 1 when it accepts a task. The vendor port returned 0;
  LVGL 9.6 ignores the return value but the intent is now readable.
- A GE failure marks the task `LV_DRAW_TASK_STATE_FAILED`, never `FINISHED`. A
  rectangle the engine did not draw must not be reported as drawn.
- The unit is gated on `mpp_ge_open()`. If the device is unavailable the unit is
  still registered and declines every task, so software rendering keeps the
  display alive instead of the unit dispatching into a NULL device. The
  2026-09-27 board run exercised exactly this path.
- `header.stride` is authoritative and is never assumed to equal `width * bpp`.
- The destination cache is prepared inside the backend for the touched region
  only. The global LVGL draw-buffer handlers are deliberately left alone, so the
  rest of LVGL keeps its own cache policy.
- Preference score is 70. The software unit claims at `>= 100`, so 70 wins, and
  a task GE2D declines still reaches the software renderer.
- `dispatch()` must keep the explicit `preferred_draw_unit_id !=
  AIC_GE2D_DRAW_UNIT_ID` check: `lv_draw_get_available_task()` also returns tasks
  whose `preferred_draw_unit_id` is `LV_DRAW_UNIT_NONE`, and dropping the check
  would dispatch another backend's work.

### Trap found and fixed while bringing this up

`rtconfig.h` emits an enabled `bool` as a *bare* `#define AIC_LVGL_USE_GE2D`.
A bare define makes `#if AIC_LVGL_USE_GE2D` a hard compile error
(`#if with no expression`), not a false branch. `lv_conf.h` already normalized
`AIC_LVGL_USE_MPP_DEC`, `AIC_LVGL_USE_TOUCH` and `AIC_LVGL_USE_DISPLAY` to `1`
for exactly this reason; `AIC_LVGL_USE_GE2D` had been missed. Any future
`AIC_LVGL_USE_*` symbol read with `#if` must be added to that block.

### Verification so far (host and target, no board claim)

Host, `build/lvgl-host` (external LVGL checkout):

- `cmake --build` clean under `-Wall -Wextra -Werror` for `lvgl_aic`,
  `lvgl_aic_smoke`, `lvgl_aic_mpp_contract` and `lvgl_aic_disabled_features`;
- `ctest`: 9/9 PASS in 11.82 s. The SDL tests need `/c/msys64/ucrt64/bin` on
  `PATH` for `SDL2.dll`; without it they fail with `0xc0000135`
  (STATUS_DLL_NOT_FOUND), which is environmental.

Target, built with the project's own entry point
`packages/custom/lvgl-aic/tools/sdk/build.sh` (`PHASE=ge2d`,
`ALLOW_COMPONENT_DIRTY=1`):

- the five new/changed translation units compile with no warning attributable to
  them (`lv_aic_pixel_format.c`, `lv_draw_aic_ge2d.c`,
  `lv_draw_aic_ge2d_fill.c`, `lv_draw_aic_ge2d_utils.c`, `lv_aic_mpp_format.c`);
- `ge2d static checks: PASS (not board validation)`: all 10 base symbols plus
  `lv_draw_aic_ge2d_init`, `lv_draw_aic_ge2d_fill` and `lv_aic_ge2d_test_run`
  resolve to their required objects;
- `verify_image.py`: application and bootloader ELF32 RISC-V double-float ABI
  PASS, 9 payload CRCs PASS, 26 packaged MPP fixture/provenance hashes PASS;
- link map: our objects are `common/lv_aic_pixel_format.o`,
  `draw/ge2d/lv_draw_aic_ge2d{,_fill,_utils}.o`,
  `tests/manual/lv_aic_ge2d_test.o`; the engine entry points `mpp_ge_open`,
  `mpp_ge_fillrect`, `mpp_ge_emit` and `mpp_ge_sync` are all present, so the
  backend really does call the GE driver; 0 `lvgl-ui`/`lvgl_v9` hits;
- `.config` has `CONFIG_AIC_LVGL_USE_GE2D=y` **and**
  `CONFIG_AIC_LVGL_USE_MPP_DEC=y`, so the MPP regression is observable on the
  same image; `CONFIG_AIC_GE_DRV=y`, `CONFIG_AIC_GE_DRV_V11=y`,
  `CONFIG_AIC_GE_CMDQ=y`;
- image
  `output/d13x_d50t-2-lite_rt-thread_lvgl-aic-ge2d/images/d13x_D50T-2-Lite_page_2k_block_128k_v1.0.0.img`,
  1,802,752 bytes, SHA256
  `31ae0db5c965c99df9d195adc1d59d7fa664e4d13042eb4265f0e8a1384a630f`.
  Recompiling all five units from scratch reproduced a byte-identical image.

### Board result (2026-09-27): GE2D bring-up failed, root-caused and fixed

The image above was flashed to the D50T-2-Lite board. The run split cleanly.

Confirmed on hardware:

- display and touch come up, and the smoke page presents a first frame with the
  scheduler still progressing (`first frame presented; scheduler is still
  progressing`), so there is no scheduler stall;
- the MPP decoder is not regressed: every fixture PASSes (including the
  interlace/bit-depth rejection cases and the corrupt-CRC gate), 1000
  decode/close cycles complete, and CMA stays balanced (`current=0 peak=61440
  alloc=1000 free=1000`).

Failed:

- `E/lvgl.ge2d.test: FAIL GE2D device unavailable: mpp_ge_open() failed; check
  the GE driver`. The unit therefore declined every task - the designed
  behaviour for an unavailable device - and the page rendered entirely in
  software. Criteria 2, 3, 4 and 7 are unverified as a result, and criterion 5
  is only trivially true because GE2D drew nothing.

The root cause was in the draw unit itself, not in the driver. The full boot log
shows `hal_ge_init()` running at 0.515 (`[I]hal_ge_init()342 cmd queue hal, cmdq
buffer size = 2048`), and **no `mpp_ge_open()` failure message appears anywhere**
- yet `mpp_ge_open()` has three failure exits and every one of them prints.

`lv_draw_aic_ge2d_init()` began with:

```c
if (g_ge2d_registered) {
    return;                 /* <-- skipped mpp_ge_open() on re-init */
}
if (g_ge2d_dev == NULL) {
    g_ge2d_dev = mpp_ge_open();
}
```

The smoke application runs three lifecycle cycles and then a fourth init, so
`lv_aic_init()` runs four times and `lv_aic_deinit()` three times.
`lv_draw_aic_ge2d_deinit()` closes the device and clears `g_ge2d_dev`, but it
cannot clear `g_ge2d_registered` - LVGL has no API to unregister a draw unit, so
the unit stays in the draw-unit list by design. The guard therefore made the
second init a no-op, leaving the device closed and `g_ge2d_ready` false for the
rest of the boot. GE2D was never exercised at all, and the "device unavailable"
message was a misleading diagnosis derived from the `ready` flag.

Fixed by opening the device on every init and guarding only the unit creation:

```c
if (g_ge2d_dev == NULL) {
    g_ge2d_dev = mpp_ge_open();
}
g_ge2d_ready = (g_ge2d_dev != NULL);
g_ge2d_stats.ready = g_ge2d_ready;
if (!g_ge2d_ready) {
    LV_LOG_ERROR("GE2D device unavailable; ...");
}
if (g_ge2d_registered) {
    return;                 /* device reopened; unit already listed */
}
/* ... create the unit ... */
```

Confirmed in the linked image: in `lv_draw_aic_ge2d_init`, the branch to
`mpp_ge_open` (`0x40083b5a`) is taken before any load of `g_ge2d_registered`
(`0x40083b08`), and the failure path calls `lv_log_add` before falling through to
the registration check.

This was not a test-only defect: any deinit/init cycle would silently disable
GE2D acceleration for the remainder of the run, and the product UI re-initialises
LVGL on the same path.

Rebuilt image
`f79f5531c3b4b1c5637773fb4d5130af2d4b2cc60473d03609147125bc657416`
(1,802,752 bytes): both static gates PASS again and the host suite is 9/9 PASS.

### Board result 2 (2026-09-27): PASS

The rebuilt image was flashed. The GE2D section of the console:

```text
[   2.367] I/lvgl.ge2d.test: ge2d fill accepted=10 completed=10 fallback=5 errors=0
[   2.367] I/lvgl.ge2d.test: PASS GE2D opaque fill: 10 rectangles accelerated, 5 fell back to software
[   2.382] I/lvgl.aic.smoke: LVGL 9.6 smoke page is running
[   2.388] I/lvgl.aic.smoke: first frame presented; scheduler is still progressing
```

The fix worked, and criteria 2, 3, 4 and 7 are now decided: the unit claimed
opaque fills, completed every one it claimed, declined the unsupported ones, and
reported no execution error.

`accepted=10` is a clean census rather than a coincidence. LVGL calls each
unit's `evaluate_cb` exactly once per task, at task-creation time
(`lv_draw_finalize_task_creation()` in `src/draw/lv_draw.c`), and the unit
returns before incrementing any counter for non-`FILL` types. The page sets
`bg_opa = LV_OPA_COVER` on twelve objects, ten of which have `radius == 0`: the
page root (800x480), the MPP alpha swatch (128x128), the large (280x56) and
medium (120x56) rectangles, and the six 28x28 squares. Ten accepted, and
`completed == accepted`, so every claimed rectangle was actually drawn by the
engine.

Only eight of those ten are Phase 3A test shapes: the unit also claimed the page
background and the MPP white swatch without being asked. The board check's
`>= 8` guard is deliberately below the observed value for exactly this reason.

`fallback=5` is higher than the two rounded objects the page declares
(`lv_aic_manual_marker`, radius 16; `lv_aic_ge2d_round`, radius 18). The other
three are additional `FILL` tasks LVGL submitted on this page that Phase 3A
declines by design; they are not individually identified here. Naming them would
need a debug line in `evaluate()` plus another board cycle, and no completion
criterion depends on the exact number - the board check asserts only
`fallback >= 1`, because the count is a property of the page rather than of the
unit. Recorded as an open observation, not a defect.

The `Built on Sep 27 2026 01:13:46` banner is identical in run 1 and run 2 and
is **not** evidence of a stale flash. `kernel/rt-thread/src/kservice.c` prints
`__DATE__ __TIME__`, so the string is frozen at the moment that one translation
unit was compiled: `kservice.o` is stamped `01:13:46` while
`lv_draw_aic_ge2d.o` is stamped `01:52:27`, and the flashed `d13x.elf` and image
are stamped `01:52`. The banner tracks `kservice.c`, not the image.

### Panel confirmation (2026-09-27): PASS

The operator confirmed the two criteria the serial log cannot decide:

- criterion 5 (no screen corruption) - the page renders correctly with GE2D
  drawing the rectangles; no corruption or tearing;
- criterion 9 (display/touch regression) - touch interaction still works.

**All nine Phase 3A completion criteria are now board-confirmed and Phase 3A is
closed.**

What came from where, so the evidence scope stays clear:

| Source | Criteria |
| --- | --- |
| Serial log, run 1 | 1, 6, 8 |
| Serial log, run 2 | 1, 2, 3, 4, 6, 7, 8 |
| Operator at the panel, after run 2 | 5, 9 |

Criteria 5 and 9 are operator judgements, not measurements: they say "the page
looked right and touch responded", not "every pixel matched a reference" or "the
touch coordinates were within N units". A machine-checkable version of either
(framebuffer capture versus a reference image; raw touch coordinates versus the
panel mapping) is separate work and is not claimed here.

## Phase 3B closeout (closed: all eleven criteria board-confirmed)

Branch `phase3-ge2d`, continuing from the Phase 3A closeout. Scope is two more
task types on the draw unit Phase 3A built:

```text
LV_DRAW_TASK_TYPE_IMAGE   untransformed, untiled, unrecolored, no mask
LV_DRAW_TASK_TYPE_LAYER   the same blit, fed from a child layer's buffer
```

Scale, rotation, skew, recolor, masks, colour keys, non-normal blend modes, image
opacity and the asynchronous render thread are still out of scope and were not
started. A LAYER task is the one place a partial opacity is accepted, because
LVGL only creates a LAYER task for a transform or for a partial layer opacity
(`calculate_layer_type()` in `lv_obj_style.c`); declining it would decline nearly
every layer.

### What changed

New:

- `draw/ge2d/lv_draw_aic_ge2d_image.c` - the blit: `ge_bitblt` ->
  `mpp_ge_emit` -> `mpp_ge_sync`, all three return codes checked. It also reports
  which of three outcomes occurred: the engine drew it, the engine declined and
  software drew it, or there was nothing to draw.

Changed:

- `draw/ge2d/lv_draw_aic_ge2d.{c,h}` - `evaluate()` gained the IMAGE and LAYER
  cases; `dispatch()` routes both to the one blit entry point and counts the
  engine-versus-software split. New `image_sw_fallback` / `layer_sw_fallback`
  counters and a new `lv_draw_aic_ge2d_outcome_t`;
- `common/lv_aic_pixel_format.h` - the source-format policy now documents why
  ARGB8888 is accepted: it is blended with its own per-pixel alpha, not copied.
  Accepting it and then dropping the alpha channel would paint a transparent
  image as an opaque one;
- `tests/manual/lv_aic_manual_test.c` and `tests/manual/lv_aic_ge2d_test.c` - the
  Phase 3B probes and the extended board check.

### Reused, not reimplemented

The decode, the clip intersection and the decoder lifetime stay LVGL's: the blit
supplies a core callback to LVGL 9.6's own `lv_draw_image_normal_helper()`. The
9.1 helpers `_lv_draw_image_normal_helper()` and `_lv_draw_image_tiled_helper()`
are not ported.

A LAYER task carries an `lv_layer_t` where an IMAGE task carries its source, so
the layer's `draw_buf` is wrapped in a temporary image descriptor and the
identical blit runs - the same trick `lv_draw_sw_layer()` uses, for the same
reason. There is no second "blend a layer" implementation to drift, and no second
draw unit.

### Design points worth keeping

- The blend is chosen by operation, not by task type: `opa >= LV_OPA_COVER &&
  src_cf != ARGB8888` takes the plain-copy path, everything else takes
  `GE_PD_SRC_OVER` with `src_alpha_mode = 2` (mixed, pixel alpha x global alpha).
  `accepts_image()` still declines image opacity, so today the global-alpha half
  is reached only by LAYER - but the condition is written for the operation so it
  stays correct when Phase 3C lifts that.
- `struct ge_ctrl.alpha_en` is inverted in the D13x header comment
  (`aic_drv_ge.h` says "0: enable Porter/Duff alpha blending"). The HAL and the
  vendor port both treat non-zero as blending on. Trust the HAL; the comment is
  wrong.
- The fallback keeps the ORIGINAL descriptor, so the layer path goes through
  `lv_draw_sw_layer()` and the layer bookkeeping stays in upstream's hands.
- The image assertions in the board check compile out when the MPP decoder is
  disabled, so a GE2D-without-decoder build does not fail a check whose probes do
  not exist.

### Trap found while bringing this up: the source side of the GE address window

Phase 3A gated the **destination** at `>= 0x40000000` on D13x/G73x, matching the
vendor port (`lv_drivers/lv_ge2d/lv_draw_ge2d.c`). Phase 3B has to gate the
**source** too, because the engine reads it. That second gate is not a formality:

| Buffer | Allocation | Address | Inside the GE window? |
| --- | --- | --- | --- |
| Display draw buffer | `mpp_fb` AICFB framebuffer | PSRAM | yes |
| Decoded image | `aicos_malloc_align(MEM_CMA, ...)` | `0x400e2464` | yes |
| Layer draw buffer | `lv_draw_buf_create` -> `lv_malloc` -> `rt_malloc` | `0x30040000`-`0x30140000` | **no** |

`CONFIG_AIC_DEFAULT_SYS_HEAP_SRAM=y` puts `__heap_start` at `0x30040000` (D13x
linker script, initialised in `target/d13x/d50t-2-lite/board.c`), and
`lv_mem_core_rtthread.c` makes `lv_malloc` `rt_malloc`. So every LAYER composite
is declined after being accepted, and composited in software.

This was found while writing the board check, not on hardware. It matters because
the first version of the check would have asserted `layer_completed > 0` and
printed "N layers accelerated" - both of which would have been true statements
about the counters and false statements about the engine. Two things changed as a
result:

1. the executor now reports the outcome, and `*_sw_fallback` counts the
   hand-offs, so the engine-drawn count is `completed - sw_fallback`;
2. the board check asserts only that a LAYER task is claimed and completed, and
   reports the engine-versus-software split instead of requiring engine work.
   Requiring engine work would fail on correct code.

Moving layer buffers into CMA is the fix, and it is deliberately NOT part of this
phase: CMA is shared with the decoder and the display, so it is a memory-policy
decision. The address gate already makes the transition safe, so the blit itself
would not change.

### Verification so far (host and target, no board claim)

Host, `build/lvgl-host` (external LVGL checkout):

- `ctest`: 9/9 PASS in 13.56 s. The SDL tests need `/c/msys64/ucrt64/bin` on
  `PATH` for `SDL2.dll`; without it they fail with `0xc0000135`
  (STATUS_DLL_NOT_FOUND), which is environmental.

Target, built with the project's own entry point
`packages/custom/lvgl-aic/tools/sdk/build.sh` (`PHASE=ge2d`,
`ALLOW_COMPONENT_DIRTY=1`):

- `lv_draw_aic_ge2d.c`, `lv_draw_aic_ge2d_image.c` and both manual test files
  compile with no warning attributable to them;
- `ge2d static checks: PASS (not board validation)`: the Phase 3A symbol set plus
  `lv_draw_aic_ge2d_image` resolve to their required objects;
- `verify_image.py`: application and bootloader ELF32 RISC-V double-float ABI
  PASS, 9 payload CRCs PASS, 26 packaged MPP fixture/provenance hashes PASS;
- image
  `output/d13x_d50t-2-lite_rt-thread_lvgl-aic-ge2d/images/d13x_D50T-2-Lite_page_2k_block_128k_v1.0.0.img`,
  1,804,800 bytes, SHA256
  `ad32c8540a640c71c25bae6ddf06b1835eec773937c4bf9dae997203e0e5f18b`.
  Phase 3A's image was 1,802,752 bytes; the 2,048-byte growth is the new
  counters, the outcome enum and the extra log strings.

None of the above is board evidence. Compiling, linking and passing a static gate
does not prove the engine drew anything.

### Board result (2026-09-27): criteria 1-9 PASS

The image above was flashed to the D50T-2-Lite board. The GE2D section of the
console:

```text
[   2.388] I/lvgl.ge2d.test: ge2d accepted fill=11 image=3 layer=1 | engine fill=11 image=3 layer=0 | sw_fallback im[...]
[   2.388] I/lvgl.ge2d.test: PASS GE2D: 11 fills and 3 images drawn by the engine; 1 layer task(s) claimed, 0 drawn [...]
```

Both lines were captured truncated at about 110 characters. The truncation does
not weaken the verdict: the check returns non-zero and prints `FAIL` for every
assertion, so a printed `PASS` proves `errors=0`, `declined>=1` and every counter
assertion held. The full lines are still worth capturing for the record.

What the counters decide:

- **Criterion 4 is the result of this run.** `engine image=3` with
  `accepted image=3` means `image_sw_fallback == 0`: every claimed IMAGE task was
  drawn by the GE2D, not handed back to software. The ARGB8888
  `GE_PD_SRC_OVER` blend therefore ran on the engine.
- `image=3`, not the 2 the new probes alone would give. The third is `a.jpg`,
  the Phase 2 JPEG fixture at x=24: it is the only image in that row that is
  **not** scaled 4x (`lv_image_set_scale` is applied for `i != 0`), so it is an
  untransformed 160x120 blit the unit claims. The engine accelerated a JPEG
  decode-and-blit as well as the two Phase 3B PNG probes. The Phase 3B probes
  themselves are `b.png` (RGB888, plain copy) and `c.png` (ARGB8888, blend).
- `fill=11` is Phase 3A's 10 plus the white 32x32 swatch added under `c.png`,
  which is opaque, unrounded and therefore claimable. All 11 were engine-drawn.
- `layer=1`, `engine layer=0`. The single LAYER composite was declined and
  composited in software, exactly as the address-window analysis predicted. No
  layer was dropped and no error was reported.

The Phase 2/3A regression check also holds on the same image: `PASS 1000
decode/close cycles` with `CMA current=0 peak=61440 alloc=1000 free=1000` and
`PASS CMA lifecycle balanced`, so criterion 9 is satisfied.

Criteria 10 and 11 are panel judgements and are not decided by the serial log.

### Panel confirmation (2026-09-27): PASS

The operator answered the four questions the serial log cannot decide:

- **`c.png` alpha blend** - the transparent top-left corner shows the white
  swatch through it and the opaque bottom-right corner shows blue, with the ramp
  between. A missing or ignored alpha channel would have painted the whole tile
  opaque with no white anywhere, so this is the observation that confirms the
  blend rather than merely the absence of a crash.
- **`b.png` colours** - correct, no channel swap.
- **The 50% layer rectangle** - a uniform dark slate blue, consistent with
  `0x40a0e0` at `opa_layered = LV_OPA_50` over the `0x202020` page. Had the
  opacity been ignored the rectangle would have been noticeably brighter and more
  saturated.
- **Touch** - still works.

Criteria 10 and 11 are operator judgements, not measurements: they say "the blend
looked right and the rectangle was uniform", not "every pixel matched a
reference". A machine-checkable version (framebuffer capture versus a reference
image) is separate work and is not claimed here.

**All eleven Phase 3B completion criteria are now board-confirmed and Phase 3B is
closed.**

### What is claimed for LAYER, and what is not

Claimed, and now board-confirmed: the unit claims a LAYER task, the composite is
correct because the software path draws it, the layer is never dropped, and the
log states which path ran.

**Not** claimed: that a LAYER composite is drawn by the engine on this board. The
board result is `engine layer == 0` with `sw_fallback layer == layer`, which is
the documented buffer-placement outcome and not a failure.

One qualification is worth keeping in view across the two halves of the phase.
Criterion 10 confirms the ARGB8888 blend is **visually correct**, and criterion 4
confirms the blend was **executed by the engine** - so together they establish
the engine's `GE_PD_SRC_OVER` path. Criterion 11 confirms the 50% layer composite
is visually correct, but that composite was drawn in **software**, so it
validates the fallback rather than the engine's layer path. Phase 3B never claims
the latter.

The full criteria list is in
[phase3b-image-layer-plan.md](../../../../docs/phase3b-image-layer-plan.md).

---

## Phase 3C - GE2D image transform

Phase 3C adds the image transforms the blit was written to grow into. It is split
so each capability lands with its own evidence rather than as one large change.

| Sub-phase | Capability | Status |
|-----------|------------|--------|
| 3C1 | IMAGE opacity (`opa < LV_OPA_COVER`) | **closed** - board-confirmed |
| 3C2 | IMAGE scale (`rotation == 0`, `scale_x`/`scale_y`) | not started |
| 3C3 | orthogonal rotation (90/180/270) | not started |

Phase 3C1 is closed: the opacity gate is lifted, the Porter/Duff rule is the
straight-alpha one, and both are board-confirmed - the engine drew all nine IMAGE
tasks, and its blend arithmetic matches LVGL's within 0/255 on all three accepted
source shapes. The rule choice is measured rather than argued: the premultiplied
alternative misses by 100/255 on the same inputs.

The capability definition this phase has to keep accurate:

```text
FILL        GE2D accelerated
IMAGE       GE2D accelerated
ARGB IMAGE  GE2D source-over accelerated
LAYER       supported, SW fallback on D13x SRAM layer buffers
```

Three constraints apply to the whole phase and are not negotiable:

- the global LVGL allocator is **not** to be changed to raise the LAYER GE hit
  rate. The layer buffer sits below the GE address window, and moving it is a
  memory-policy decision for a later phase, not a tweak to make a counter look
  better;
- no async GE thread, no global CMA layer allocator, no temporary layer-to-CMA
  copy, no recolor or tile. The no-arbitrary-angle constraint belongs to this
  historical 3C1 checkpoint; unscaled IMAGE rotation is covered by the later
  3C6 implementation candidate.
- the synchronous GE2D dispatch, the existing FILL implementation, the MPP
  decoder and the software fallback all stay as they are.

### Phase 3C1 - IMAGE opacity

#### What changed

1. `lv_draw_aic_ge2d_accepts_image()` no longer declines a partial opacity. It
   declines only `opa <= LV_OPA_MIN`, which is the floor LVGL's own renderer and
   the vendor port use: below it there is nothing visible to draw. Everything
   above runs on the engine.
2. The Porter/Duff rule in `lv_draw_aic_ge2d_blit()` changed from
   `GE_PD_SRC_OVER` to `GE_PD_NONE`.
3. A numeric blend probe was added to the board test, and an opacity row was
   added to the manual page.

The second change is a correction to a Phase 3B claim, and it is the substantive
part of this sub-phase.

#### Why the blend rule changed

Phase 3B recorded that an ARGB8888 source "is blended with its own per-pixel
alpha through `GE_PD_SRC_OVER`". The rule constant was wrong, and Phase 3B's
panel check could not see it.

The GE's blend coefficients are documented in `hal_ge_hw.h` as
`1: 1.0, 2: As, 3: 1-As, 4: Ad, 5: 1-Ad`. The `enum ge_pd_rules` table in
`hal_ge_normal.c` and `cmdq_ops.c` is the **premultiplied** Porter/Duff table:

| rule | coefficients | meaning |
|------|--------------|---------|
| `GE_PD_NONE` | (2, 3) = (As, 1-As) | straight-alpha source-over |
| `GE_PD_SRC_OVER` | (1, 3) = (1.0, 1-As) | premultiplied source-over |
| `GE_PD_DST_OUT` | (0, 3) = (0, 1-As) | this is what pins coefficient 3 to `1-As` |

`GE_PD_SRC_OVER` therefore computes `Cs + Cd*(1-As)`: it adds the source colour
at full strength and expects the source to already carry `As`. The source here
does not. The MPP decoder returns PNG/JPEG output as it found it, and nothing in
the pipeline sets `MPP_BUF_IS_PREMULTIPLY`, so the source is straight alpha and
the premultiplied form over-brightens every partially transparent pixel.

The HAL says the same thing from the other side. `set_premuliply()` rewrites the
`GE_PD_NONE` pair to the `GE_PD_SRC_OVER` pair, but only after enabling the
hardware premultiply stage and only when `src_alpha_mode == 0`. The mixed mode
this blit uses is `src_alpha_mode = 2`, so that rewrite never applies and the
straight pair has to be chosen explicitly. Two independent ArtInChip
implementations agree: the vendor GE2D port leaves `alpha_rules` at its zero
default (which is `GE_PD_NONE`), and the aic_player PNG backend sets
`GE_PD_NONE` by name for `APNG_BLEND_OP_OVER`.

At `opa = LV_OPA_COVER` with an opaque pixel the two rules coincide, which is why
Phase 3B's `b.png` probe looked right under either. They diverge only in the
partially transparent middle of the ramp, and Phase 3B's panel question asked
about the two ends. **The Phase 3B criterion-10 observation was true and
insufficient**: it established that a blend happened, not that the arithmetic was
LVGL's.

The choice is not left as an argument. The probe below measures both rules
against LVGL's own arithmetic on the board.

#### The numeric blend probe

`tests/manual/lv_aic_ge2d_test.c` gained `aic_ge2d_probe_blend()`. It allocates a
CMA source and destination, fills them with a source and a destination that
differ in every channel, runs one `mpp_ge_bitblt` under the configuration
`lv_draw_aic_ge2d_image.c` uses, and compares every destination byte against
`Cs*As + Cd*(1-As)` computed the way LVGL computes it.

- The destination is ARGB8888 rather than the display's RGB565, so the comparison
  is not limited by 5/6-bit quantisation. This measures the blend; the panel
  check covers the display path.
- Three cases cover the two source shapes: RGB888 with a global opacity,
  ARGB8888 with per-pixel alpha, and ARGB8888 with both.
- A fourth case runs the same inputs under `GE_PD_SRC_OVER` and **must not**
  match. If both rules matched, the probe could not tell them apart and the
  choice would be unverified. This is the same anti-vacuity idea as the counter
  guards around it.
- The tolerance is 2 counts out of 255. LVGL combines alpha with `>>8` while the
  GE documents `/255`, so a single blend step can legitimately differ by one
  count. A wrong rule misses by tens.

#### What is still not measured

The probe measures the blend datapath. It does not measure the display path, and
it is not a framebuffer comparison: "the engine's arithmetic matches LVGL's"
and "the panel shows the right colour" remain two different statements. The
manual page's opacity row is what covers the second one, and it is a panel
judgement.

#### Verification so far (host and target, no board claim)

Host, `build/lvgl-host` (external LVGL checkout). The manual test file was
rebuilt in the GE2D-off configuration as well, because it is compiled into two
host targets:

- `cmake --build`: both `lv_aic_manual_test.c` objects rebuilt, no warning
  (`-Wall -Wextra -Werror`);
- `ctest`: 9/9 PASS in 13.81 s. The SDL tests need `/c/msys64/ucrt64/bin` on
  `PATH` for `SDL2.dll`; without it they fail with `0xc0000135`
  (STATUS_DLL_NOT_FOUND), which is environmental.

Target, built with the project's own entry point
`packages/custom/lvgl-aic/tools/sdk/build.sh` (`PHASE=ge2d`,
`ALLOW_COMPONENT_DIRTY=1`):

- `lv_draw_aic_ge2d.c`, `lv_draw_aic_ge2d_image.c` and both manual test files
  compile with no warning attributable to them;
- `ge2d static checks: PASS (not board validation)`: the Phase 3A/3B symbol set
  resolves to its required objects;
- `verify_image.py`: application and bootloader ELF32 RISC-V double-float ABI
  PASS, 9 payload CRCs PASS, 26 packaged MPP fixture/provenance hashes PASS;
- image
  `output/d13x_d50t-2-lite_rt-thread_lvgl-aic-ge2d/images/d13x_D50T-2-Lite_page_2k_block_128k_v1.0.0.img`,
  1,806,848 bytes. Phase 3B's image was 1,804,800 bytes; the 2,048-byte growth is
  the blend probe, its log strings and the new page objects.

Two images were built in this sub-phase. The first, SHA256
`ed64e858c52f813ed391348a75f293e7e41d0e9c7d1e62ddb0caafb03c03ec80`, carried the
probe defect described below and is superseded. The second, SHA256
`0ebd4b7d6be789befe7103aa469922e25d907a8c7507561a4cd8c44bf48d17ee`, is the current
artifact and is the one flashed for the second board run.

Both images match the source that produced them. For the first, a second build
after the source was final recompiled nothing and reproduced the same SHA256, and
the ELF contained the final label string and none of the superseded one. For the
second, the ELF contains the new probe diagnostic string and not the old one.

None of the above is board evidence. Compiling, linking and passing a static gate
does not prove the engine blended anything; that is what the board runs below
were for.

#### Board result, first run (2026-09-27): FAIL - in the probe, not in the port

The image above was flashed. The GE2D section of the console:

```text
[   2.447] I/lvgl.ge2d.test: ge2d accepted fill=13 image=9 layer=1 | engine fill=13 image=9 layer=0 | sw_fallback im
[   2.447] I/lvgl.ge2d.test: blend rgb888 + global 128: max deviation 52/255 at alpha=128
[   2.447] E/lvgl.ge2d.test: FAIL the rgb888 global-alpha blend is 52/255 off LVGL's result
[   2.447] E/lvgl.aic.smoke: GE2D checks failed; page remains available for inspection
```

The failure is in the measurement, not in the port, and the number says which.

52 is exactly the red-versus-blue difference. The probe tables are indexed R, G,
B, but the buffers are stored blue first (`lv_color32_t` is
`{blue, green, red, alpha}`), and the comparison read the destination bytes in
index order - so it compared the red expectation against the blue byte. Worked
through: the engine wrote bytes `[160, 100, 108]` (blue, green, red) and the
expectation was `(R=108, G=100, B=160)`. Mapped correctly all three agree;
mis-mapping red against blue gives `|160 - 108| = 52`, and green matches at 0.
That is precisely the observed 52, which is what identifies the cause.

So `GE_PD_NONE` did compute LVGL's result for this case. What the run does **not**
establish is the other two cases or the cross-check: the first case failed and
the check returns early.

Fixed by comparing `px[2 - c]` instead of `px[c]`. The probe now also logs the
worst channel's expected and actual values alongside the deviation, because a
bare "52/255 off" cost a board cycle to interpret while "R expected 108, got 160"
would not have.

What the same log establishes independently of the probe:

- `fill=13` and `image=9`, up from Phase 3B's 11 and 3. The six new
  partial-opacity IMAGE tasks and the two new backdrop FILL tasks were all
  claimed, so lifting the opacity gate did reach the page.
- `engine fill=13 image=9`, with `accepted` equal to `completed` for both, and
  the run got past the `image_sw != 0` assertion - so `image_sw_fallback == 0`.
  **Every one of the nine IMAGE tasks was drawn by the engine**, including the
  six at `opa` 128 and 64. The partial-opacity path is executed by the engine,
  not merely claimed.
- `layer=1`, `engine layer=0`, unchanged from Phase 3B and expected.
- Phase 2 regression on the same image: `PASS 1000 decode/close cycles`,
  `heap before=121972 after=117708 peak=154468`, `CMA current=0 peak=61440
  alloc=1000 free=1000`, `PASS CMA lifecycle balanced across 1000 cycles`.

The run stopped before the LAYER and fallback assertions and before the summary
`PASS`, so the run as a whole is a FAIL and no criterion is closed by it.

#### Panel observation (2026-09-27)

The operator reported the Phase 3C1 row renders as three tiles fading towards grey
(`b.png` over the mid-grey backdrop) and three fading towards white (`c.png` over
white) - the monotonic ramp the row exists to show. That confirms a global opacity
is being applied; it does not measure its value.

#### Board result, second run (2026-09-27): PASS

Image `0ebd4b7d6be789befe7103aa469922e25d907a8c7507561a4cd8c44bf48d17ee` was
flashed. The GE2D section of the console:

```text
[   2.450] I/lvgl.ge2d.test: ge2d accepted fill=13 image=9 layer=1 | engine fill=13 image=9 layer=0 | sw_fallback im
[   2.450] I/lvgl.ge2d.test: blend rgb888 + global 128: max deviation 0/255 at alpha=128 (R expected 108, got 108)
[   2.450] I/lvgl.ge2d.test: blend argb8888 + pixel alpha 128: max deviation 0/255 at alpha=127 (R expected 108, got
[   2.450] I/lvgl.ge2d.test: blend argb8888 + pixel 200 x global 64: max deviation 0/255 at alpha=50 (R expected 52,
[   2.450] I/lvgl.ge2d.test: cross-check the premultiplied rule: max deviation 100/255 at alpha=128 (R expected 108,
[   2.450] I/lvgl.ge2d.test: PASS GE2D blend: rgb888+global, argb8888 per-pixel and pixel x global all match LVGL wi
[   2.451] I/lvgl.ge2d.test: PASS GE2D: 13 fills and 9 images drawn by the engine; 1 layer task(s) claimed, 0 drawn
[   2.465] I/lvgl.aic.smoke: LVGL 9.6 smoke page is running
[   2.471] I/lvgl.aic.smoke: first frame presented; scheduler is still progressing
```

All three accepted source shapes match LVGL's arithmetic exactly, and the
must-fail cross-check misses by precisely the amount the coefficient table
predicts.

| case | rule | effective alpha | expected R | observed R | deviation |
|------|------|-----------------|-----------|-----------|-----------|
| rgb888 + global 128 | `GE_PD_NONE` | 128 | 108 | 108 | **0/255** |
| argb8888 + pixel alpha 128 | `GE_PD_NONE` | 127 | 108 | 108 | **0/255** |
| argb8888 + pixel 200 x global 64 | `GE_PD_NONE` | 50 | 52 | 52 | **0/255** |
| cross-check, premultiplied | `GE_PD_SRC_OVER` | 128 | 108 | 208 | **100/255** |

The alpha column is the composition LVGL itself would use. An RGB source has no
per-pixel alpha, so the effective alpha is the global opacity (`128`). For the
two ARGB cases it is `LV_OPA_MIX2(pixel, global)`, i.e. `(a1*a2) >> 8`:
`(128*255)>>8 = 127` and `(200*64)>>8 = 50`. The expected red is
`(Cs*As + Cd*(1-As) + 127) / 255` with `Cs = 200` and `Cd = 16`.

The cross-check is the row that matters. The premultiplied rule computes
`Cs + Cd*(1-As)`, which for these inputs is `200 + 16*127/255 = 208`. Against
LVGL's `108` that is a miss of **exactly 100 counts** - the arithmetic prediction,
and 50x the tolerance of 2. The probe therefore separates the two rules by a wide
margin, and the rule the port uses is the one that matches LVGL. A probe that
passed under both rules would have proved nothing about the choice; this one
cannot pass under the wrong rule.

What the same run establishes beyond the blend arithmetic:

- the run reached the summary `PASS`, so every assertion after the probe held:
  `errors == 0`, `fill >= AIC_GE2D_MIN_FILL`, `fill_done == fill`,
  `image_done == image`, `image_sw == 0`, `layer_done == layer`, and
  `fallback >= 1`;
- `fill=13` and `image=9`, up from Phase 3B's 11 and 3 - all eight new page tasks
  were claimed;
- `engine image=9` with `image=9` accepted means `image_sw == 0`: **every one of
  the nine IMAGE tasks was drawn by the engine**, the six partial-opacity ones
  included. The opacity path is executed on the engine, not merely claimed;
- `layer=1` with `engine layer=0` is unchanged from Phase 3B and is the documented
  address-window behaviour, not a regression;
- Phase 2 regression on the same image: the 1000-cycle decode/close and the CMA
  lifecycle balance both PASS.

The console truncated several lines at its terminal width. The truncated tails
are fully determined by the numbers that did survive: `engine image = image -
image_sw` together with `image_done == image` forces `image_sw = 0`, and
`engine layer = layer - layer_sw` together with `layer_done == layer` forces
`layer_sw = 1`.

#### Panel confirmation (2026-09-27)

The operator had already reported, on the first run, that the Phase 3C1 row
renders as three tiles fading towards grey (`b.png` over the mid-grey backdrop)
and three fading towards white (`c.png` over white) - the monotonic ramp the row
exists to show. That confirms a global opacity is being applied; it does not
measure its value, which is what the probe above is for. The two statements are
kept apart deliberately.

#### Phase 3C1 completion checklist

| # | Criterion | Result |
|---|-----------|--------|
| 1 | RGB image opacity correct | **PASS** - board run 2, probe deviation 0/255 |
| 2 | ARGB image opacity correct | **PASS** - board run 2, both ARGB cases at 0/255 |
| 3 | `GE errors == 0` | **PASS** - board run 2 reached the summary `PASS`, which is printed only after the error assertion |
| 4 | no FILL/IMAGE regression | **PASS** - 13 fills and 9 images engine-drawn, `image_sw == 0`, Phase 2's 1000-cycle MPP test PASS on the same image |
| 5 | display and touch normal | display **PASS** (first frame presented, page running); touch **pending operator confirmation** |

Criteria 1 and 2 are decided by the probe, not by the panel: it compares the
engine's output against LVGL's arithmetic, and a wrong rule misses by 100 counts
where the tolerance is 2. Criterion 5's display half is read off the log; its
touch half is an operator judgement and is the one item still open.

The remaining checklist items belong to 3C2 and 3C3 and are listed in
[phase3c-image-transform-plan.md](../../../../docs/phase3c-image-transform-plan.md).

#### What the page looks like now

The Phase 3C1 row sits in the free strip above the Phase 2 row, starting at
x=320, so nothing that Phase 2, 3A or 3B measured has moved:

- a mid-grey 104x32 swatch at (320, 166) carrying `b.png` (RGB, no alpha) three
  times at `opa` 255, 128 and 64. The grey backdrop is what makes a partial
  opacity visible: an RGB source at 50% over mid grey is a visible wash towards
  grey, and the three tiles form a monotonic ramp;
- a white 104x32 swatch at (428, 166) carrying `c.png` (RGBA) three times at the
  same three opacities. `c.png` carries its own 0..255 alpha ramp, so this row
  shows the two contributions multiplying rather than one replacing the other;
- a label at (320, 146).

The `opa` values are the ones the phase specifies: 255, 128, 64. The row adds
six IMAGE tasks and two FILL tasks to the page, all of which the unit should now
claim, so the board check's `image` count should rise from 3 to 9 and its `fill`
count from 11 to 13.

## Phase 3C2 candidate (2026-09-27)

Scale implementation and board probes are ready for D50T-2-Lite validation.
Host regression: 10/10 CTests PASS, including the new production-code scale
contract. GE2D firmware builds and passes static/image checks. These results
are not board acceptance. See [per-stage gates](phase3c-transform.md).
3C3/3C4/3C5 remain NOT_STARTED pending each preceding board gate.

Board feedback from the first candidate showed RGB 0.5 falling back to software despite 19 page IMAGE engine executions. Reproduced and corrected decoder stride normalization moving a padded CMA variable source into inaccessible heap. Full-decoder regression now passes for four formats and three ratios. The corrected image passed the D50T-2-Lite board probes: RGB/ARGB 0.5, 1.5, 2.0, nonuniform, clipped pivot and fallback cases all passed; the scheduler reported 10 scaled engine executions and zero errors. The captured 800x480 RGB888 framebuffer (frame 1703, CRC32 309be4bb) shows the scale, alpha and clip rows without visible corruption. 3C2 is closed.
The optional serial framebuffer capture and Python PNG converter are described
in [capture instructions](framebuffer-capture.md); converter integrity tests
pass, but actual board capture still needs validation.
The [UART upgrade audit](d50t-uart-upgrade.md) confirms required existing
Bootloader/application features and linked entry points, without flashing.

## Phase 3C3 page feedback (2026-09-27)

The D50T-2-Lite operator reports that the Next/Prev controls are visible and
usable and that the rotation images look correct. The preceding delivered
candidate is SHA256
`78bad6d63a0e88a606f5933003cf80d9714ba9d63e47fbbfc68bcf34af21552c`;
the running firmware hash was not read back from the board. The supplied log
also repeats 3C2 scale PASS, scheduler engine=10/errors=0, and first-frame
presentation. These are visual/operator and 3C2 regression results, not
numeric rotation/pivot/clipping acceptance.

Capture initially failed at the interactive shell: both
`lv_aic_capture` and `lv_aic_capture dump` report command not found. A subsequent complete board `help` output listed `lv_aic_capture` with its expected description. Registration is therefore present at runtime;
the failure must not be attributed to a missing export or stale image.
The operator then confirmed recovery after Ctrl+C, manual input and Tab
completion, without reflashing. Terminal input/paste state is suspected; exact
bytes were not captured. No SDK Shell change or extra diagnostic command is
needed. A new rotation-page dump still needs its END/CRC validation.
## GE LAYER source preflight (2026-10-05)

The LAYER evaluator now checks the child layer's declared RGB source format and
rejects a stale layer/buffer format before GE scheduling. Lazy child buffers are
still accepted and validated after allocation. The focused scale contract covers
supported formats, unsupported YUV children and descriptor mismatch; the full GE
and disabled host profiles remain green. Board GE pixels, cache coherency and
panel output are **NOT_RUN**.

- GE host CTest: **71/71 PASS**; no-GE host CTest: **35/35 PASS**.
- SDK `build.ps1 -Phase ge2d -Jobs 8`: **PASS** for boot/app config and build,
  static checks, image checks and manifest.
- SDK manifest records the final SDK and component source identities; LVGL
  `80ca777e37a2b176770726a02e07a6fb79ef0b39`; image SHA256
  `2fb800111c6537ebc0ebeefba539c56dd22d2299e4d1962a61f8488b06364a13`.

## GE IMAGE source admission (2026-10-05)

The IMAGE evaluator now applies the ArtInChip SDK source-format admission gate
before claiming a task. RGB565/RGB888/ARGB8888/ARGB8888_PREMULTIPLIED/XRGB8888
and I420/I422/I444/I400 are admitted; RAW and RAW_ALPHA remain encoded-resource
markers for the component decoders. A8/L8 and other mask-only formats decline to
LVGL software before GE scheduling. The post-decode blitter keeps its four direct
RGB mappings, while immutable YUV frames use the validated frame path.

- Component host GE profile: **71/71 PASS**.
- Component host no-GE/current profiles: **35/35 PASS** each.
- D13x target `build.ps1 -Phase ge2d -Jobs 8 -WithFonts -WithWidgets -WithAicp -WithPlayer -WithApng`: boot/app config and build, static checks, image check and manifest: **PASS**.
- Final pinned evidence: `output/lvgl-evidence/ge2d-fonts-widgets-aicp-player-apng-ge-source-admission-final-pinned/`.
- Image SHA256: `34c493a65b3fcdfe294454279fb2fbdd05e782c5c3a4fa586056691397c1a5df`.
- The manifest records the exact SDK, component and LVGL source pins used for this image.
- Board GE pixels, cache coherency, panel and touch acceptance: **NOT_RUN**.

## D50T-2-Lite key565 PASS and blended-fill alpha probe (2026-10-06)

Board boot of the `ge2d-fonts-widgets-aicp-player-apng-key565-measure-final`
candidate (image SHA256
`5cf251cfd3ae923045b903b239eab38fd007be8d09012fa48fab7cecf68580fd`) resolves
the RGB565 color-key measurement and exposes the first native-fill alpha result.

Serial log: SDK
`output/lvgl-evidence/board-2026-10-06-key565-measure-native-fill/serial.log`,
SHA256 `fe71ec491faed54e79e2971853acab8f1c63b39a587c81840d56fc721a958da2`.

- **key565: PASS 9/9.** All nine samples matched the engine's own RGB565 to
  ARGB8888 conversion; `common encoding=engine mask=03` means the engine value
  equals the replicating expansion `(c<<3)|(c>>2)` for every sample. The
  earlier mismatch was LVGL's plain-shift key, not the comparator. Production
  RGB565 key admission stays disabled until that policy is decided.
- **Native fill, first execution: FAIL (blended ARGB8888 destination alpha).**
  The two blend-disabled ARGB8888 gradient probes that ran kept destination
  alpha 255; the first blended probe (`fmt=0 dir=1 blend=1`) returned a
  non-255 alpha and stopped the suite, so the remaining RGB gradient cases,
  the 240 solid YUV cases, the 32 YUV gradient cases and every later GE2D
  block were not evaluated in this boot.
- Suite change: the blended-alpha expectation is now a soft measurement that
  records `min/max/bad` per probe and continues. Blend-disabled destination
  alpha remains a hard check. The host contract adds an engine that writes the
  blended source alpha into the destination byte and asserts the soft marker,
  the 308-submission count and balanced allocation. Host GE profile
  **71/71 PASS**; the widget-enabled vector profile's four GE contracts
  (`native_fill_probe`, `ge2d_fill`, `ge2d_yuv`, `canvas_image`) are 4/4 PASS.
  The next image carries this probe; the raw GE blended-alpha values are still
  open until that log is returned.
- Also passed in this boot before the stop: build-identity line, RT-Thread
  event binary-sync self-test, three lifecycle cycles, 18/28/42 px font probes
  with cache churn, MPP JPEG/PNG fixtures including corrupt and unsupported
  files, the resource stage (memory JPEG/PNG, BMP, AICP, 1000-cycle CMA
  balance), 12 solid-fill probes, 84 ARGB solid/fake fill probes and the
  fake/video-window probes - all with guards OK.
- The build-identity banner was cut after `lvgl-aic=...@0f8c15a8`: the ulog
  line buffer is 128 bytes including colour and timestamp, and the combined
  137-character line truncates at 115 visible characters. The application now
  logs one identity per line (see SDK `VALIDATION.md`); each fits the buffer.
- Panel, touch and framebuffer capture: **NOT_RUN**.

## D50T-2-Lite blended-alpha characterization and GE v1.1 fillrect admission (2026-10-06)

The `native-alpha-probe` image (SHA-256
`697ad25c9952e7fe8643ce6acb26abe776f2e962f2c26ff19b8de4c3b24e0b12`) was
flashed; the raw log is archived at
`output/lvgl-evidence/board-2026-10-06-native-alpha-probe/serial.log`,
SHA-256 `115aa9cac39c8eecb13eb2014b0eaa74735f6950345e9629f43d04930f054bf0`.

Measured and resolved in this boot:

- **Blended ARGB8888 destination alpha is now fully characterized.** With
  destination alpha 255 the GE v1.1 blend unit applies its straight-alpha OVER
  arithmetic to the alpha channel itself:
  `A_out = (A*A + 255*(255-A) + 127)/255`. The board matched exactly:
  constant A=128 -> 191 for all four direction/reverse cases, and the 32..224
  ramp -> min 192 at mid-ramp, max 228 at the 224 end, again for all four
  cases. RGB888 and RGB565 gradients with constant and ramped alpha stay
  inside their 2 and 3-8 per-255 tolerances.
- Suite change: the probe now hard-checks that formula with tolerance 1
  instead of measuring min/max/bad softly; blend-disabled ARGB probes keep
  requiring 255.
- **key565: PASS 9/9** reconfirmed (`common encoding=engine mask=03`).
- **New GE v1.1 fillrect admission.** After the RGB matrix the runner reached
  the YUV cases and stopped on the first submission: SDK printf
  `fill rectangle not support yuv format, except yuv400`, our
  `FAIL YUV submission fmt=32 space=0 fault=1` and GE quarantine. Root cause
  is the SDK gate `ge_fillrect()` in
  `packages/artinchip/mpp/ge/cmdq_ops.c` (`AIC_GE_DRV_V11`): fillrect admits
  RGB and YUV400 destinations only. `lv_ge_fill` now mirrors the gate locally
  before submission, so non-admitted layouts return `LV_RESULT_INVALID`
  without touching the command queue and without quarantine; only a real DMA
  uncertainty quarantines.
- The native fill runner submits 72 probes (36 RGB gradient, 20 YUV400 solid,
  16 YUV400 gradient) and asserts 236 other-YUV probes are locally rejected
  with untouched buffers. Suite change: the GE2D counters/refresh,
  video-window and remaining GE2D blocks were **NOT_EVALUATED** in this boot
  because the quarantine stopped the suite before them; the next log should
  reach them.
- Also passed before the stop: 1000-cycle decoder CMA balance
  (`CMA current=0 peak=61440 alloc=1000 free=1000`), font probes, 12
  solid-fill, 84 ARGB solid/fake fill, 12 key565 and 12 RGB gradient probes
  with guards OK.
- Panel, touch and framebuffer capture: **NOT_RUN**.

Expected new log lines on the next flash: `PASS YUV400 20 CSC solid probes
guards=OK`, eleven `SKIP YUV fmt=...` lines, `SKIP YUV444P 16 CSC gradient
probes; GE v1.1 fillrect admits RGB/YUV400 only` and
`PASS 36 RGB gradient, 20 YUV400 solid, 16 YUV400 gradient; 236 other-YUV
probes rejected locally`, followed by the GE2D counter/refresh and
video-window blocks that this boot did not reach.

## D50T-2-Lite fill-admission execution: RGB hard alpha PASS, YUV400 CSC open (2026-10-06)

The `fill-admission` image (SHA-256
`5e92f14bdd81b47676138212785e441a8e792d93878bd2e4703387c54b1238b8`) was
flashed; the raw log is archived at
`output/lvgl-evidence/board-2026-10-06-fill-admission/serial.log`, SHA-256
`661740b0304c69e6c2e1e09f36d267e0d041b854b7fecaed06dae13361c5f4a4`.

Measured in this boot:

- **The blended-fill hard check is board-verified.** All 24 non-alpha RGB
  gradient probes passed (ARGB8888/RGB888 error 0, RGB565 error 3-4) and the
  12 alpha-gradient probes passed (ARGB8888 error 1, RGB565 error 7-8, inside
  the 9/255 5-bit tolerance), so the destination-alpha formula
  `A_out = (A*A + 255*(255-A) + 127)/255` now holds as a hard on-hardware
  check rather than a soft measurement. 84 ARGB solid/fake fills and nine
  key565 samples passed again in the same log.
- **The GE v1.1 fillrect admission works on hardware.** The eleven linear
  non-YUV400 layouts printed `SKIP YUV fmt=32/35/43/33/34/36/37/38/39/40/41
  20 CSC solid probes` - local `LV_RESULT_INVALID`, no SDK printf, no
  quarantine, no data written - so the suite reached the YUV400 probes the
  previous image never reached.
- **First YUV400 CSC submission mismatch (open).** The run stopped on
  `FAIL YUV pixel fmt=42 space=0 color=0 error=16`. The expectation for
  BT.601-limited black comes from the SDK `rgb2yuv_bt601` table
  (`{66,129,25,16}`), Y=16; a delta of exactly 16 means the engine wrote 0 or
  32, and the offset column (not the matrix) is in question. This log cannot
  distinguish the cases and the responsible stage is not yet proven.
- **NOT_EVALUATED** because the mismatch stopped the native-fill runner: the
  remaining YUV400 solids and gradients, the GE2D counter/refresh block and
  the video-window block. Panel, touch and framebuffer capture remain
  **NOT_RUN**.

Suite change (component `676b970`, SDK pin `d2a6aadc`): the failure line now
prints `got`, `want`, the active-pixel `range` and the failing `offset`, and
all 20 YUV400 solids run before the summary decides, so one boot yields the
full 4-space x 5-color observed table. The next log should end either with
`PASS YUV400 20 CSC solid probes guards=OK` or with one `FAIL YUV pixel...
got=... want=... range=... offset=...` line per mismatching probe plus
`FAIL YUV400 solids N of 20 probes mismatch`; those values pin the YUV400 CSC
model for the corrective change.

## D50T-2-Lite YUV400 raw-byte fill resolution (2026-10-06)

The `yuv400-csc` diagnostic image (SHA-256
`f1fef3b6eb672aa2720a9b1099a479bab195fff9c9ed194add84f6c98b09b9dd`) was
flashed; the pasted serial slice (starting at t=9.672 s, banner not captured)
is archived at `output/lvgl-evidence/board-2026-10-06-yuv400-csc/serial.log`,
SHA-256 `89e5035bd86dc7c28e4dec6e286f9d63e2548c8610ed969d7d9d1ef373dfa765`.
Some trailing `offset=` values are missing from the captured paste's long
lines; the `got`/`want`/`range` fields are complete.

Measured in this boot:

- RGB gradients and the destination-alpha hard check stayed green
  (ARGB8888 error 1, RGB565 error 7-8); the eleven non-YUV400 layouts again
  printed `SKIP YUV fmt=...` with no SDK printf and no quarantine.
- All 20 YUV400 solids ran; 16 failed. Every failing probe wrote the raw red
  byte of the ARGB8888 fill color in all four SDK color spaces (`got == R`):
  BT.601-limited red got 255 against CSC2 Y=82, green got 0 against 144,
  blue got 0 against 41, white got 255 against 235. Black and white pass in
  the two full-range spaces only because the red byte equals their luma
  there (0/255), hence 16 of 20. The earlier `error=16` offset-column
  hypothesis is retired: GE v1.1 bypasses the configured CSC2 output stage
  on this fillrect path.
- NOT_EVALUATED: YUV400 gradients plus the GE2D counter/refresh and
  video-window blocks (the runner stopped after the solids summary); panel,
  touch and capture remain NOT_RUN.

Suite change (`2f6b37e`): `yuv_probe` and `yuv_gradient_probe` model YUV400
as the measured raw red byte for solids and gradients alike; the FAIL lines
fit the 128-byte ulog buffer (the solid line dropped `offset`); the summary
wording reports raw red bytes. Host contracts mirror the model in the engine
mock: vector **93/93**, GE **76/76**. Expected next-boot lines: `PASS YUV400
20 raw red-byte solid probes guards=OK`, `SKIP YUV444P 16 CSC gradient
probes; GE v1.1 fillrect admits RGB/YUV400 only`, sixteen `PASS YUV gradient
...` lines, then `PASS 36 RGB, 20 YUV400 raw-R solid, 16 raw-R gradient; 236
other-YUV rejected`, followed by the counter/refresh and video-window blocks.
If the gradient raw-R model is wrong, its FAIL line prints `got`, `want` and
the failing `offset` for correction.

## D50T-2-Lite YUV400 raw-R verification and GE v1.1 bitblt admission (2026-10-06)

The `yuv400-raw` candidate (image SHA-256
`0b64c97210004ba600a006c827c6d5a1014c75df00a6c2bf2ad1c6dd0b8e1a46`) was
flashed; the pasted serial slice (from 9.330 s, banner not included) is
archived at `output/lvgl-evidence/board-2026-10-06-yuv400-raw/serial.log`,
SHA-256 `2778e45572ceb541438a5f48f871e97be302d1052d014e1448d296a55d803b92`.

Measured in this boot:

- **The raw-R fill model is board-verified.** All 20 solids passed
  (`PASS YUV400 20 raw red-byte solid probes guards=OK`) and all 16 gradient
  probes passed with `error=0` across the four SDK color spaces and both
  directions; the summary closed with `PASS 36 RGB, 20 YUV400 raw-R solid,
  16 raw-R gradient; 236 other-YUV rejected`. Key565 stayed at 9/9 and the
  eleven `SKIP YUV fmt=...` fillrect admission lines plus the
  RGB/destination-alpha hard checks stayed green.
- The runner then entered the YUV image path: `PASS CPU YUV matrices=4 odd
  I420 pixels=36 guards=OK; GE DMA not tested` and `PASS YUV image decoder
  pixels and deferred producer release`.
- **New failure: GE v1.1 bitblt admission.** The first non-YUV400 image
  submission printed the SDK line `bitblt not support yuv format, except src
  format yuv400`, then `FAIL YUV DMA; retaining source/destination until
  reboot` and `FAIL YUV frame/CPU conversion contract`. `ge_bitblt()` in
  `packages/artinchip/mpp/ge/cmdq_ops.c` (`AIC_GE_DRV_V11`) admits
  `is_rgb_or_yuv400(src) && is_rgb(dst)` only; the port treated the
  admission rejection as a DMA fault, so the shared GE session quarantined
  and the remaining probes did not run.
- NOT_EVALUATED in the captured slice: YUV400 ARGB-target probes, the
  I420/packed rotation/scale/tile probes, YUV stripes, and the GE2D
  counter/refresh and video-window blocks; panel/touch/capture remain
  NOT_RUN.

Corrective revision (component `2adbc6a`): `lv_draw_aic_ge2d_yuv` mirrors the
SDK bitblt gate before any cache handoff or submission, so non-admitted
layouts return 0 without touching the command queue and the executor falls
back to its decoded-RGB path (CPU decode to RGB888, then an admitted GE
blit, or the software renderer when that declines). The video-plane rotation
helper declines the same layouts without marking the session GE-faulted. The
runner asserts the eleven non-YUV400 ARGB-target probes decline with
`SKIP YUV ARGB fmt=... 4 probes; GE v1.1 bitblt admits YUV400 src only`,
accepts ENGINE or SOFTWARE for the I420/packed probes while reporting the
first use of each path (pixel oracles unchanged), and keeps YUV400 on the
engine. New host contract `ge2d_yuv_v11_contract` mirrors the SDK gate and
proves the decline releases its lease with zero submissions, rejections,
cache handoffs or allocations. Host contracts: vector **95/95**, GE
**78/78** PASS.

Expected next log: eleven `SKIP YUV ARGB fmt=...` lines, one or two
`GE v1.1 declined; ...` lines, no SDK `bitblt not support yuv format`
printf, and no `FAIL YUV DMA` / `FAIL YUV frame/CPU conversion contract`.

## D50T-2-Lite decoded-RGB fallback verification and I420 tile rotation open item (2026-10-06)

The `yuv-bitblt-admission` candidate (image SHA-256
`6f3aab96a4908bdaa63f0d8cd7923fe097d8ea21f3f1067cef5d91ced89e296b`) was
flashed; the pasted serial slice (from 9.394 s, banner not included) is
archived at `output/lvgl-evidence/board-2026-10-06-yuv-bitblt-admission/serial.log`,
SHA-256 `e62a7c90708849ce90c04a011b3216fdd9ca0568031769b1475adbb8748b3bf1`.

Measured in this boot:

- **The decoded-RGB fallback is board-verified on the engine.** The eleven
  `SKIP YUV ARGB fmt=...` lines appeared, the SDK `bitblt not support yuv
  format` printf did not, the session never quarantined, and after
  `GE v1.1 declined; I420 rotation decodes and blits RGB on the engine` all
  four rotation probes passed with `max_error=0`, the seven scale probes
  with `max_error<=1`, the clipped 2x2 tiles with `max_error=0` and
  `tile rot=0` with `max_error=1`. The fill side stayed green (`PASS 36 RGB,
  20 YUV400 raw-R solid, 16 raw-R gradient; 236 other-YUV rejected`).
- **Open item: `I420 tile rot=90`.** Its tile cell declines on GE v1.1, so
  this is the first probe the software fallback draws:
  `GE v1.1 declined; I420 tile scale decodes and draws RGB in software`,
  then `FAIL I420 tile rot=90 pixels=1920 max_error=93`. A host reproduction
  of the same geometry paints the correct tile-rotated pattern (worst
  channel error 3 without and 5 with antialiasing), so 93 is device-specific;
  93 = 165 (`0xa5` target background) - 72 (darkest expected pixel) is
  consistent with pixels the fallback never painted. The runner then stopped
  at `FAIL GE I420 probe` and `FAIL YUV frame/CPU conversion contract`, so
  the YUV stripes plus the GE2D counter/refresh and video-window blocks
  remain NOT_EVALUATED; panel/touch/capture remain NOT_RUN.

Diagnostic revision (`047baa1`): `tile_rot_compare` reports the worst pixel
(`over3`, `xy`, `got`, `want`, `aa`) on the FAIL line, the probe re-runs once
with `aa=0`, and a `DIAG I420 tile sw open=... cf=... stride=...` line
records whether the software re-decode still opens. The next boot must show
either a region of unpainted `0xa5` pixels (paint path) or a small systematic
filtering error (tolerance model).

## D50T-2-Lite YUV ARGB staging alpha lane and the tile-rot90 closure (2026-10-07)

The `yuv-tile-clean` candidate (image SHA-256
`7ec543b9be84ef07f2e47d2e1f24c50c6f8642faa05f3dd00702a427c00460b5`) was
flashed; the pasted serial slice (from 4.048 s, banner not included) is
archived at `output/lvgl-evidence/board-2026-10-07-yuv-argb/serial.log`,
SHA-256 `3D7B810BC351B1069E7D7B18D07C1A764B7C8F396944B789609825105140CD04`.

Measured in this boot:

- **Board run 8's open item is closed.** `PASS I420 tile rot=90 pixels=1920
  max_error=3 guards=OK` with no `DIAG` and no `aa=0` re-run: the
  clean+invalidate readback and the host-measured software tolerance hold on
  hardware. Every GE I420 rotation/scale/tile probe passed
  (`max_error` 0/1/3).
- All thirty-two colored stripe cells passed (`error<=4`, whole and split
  refresh, opacity 255/128) and the eight packed-format probes at
  `error=0`; the fill side stayed green.
- **New failure: the GE v1.1 bitblt writes the source Y sample into the
  destination alpha lane of the private ARGB surface.** The probe printed
  three `SKIP YUV ARGB fmt=32/33/34 ...` lines (I420/I422/I444 decline
  locally) and then `FAIL YUV ARGB fmt=3 rot=0 error=39`: probe index 3 is
  I400, the only layout GE v1.1 admits. The uniform Y=100 frame left alpha
  100 in the staging surface; the CPU tail mixed it with the probe's opa 64
  to 25, where the native fill oracle keeps 64 - exactly 39. The line
  carries no `xy`/`got`/`want` because the flashed firmware predates
  the probe diagnostics, and the updated host contract reproduces the same
  class of failure when the normalization is disabled (`error=54` at its
  worst analytic pixel). YUV has no source alpha, and `ge_bitblt()`
  builds its blend command with `en_alpha_out_oxff = 0`
  (`OUTPUT_ALPHA_CTRL(0)` in `update_blend_cmd()`), so no engine control
  in the port can ask this path to pass an opaque alpha through.
- The runner stopped at `FAIL YUV ARGB target probes` / `FAIL YUV
  frame/CPU conversion contract`, so the eight remaining `SKIP` lines,
  the I420/packed ARGB-target fallback probes, the mask/color-key ARGB
  blocks and the GE2D counter/refresh and video-window blocks are
  NOT_EVALUATED; panel/touch/capture remain NOT_RUN.

Corrective revision (component `9892cea`): `lv_aic_ge2d_alpha_mark_opaque()`
normalizes each engine-written crop of the opaque staging surface to alpha
0xff. After every successful submission the executor invalidates that crop's
DMA, marks the crop and cleans the marked lines back for the CPU tail, so no
CPU pass reads the engine's alpha lane. Pixels no command wrote (tile and
rotation gaps) keep their zeroed alpha and stay excluded from the tail
blend; the mask tail now multiplies an opaque lane and the color-key tail
clears one. This is a component-owned normalization; SDK behavior is
unchanged.

Host `ge2d_yuv_contract` models the board lane (`board_color()` hands the
Y byte through in the mock's ARGB output) and counts the staging
invalidate/clean pairs: GE profile **73/73 PASS** (17.93 s) and the no-GE2D
baseline **35/35 PASS** (8.75 s). Disabling the normalization makes the
model fail immediately (`ARGB YUV f=0 angle=0 zoom=0 opa=64 xy=54,44 c=3
error=54`), so the contract covers this defect.

Expected next log: four `PASS YUV ARGB fmt=3 rot=0/90/180/270 ...` lines,
eight `SKIP YUV ARGB fmt=...` lines for the probe list's tail, then the
later YUV and GE2D blocks; a remaining mismatch prints `xy`, `got` and
`want`.

## D50T-2-Lite rotated 2x strip against the 8x8 YUV minimum (2026-10-07)

The `yuv-argb-alpha` candidate (image SHA-256
`7c2cd6be03596704ef481cfd14942bfb23db7584723ca9975e2b20eac62255bc`) was
flashed; the pasted serial slice (10.071-10.870 s, banner not included) is
archived at `output/lvgl-evidence/board-2026-10-07-yuv-argb-alpha/serial.log`,
SHA-256 `3D4F16686F3EF0D860904B203AE6C05669A7FE85663F1E89920024BA7DD9841C`.

Measured in this boot:

- **The Board run 9 alpha item is closed.** `PASS YUV ARGB fmt=3 rot=0
  opa=64 error=1` replaced `error=39`; the `mark_opaque` normalization
  holds on hardware.
- Every earlier probe block of the slice stayed green: four I420
  rotations (`max_error=0`), seven scale runs (`max_error<=1`), both tile
  probes, thirty-two stripe cells (`error<=4`), eight packed probes
  (`error=0`) and the three v1.1 `SKIP` lines.
- **New finding: the 90/270 rotated strips were not admissible.** After
  `PASS ... rot=0` the probe went silent. The 8-wide strip at 2x maps to
  a 10x6 source crop; the executor's `w < 8 || h < 8` guard declined
  (return 0) before any cache handoff, allocation or write - matching the
  SDK `check_blit()` gate ("invalid src size, the min size of yuv is
  8x8"). The decline is safe by contract; the probe rejected `!= 1` and
  had no diagnostic on that path.

Probe revision: the 90/270 clips are 16 wide (`{36,24,51,39}`, window
10x10) so the rotated strips reach the engine with the alpha oracle
intact, and every former silent exit now prints a line (executor/decline
`rc=`, clip-outside `xy`/`got`/`want`, setup and lease checks).

Host coverage: `ge2d_yuv_v11_contract` pins the decline (ARGB target, no
allocation/submission/cache, no fault) and the widened submit (source
crop `{12,2,10,10}`, phase `{0,32768}`, destination `{36,24,16,16}`);
`ge2d_yuv_contract` added ratio 512 to the staged-ARGB matrix - **576
scenes, 294912 analytic RGBA pixels, max error 3**.

Expected next log: four `PASS YUV ARGB fmt=3 rot=0/90/180/270 ...` lines,
eight `SKIP YUV ARGB fmt=...` lines, then the later YUV and GE2D blocks;
any remaining failure names its reason on the FAIL line.

## D50T-2-Lite unclipped near-unity scale outcome (2026-10-07)

The `yuv-argb-rot90` candidate (image SHA-256
`2DC19002DB18380A6477A6608F48444A4DFE96CAB581E355F39326666577F220`) was
flashed; the pasted serial slice (9.378-12.908 s, banner not included) is
archived at `output/lvgl-evidence/board-2026-10-07-yuv-argb-rot90/serial.log`,
SHA-256 `BAB4316555D580F0E3878597816C17BB60DA104A5B65D072740112CA8B039121`.

Measured in this boot:

- The Board run 10 rotated-strip revision is closed: four `PASS YUV ARGB
  fmt=3 rot=0/90/180/270` lines with `error=1/1/0/1` plus the eleven v1.1
  `SKIP` lines.
- Every earlier block of the slice stayed green: native fills, the I420
  rotation/scale/tile probes, thirty-two YUV stripe cells, packed probes,
  the image-mask/mask/recolor blocks, the twelve scale ratio cells
  (`max_error<=2`), the three nonuniform and clipped-pivot cases and the two
  fallback probes (`sx=15`, `sx=4097`).
- **New finding: the unclipped `sx=264 sy=256` probe is no longer a software
  fallback.** It stopped with `FAIL scale sx=264 sy=256 argb=0 clip=0 pivot=0`
  and no outcome on the line; `lv_aic_ge2d_scale_test_run()` returned there,
  leaving the RGB stripe matrix, multipass, tiles, SPI stripes, video-window
  and counter/refresh blocks NOT_EVALUATED. The expectation predates the
  near-unity planner: with the 32-wide clamped destination the fractional
  phases stay inside the source, the scale-axis bound holds and the executor
  emits two balanced commands - the host contract for the same geometry
  asserts `submits == before + 2`.

Probe revision: `scale_probe()` initializes the outcome, the software branch
prints `got outcome=` before failing instead of a bare generic line, the
generic FAIL line records `outcome=`, and the 264/256 case asserts ENGINE and
runs the existing gradient/clip-guard pixel oracle. Host `ge2d_scale_contract`
gained a dispatcher-level pin of the same geometry (32x32 RGB888, full-buffer
clip, pivot 0,0): `LV_DRAW_AIC_GE2D_OUTCOME_ENGINE`, exactly two submits, and
the second strip's source crop `{15,0,17,32}` with destination `{26,10,16,32}`.
Combined host **73/73 PASS**; vector/SVG/Lottie-disabled baseline **35/35
PASS** (2026-10-07).

Expected next log: `PASS pixels=... clip_guard=OK engine=1` for the 264/256
case, then the 32 RGB stripe cells, eight striped multipass cases, native
tiles, tile+rotation, SPI stripes, the video-window block and the
counter/refresh summary; a remaining mismatch names its pixel, error and guard
on its own line.

## D50T-2-Lite full GE2D scale matrix closure (2026-10-07)

The `scale-264-engine` candidate (image SHA-256
`6C49BA53ABB3D94674850A1A2DB4F659733CD5E71021878B317E1DBE92E4D8EA`, banner
`sdk@c290bbd1 / lvgl-aic@a31c613a / lvgl=9.6.0@80ca777e`) was flashed; the
pasted serial log (0.000-14.185 s, full boot banner included) is archived at
`output/lvgl-evidence/board-2026-10-07-scale-264-engine/serial.log`, SHA-256
`1667C86F7DE0D68B87348E61CCBE3416146CAEBB925E95FE8249A4129F74297C`.

Measured in this boot (zero `FAIL` lines, zero `E/lvgl.*` records):

- The revised unclipped `sx=264 sy=256` probe passes: `PASS pixels=552
  max_error=0 clip_guard=OK engine=1` - the two-command stripe plan and its
  fractional phases are now hardware-verified.
- All 40 RGB stripe cells pass (`max_error<=2`): `sx=264/281` x four source
  kinds x four rotations (`pixels=576/624`) and the eight 33-degree multipass
  stripe cases (`pixels=819/870`).
- All 20 tiled cases pass (`pixels=3024`, `max_error<=2`): four native-tile
  source kinds and sixteen `sx=384 sy=512` rotations; the eight arbitrary-angle
  multipass cases (four `sx=384 sy=192 rot=33` at `pixels=595` and four tiled
  `sx=512 sy=512 rot=45` at `pixels=3024`) also pass.
- The full scale matrix closes: ten image-mask, thirty-six mask and thirty-six
  recolor cases, the twelve ratio cells, three nonuniform/clipped-pivot cases
  and both fallback probes (`sx=15`, `sx=4097`) pass; `PASS 3C2 numeric
  probes`.
- The previously NOT_EVALUATED blocks ran green: twelve video-window probes
  (`pixels=4096`; physical scanout remains separate), the GE2D counters
  (`accepted fill=23 image=19 layer=1`, `sw fill=0 image=0 layer=0`,
  `declined=8 errors=0`, `refresh timing s=0 us_part=119292`, `ge2d_ready=1`),
  the blend probes (`PASS GE2D blend: three cases within 2/255`),
  `scale scheduler engine=10 errors=0` and the smoke summary `PASS GE2D
  fill=23 image=19 layer=1 hw_layer=1 sw_layer=0 declined=8`.
- The YUV ARGB block stayed green (four `PASS ... error=1/1/0/1` plus eleven
  `SKIP`), and the earlier fill/YUV/mpp/font/native-fill blocks reported no
  failures.

Still open: panel edges/touch human confirmation (the probe itself says so),
`lv_aic_capture` dumps, the APNG/plane/GIF shell gates and physical
scanout/GE timing. SPI stripe probes belong to the separate `-WithSpi` profile
and are intentionally not compiled into this image.
