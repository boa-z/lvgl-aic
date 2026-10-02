# Host smoke test

`AIC_BUILD_IMG_ROLLER_TESTS=ON` adds the adapted SDK carousel contract. It
exercises production code with real layout, scrolling, animation and repeated
teardown. See [image roller stage](../../docs/image-roller-stage.md).

This test is intentionally separate from the D50T product application. It
builds the pinned LVGL checkout supplied by the caller, compiles the
`lvgl-aic` public API and platform sources, and runs a small platform-only UI
smoke page. It does not emulate the ArtInChip framebuffer or touch hardware.

```sh
cmake -S tests/host -B build/host -G Ninja \
  -DLVGL_ROOT=/path/to/lvgl-9.6.0
cmake --build build/host
ctest --test-dir build/host --output-on-failure
```

Without AIC_SDK_ROOT, five tests cover OS notifications, platform lifecycle,
manual pages and disabled features. With SDK ABI headers, eight tests run.
AIC BSP/board validation remains a separate pending step.

## Interactive SDL2 smoke window

The optional `lvgl-aic-sdl-smoke` target reuses the same
`tests/manual/lv_aic_manual_test.c` page with the pinned LVGL 9.6 checkout. It
opens an 800x480 SDL2 window, animates the green marker, and maps the SDL
mouse to an LVGL pointer device. It is a host UI check only; it does not
emulate the ArtInChip framebuffer, PAN/VSync, cache, or GT911 hardware.

Set `AIC_BUILD_SDL_DEMOS=ON` to additionally build the official LVGL 9.6 demo
library from the external checkout. The same executable can then select a
broader upstream page with `--demo`:

```sh
./build/host-sdl/lvgl-aic-sdl-smoke --demo widgets
./build/host-sdl/lvgl-aic-sdl-smoke --demo benchmark
./build/host-sdl/lvgl-aic-sdl-smoke --demo stress
./build/host-sdl/lvgl-aic-sdl-smoke --demo music
./build/host-sdl/lvgl-aic-sdl-smoke --demo keypad_encoder
```

These modes use the unmodified upstream `demos/` sources. They are intended
for 800x480 layout, animation, input, rendering, and lifecycle stress; they do
not prove ArtInChip display hardware behavior. The existing D50T SDL2 product
simulator is not used because it links the legacy
`packages/artinchip/lvgl-ui/lvgl_v9/lvgl` tree. Vector/GLTF and legacy
ArtInChip LVGL 9.1 demos remain excluded from this target.

```sh
cmake -S tests/host -B build/host-sdl -G Ninja \
  -DLVGL_ROOT=/path/to/lvgl-9.6.0 \
  -DAIC_BUILD_SDL_SMOKE=ON \
  -DAIC_BUILD_SDL_DEMOS=ON
cmake --build build/host-sdl
./build/host-sdl/lvgl-aic-sdl-smoke
```

On Windows, run the configure/build commands from an MSYS2 UCRT64 shell (or
use its CMake/Ninja toolchain), with the SDL2 package installed:

```sh
pacman -S --needed mingw-w64-ucrt-x86_64-SDL2
```

The executable also accepts `--frames N` for a bounded run,
`--screenshot PATH` to save a BMP after a short warm-up, and
`--screenshot-frame N` to choose the capture frame (default `30`). Use
`--self-test` to inject a mouse click and verify the status label changes to
`button event received`. When SDL2 is built, CTest also runs the bounded
self-test and the official demo smoke modes with SDL's dummy video driver.

## Decoder and feature-guard regressions

Pass `-DAIC_SDK_ROOT=/path/to/luban-lite` to enable `lvgl_aic_mpp_contract`.
It compiles the production decoder and real SDK MPP ABI headers with a fake
engine/OSAL: fixture info parsing, padded JPEG allocation, correct allocator
callback address, allocation failure and decode-failure cleanup are exercised.
It does not decode image pixels or prove CMA/cache hardware behavior.
`lvgl_aic_disabled_features` exercises the real lifecycle with MPP and touch
explicitly set to zero; no disabled backend may be called.

## GE2D production-source contracts

Set -DAIC_SDK_ROOT=/path/to/luban-lite to additionally build the real draw-unit
and executor contracts. No SDK libraries or physical hardware are required.

- lvgl_aic_ge2d_fill_contract: RGB565/RGB888/XRGB8888 partial opacity; opaque
  ARGB8888; opacity thresholds; partial ARGB/rounded/gradient fallback; color
  and alpha-rule descriptors; nonzero origin and layer clipping; padded stride;
  address/buffer guards; fillrect/emit/sync failure propagation.
