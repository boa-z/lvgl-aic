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
  Vector storage uses native heap allocation, with no separate component byte
  budget. This option does not enable the Lottie widget, SVG loaders or ThorVG
  worker threads. Those require separate integration and validation.

C++14/no-exception/no-RTTI flags are local to the ThorVG group; sized deallocation
is disabled so the backend uses the SDK's existing unsized delete interface.
A build-only compatibility header normalizes disabled loader flags because the
pinned upstream configuration defines them numerically while the registry uses
`#ifdef`. Loader source files are excluded. The shim establishes application
configuration before including upstream headers, even when SCons orders the
C++ forced include before its global C forced include. A linked C++ probe asserts
that the application OS bridge, vector/float/matrix settings and single draw
unit are all selected, with no accidental loader/thread features.

## Validation

Host vector-enabled GE/widget regression: **73/73 PASS**; original vector-disabled
profile: **72/72 PASS**. A real native canvas/ThorVG contract checks triangle
pixels, even-odd holes, partial opacity, two-color linear gradients, transformed
clipping with all outside pixels guarded, and 20 create/render/delete cycles.
Full-coverage channel checks allow the native LVGL opacity product's one-level
rounding. No mocked rasterizer or source implementation mirror is used.

The development combined D13x profile passed boot/app, final-link/static, image
and manifest checks after selecting the SDK C++ runtime. The initial omitted
runtime failed with `__dso_handle`; no SDK source patch was required. A clean
commit build, including the new C++ configuration probe, is the next gate.
Logs: component `output/vector-{config,build,tests,all-build,all-tests}.log`,
`output/vector-baseline-{build,tests}.log`, `output/vector-firmware-dev.log`.
Physical vector rendering, memory pressure, throughput and GE coexistence on
panel are **NOT_RUN**. Camera/panel setup and flashing are not part of this stage.
