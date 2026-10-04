# Application-owned target demos

The SDK exposes upstream widgets and benchmark demos on target. The port now
provides independent `AIC_LVGL_BUILD_DEMO_WIDGETS` and `AIC_LVGL_BUILD_DEMO_BENCHMARK`
Kconfig switches, both off by default. Benchmark selects widgets because its
last scene uses that demo. Only their upstream 9.6 source trees and shared demo
entry helpers are added to the application LVGL group; no legacy SDK LVGL/demo
implementation is linked. No sibling LVGL source changes are needed.

The configuration enables the built-in fonts used at small/medium/large screen
sizes. Benchmark also enables native system and performance monitoring; the
compile-time probe rejects a benchmark built without those measurements.
Vector/ThorVG and music are separate options and are not enabled by these switches.

## Application use

Include `demos/lv_demos.h` from the pinned LVGL root and call `lv_demo_widgets()`
or `lv_demo_benchmark()` on the LVGL owner thread after display/input setup.
The application selects which entry to start, replacing its ordinary UI startup.
The switches only compile the demos; they do not automatically launch them over
the smoke UI. Widgets also supports `lv_demo_widgets_with_args` for an explicit
parent. Upstream benchmark owns active-screen contents, timers and top-layer
objects across all scenes: run on a dedicated screen and do not destroy or
switch away while it runs. Register `lv_demo_benchmark_set_end_cb` before start
if the application needs the native completion summary. Neither demo is a
component-managed media/widget session with an asynchronous close contract.

## Build and verification

`tools/sdk/build.ps1 -Phase ge2d -WithDemos` enables both demos in the isolated
smoke defconfig and gives the evidence directory a `-demos` suffix. It can combine
with the existing optional features and rotation. The script restores the
original defconfig. Smoke builds retain the actual widgets and benchmark entry,
parent, completion and summary APIs; static gates check live final-map symbols
and configuration/header agreement. No demo is started by this linking gate.

Host configuration: `AIC_BUILD_TARGET_DEMO_TESTS=ON` in `tests/host`, with an
absolute quoted `LVGL_ROOT` pointing to the sibling 9.6 checkout. Build target
`lvgl_aic_target_demo_contract`, then run tests matching `lvgl_aic_target_`.
These use the same application `lv_conf.h` demo switches, not the SDL overlay.
Widgets renders a varied frame and cleans its screen; benchmark completes all
16 upstream scenes and checks each received measurement samples. Virtual time
advances during software rendering: results are not performance measurements.

Initial host evidence: both demo tests PASS. Target build/link verification
pending at this checkpoint. Physical display, input, GE throughput and native
benchmark measurements remain **NOT_RUN**.
