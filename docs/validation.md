# Validation record

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
