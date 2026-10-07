# Native Lottie animation widget

`AIC_LVGL_USE_LOTTIE=y` opts into LVGL 9.6's native `lv_lottie` widget and the
pinned ThorVG JSON loader. It selects the existing application-owned vector/C++
profile. `tools/sdk/build.ps1 -WithLottie` adds `-vector-lottie` (or
`-vector-svg-lottie` with SVG) to the evidence profile; all options default off.

The original SDK v9 exposes `LV_USE_RLOTTIE` and `lv_rlottie` controls. This port
provides the native 9.6 animation capability using its bundled ThorVG dependency;
it does not implement the old RLottie API or add the external RLottie library.
Application code needs migration to `lv_lottie` and its `lv_anim_t` controls.

## Application use

Create the widget on the LVGL owner thread, supply an aligned ARGB8888 canvas
using `lv_lottie_set_buffer` or `lv_lottie_set_draw_buf`, then call
`lv_lottie_set_src_data` with JSON bytes and their exact length. The loader copies
source data. Canvas storage remains application-owned: allocate sufficient stride
and alignment, keep it alive while the widget is in use, delete the widget before
freeing it, and use the platform's DMA-safe policy if sharing buffers with devices.
Rendered canvas pixels carry premultiplied alpha; the native drawing pipeline
must preserve that representation. The component wrapper normalizes an accepted
ARGB8888 draw buffer to `ARGB8888_PREMULTIPLIED` before the native canvas caches
its header, because this LVGL software renderer selects blending by color format.
This updates the caller's descriptor metadata, without reallocating storage or
changing ownership. The subsequent [GE premultiplied stage](ge-premult-stage.md)
adds image/layer composition for addressable storage, with software fallback for
unsupported requests. The GE backend does not claim Lottie rasterization.

`lv_lottie_set_src_file` uses a C filesystem path through ThorVG `fopen`, not an
LVGL drive such as `S:/`. The native setters return void. The optional
[checked component loaders](lottie-resources-stage.md) now provide explicit errors,
input/staging limits and failure-preserving source replacement through memory or
an LVGL filesystem drive. Internal renderer heap exhaustion remains governed by
upstream policy.
Files with external assets, expressions, audio and complete After Effects feature
parity are not covered. No new SVG loader, worker threads or JavaScript runtime
is enabled. Allocations use native heaps without a per-animation byte budget.

Use `lv_lottie_get_anim` with native animation controls, including pause/resume.
Source duration/frame count determines timing; setting a source resets playback.

## Pinned opacity correction

ThorVG 0.15.3's `LottieBuilder::updateSolid` and its containing layer scene both
apply layer opacity, turning a 50% solid layer into 25% output. The native pixel
contract reproduced `0x40400000` instead of approximately `0x80800000`.
`tools/sdk/stage_lottie.py` generates a build-local builder with opacity applied
once at the scene; the source SHA256 and exact patch site must match the reviewed
upstream file. Both SCons and host CMake compile that same generated correction.
Neither the LVGL submodule nor SDK source is edited. Target checks validate the
generated contents and the live layer builder's owning object (the solid helper
can be inlined); the generated source is
included in the firmware evidence manifest.

## Validation

The real ThorVG/native widget contract exercises a moving half-opacity solid
layer, FILE/data pixel parity, raw/draw-buffer APIs, copied input lifetime,
source-derived two-second timing, native timer progression, pause/resume/reset,
premultiplied screen composition and 20 create/render/delete cycles with no
remaining widget animations. Combined SVG/vector/Lottie host suite **75/75 PASS**;
source-drift rejection **PASS**. A GE contract independently checks that explicit
premultiplied canvas storage originally took software composition without a GE
submission and retained its half-opacity red pixel. This checkpoint is superseded
by the GE composition and fallback coverage in the premultiplied stage. Logs: `output/lottie-{build,tests}.log`.
Disabled baseline **72/72 PASS**. Combined D13x 90-degree GE/media/SPI/demos/
vector/SVG/Lottie firmware passes final-link/static, image and clean-source
manifest gates. Exact pins and image SHA256 are in [validation](validation.md). Physical display, heap pressure, long-running
animation timing and GE coexistence are **NOT_RUN**. No flashing.
