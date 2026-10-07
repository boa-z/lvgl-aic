# Native vector graphics stage

The SDK v9 baseline exposes `LV_USE_VECTOR_GRAPHIC` and internal ThorVG choices
in `packages/artinchip/lvgl-ui/lvgl_v9/lvgl/Kconfig`, and its SConscript collects
C++ sources. The application-owned port previously disabled those options and
collected only upstream C sources. This stage adds the native LVGL 9.6 vector
API and its pinned ThorVG 0.15.3 software backend without modifying either SDK
sources or the LVGL dependency.

## Configuration and use

- `AIC_LVGL_USE_VECTOR=y` (default off) enables vector, matrix and floating-point
  support with one software draw unit; it selects SDK `RT_USING_CPLUSPLUS` for
  startup constructors and the existing RT allocation/runtime interfaces.
- `tools/sdk/build.ps1 -WithVector` adds this profile to any test build and adds
  `-vector` to its evidence directory. Existing GE/media/SPI/demo options compose.
- Use native `lv_draw_vector_dsc_create`, `lv_vector_path_*`, descriptor setters
  and `lv_draw_vector` on the LVGL owner thread. A native canvas can be initialized
  with `lv_canvas_init_layer` and submitted with `lv_canvas_finish_layer`.
- This is software vector rasterization; GE2D has no new vector acceleration.
  Vector path/parser storage uses native heap allocation. Target staging now has
  a separate byte limit; see [surface correction](vector-surface-stage.md). The optional [native SVG image decoder](svg-stage.md) is configured separately.
  The vector option itself does not enable the Lottie widget, SVG loaders or ThorVG
  worker threads. The separately configured [native Lottie widget](lottie-stage.md)
  now enables its JSON loader; physical validation remains separate.

C++14/no-exception/no-RTTI flags are local to the ThorVG group; sized deallocation
is disabled so the backend uses the SDK's existing unsized delete interface.
A build-only compatibility header normalizes disabled loader flags because the
pinned upstream configuration defines them numerically while the registry uses
`#ifdef`. SVG loader sources stay excluded; Lottie sources are selected only
with the explicit Lottie option. The shim establishes application
configuration before including upstream headers, even when SCons orders the
C++ forced include before its global C forced include. A linked C++ probe asserts
that the application OS bridge, vector/float/matrix settings and single draw
unit are all selected, with only the requested Lottie loader and no accidental
SVG, expression or thread features.

## Validation

Host vector-enabled GE/widget regression: **73/73 PASS**; original vector-disabled
profile: **72/72 PASS**. A real native canvas/ThorVG contract checks triangle
pixels, even-odd holes, partial opacity, two-color linear gradients, transformed
clipping with all outside pixels guarded, and 20 create/render/delete cycles.
Full-coverage channel checks allow the native LVGL opacity product's one-level
rounding. No mocked rasterizer or source implementation mirror is used.

The development combined D13x profile passed boot/app, final-link/static, image
and manifest checks after selecting the SDK C++ runtime. The initial omitted
runtime failed with `__dso_handle`; no SDK source patch was required. The clean
commit build, including the new C++ configuration probe, now passes all gates.
Logs: component `output/vector-{config,build,tests,all-build,all-tests}.log`,
`output/vector-baseline-{build,tests}.log`, `output/vector-firmware-dev.log`.
Physical vector rendering, memory pressure, throughput and GE coexistence on
panel are **NOT_RUN**. Camera/panel setup and flashing are not part of this stage.


Clean final firmware: component `c4112b5012414ddea18bcca31224a5898838380c`, SDK
`837ec005cc5a60f1af101d7d6eff794733b02d1a`, LVGL
`80ca777e37a2b176770726a02e07a6fb79ef0b39`. Image SHA256:
`5a2c3b905e2c8dd0961327668ec0456f9b2e4f7b7e6a5b8073e6a4ca8e427237`.
All checkouts were clean; no generated objects remain in the LVGL source tree.
See [validation record](validation.md) for the combined profile and manifest.

The later [surface correction](vector-surface-stage.md) preserves straight and
premultiplied ARGB targets, packed RGB and RGB565A8 planes, outer/task clipping,
backdrop-dependent composition and checked scratch/raster failure cleanup.
The later [blend correction](vector-blend-stage.md) fixes translucent multiply/
screen and paint opacity. The [operator stage](vector-operators-stage.md) adds
destination-over, additive and vector subtractive erasure. The
[coverage stage](vector-coverage-stage.md) supplies SRC_IN/DST_IN and bounded
replacement with independent path coverage. General SVG document semantics
remain open; host corrections do not establish physical throughput.