- lvgl_aic_ge2d_scale_contract: real image evaluator/executor, LVGL decoder,
  bounded scale, right-angle and arbitrary-angle rotation descriptors, clipping,
  supported formats and injected failures. Arbitrary LAYER rotations exercise
  all four source formats, native decoding, narrow-clip/address fallback and
  empty child layers. All 3600 tenth-degree coefficients are checked against
  a floating-point oracle (less than 2 Q12 units of error). Exact RGB color-key
  descriptors include alpha and key-disable checks; ranges, filtered transforms,
  RGB565 and antialiased ARGB8888 remain covered by rejection/fallback checks.

Hardware calls and cache operations are mocked. These tests do not prove pixel
arithmetic, real cache coherency, DMA completion, panel output or performance.
The board probes in tests/manual exercise those separate acceptance paths.

## Memory and decoded-cache stage

lvgl_aic_mpp_memory_cache_contract runs the production decoder through real
LVGL open/close APIs with FILE and RAW memory JPEG/PNG fixtures. It covers
multiple readers, all option keys, no_cache/zero/oversize budgets, byte/entry
limits, LRU promotion, explicit invalidation, copied file keys, allocation-pressure
eviction, failure cleanup, stream bounds, bad memory CRC and teardown/reinit.
MPP is mocked; this contract does not decode real pixels. The matching board
probe is described in [resource stage](../../docs/resource-stage.md).

## Real native FreeType contract

Configure with -DAIC_BUILD_FREETYPE_TESTS=ON and a host FreeType development
package discoverable by CMake (MSYS2 UCRT64: -DCMAKE_PREFIX_PATH=C:/msys64/ucrt64).
With AIC_SDK_ROOT this yields nine CTests. Native LVGL/FreeType render Latin and
Chinese glyphs; no font rasterizer mocks are used. The contract covers missing /
corrupt files, 18/28/42 px styles, fallback, cache churn, repeated lifecycle,
Fonts/Close events and actual glyph pixels in all four rows. The font files and
licenses remain in the separately pinned LVGL checkout. See
[font stage](../../docs/font-stage.md) for board and memory limits.
## Manual UI layout and navigation contract

Coordinate tests update both screen and top-layer layout before injection. No
synthetic CLICKED fallback is permitted when hit testing fails. Font modal
tests require background navigation to remain blocked, and both feature
configurations check top-layer ownership after teardown.

`lvgl_aic_manual_pages` now exercises the three-page smoke UI with a real pointer indev: the shared top navigation is fixed above every page, previous / next controls wrap in both directions, invalid requests are ignored, and teardown clears pending state. With FreeType enabled it also opens/closes the modal Fonts panel repeatedly and verifies the overlay prevents background navigation. Configure `AIC_BUILD_MANUAL_PREVIEW=ON` with `AIC_BUILD_FREETYPE_TESTS=ON` to emit software reference PPM/PNG frames under the host build's `preview/` directory. These frames validate layout and image placement; GE2D/MPP hardware behavior is not inferred from them.

## Application-owned encoder and mouse contract

Add -DAIC_BUILD_INPUT_TESTS=ON to build lvgl_aic_input_contract. It registers
application callbacks before lv_aic_init(), verifies native encoder and pointer
indev types and callback data, rejects provider changes after initialization,
and checks teardown clears both indevs. The contract does not prescribe a board
protocol; an application supplies the sampler and owns its device lifecycle.

## Native GIF contract

Add -DAIC_BUILD_GIF_TESTS=ON to exercise the component's AIC_LVGL_USE_GIF
configuration mapping and real upstream decoder. The bulb fixture is copied
from the pinned LVGL checkout. Tests cover FILE/RAW sources, RGB565/ARGB8888,
changing pixels and frame indices, paused stability, resume/restart, invalid
inputs, 20 widget cycles plus 20 panel cycles and balanced file handles.
With SDK headers and FreeType enabled the combined suite contains ten CTests.
This is software rendering/lifecycle evidence, not board or heap-profile proof.
See [GIF stage](../../docs/gif-stage.md) for the optional firmware and shell gate.

## Native widget contract

Add -DAIC_BUILD_WIDGET_TESTS=ON to build lvgl_aic_widget_contract. The
profile enables the upstream canvas, chart, dropdown, roller, slider, table,
tabview, textarea and tileview widgets together, then verifies construction,
basic state setters/getters, selection, text storage and teardown. This proves
the application-owned third-party build can expose the native widget surface;
it does not claim board rendering/input acceptance or provide vendor
camera/player/video-window adapters.

## Common control widget contract

Add -DAIC_BUILD_CONTROL_WIDGET_TESTS=ON to build
lvgl_aic_control_widget_contract. The profile covers arc, button,
buttonmatrix, calendar, checkbox, keyboard, led, line, msgbox, spinbox and
switch with constructor, state, lookup and teardown checks. LVGL 9.6 list and
menu are deprecated and are excluded from this profile; vendor media controls
remain application-owned adapters.
