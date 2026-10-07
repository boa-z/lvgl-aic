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

Validation: baseline regression **70/70 PASS**, both demo tests **PASS**, and the combined GE/font/GIF/widget/AICP/
player/APNG/barcode/SPI/demo 90-degree target firmware passes boot/app builds,
static/live-link gates, image checks and manifest generation. The saved image
hash was independently verified. Final-map evidence includes both demo entries
and native `lv_sysmon_builtin_init` / `lv_sysmon_show_performance` functions.

- Component: `2652b94ffe54239d0ba42b7387b9eb07586b8e5f` (clean).
- SDK: `2f86ec32e17f00d8598987d229ff3c625b455327` (clean).
- LVGL: `80ca777e37a2b176770726a02e07a6fb79ef0b39` (clean).
- Image SHA256: `8a64743d82a81373f9da84ba27b3f12937d45cee50dc8b74f01eaf97598fd3c4`.
- Manifest: SDK `output/lvgl-evidence/ge2d-fonts-gif-widgets-aicp-player-apng-barcode-spi-demos-rotate90/manifest.json`.

The SDK already owns `AIC_LVGL_DEMO_*` choice symbols; the independent
`AIC_LVGL_BUILD_DEMO_*` names avoid selecting those legacy dependencies.
Two small component C wrappers include the native private headers via the
short include-search path before including the unmodified demo sources. This
avoids an old Windows E907 compiler limit on deeply expanded relative include
paths. SCons collects demo sources with its normal variant-directory mapping,
so all objects stay out of the pinned LVGL source checkout.

Physical display, input, GE throughput and native benchmark measurements remain
**NOT_RUN**. The firmware links these entries but still starts the smoke UI;
an application must select the desired demo at UI startup as described above.

## Independent music UI option

`AIC_LVGL_BUILD_DEMO_MUSIC` / `build.ps1 -WithMusic` adds the upstream music
interface and assets independently of widgets/benchmark. Evidence uses a
`-music` suffix. Both options may be combined, but music does not select the
legacy SDK demo or media/audio player. It is the stock visual music interface
with generated spectrum/track animations, not audio decoding or playback.

Use `lv_demo_music()` or `lv_demo_music_with_args()` from `demos/lv_demos.h`
on the UI owner thread after display initialization. The native compact layout
is the default; native `LV_DEMO_MUSIC_*` layout options remain available to an
application configuration. Fonts 12/16/22/32 cover compact and large layouts.
Single active instance follows upstream static state; no autoplay is selected.
The smoke image links the entry/control APIs and still starts its own UI.

Host option `AIC_BUILD_MUSIC_DEMO_TESTS=ON` builds the isolated music configuration.
`lvgl_aic_music_demo_contract` renders a varied 320x480 frame, checks pixel/flush
changes during track animation and track switching, exercises pause/resume,
then deletes the page and advances timers before shutdown. Music host test **PASS**, baseline regression **70/70 PASS**, and strict E907
wrapper compilation **PASS**. The independent music profile (widgets/benchmark
disabled) passes full boot/app, final-link/static, image and manifest gates.
Six entry/control functions are verified live in the final ELF. This image also
includes the GE wide-coordinate crop correction from `d4fcda9`.

- Component: `5c898239919c97d66bf5934e54e6cc76d8a1dbdd` (clean).
- SDK: `955f1bec8763701a3bf7851674cbaaf846e33750` (clean).
- LVGL: `80ca777e37a2b176770726a02e07a6fb79ef0b39` (clean).
- Image SHA256: `673f0a1d2c6ae2937f16f8854e3b13a7b1675fc182ef2babe81aa055ee27c77c` (independently verified).
- Manifest: SDK `output/lvgl-evidence/ge2d-fonts-gif-widgets-aicp-player-apng-barcode-spi-music-rotate90/manifest.json`.

Music UI and widgets/benchmark have independent host/target evidence; the combined
all-three-demo target configuration has not been built in this checkpoint.
The image still starts the smoke UI; physical rendering, touch and timing remain
**NOT_RUN**. Logs are `output/music-demo-{config,build,tests,target,firmware}.log`
and `output/music-demo-baseline-{build,tests}.log` in the component checkout.


## Combined widgets/benchmark/music profile (2026-10-04)

The all-three-demo configuration now passes boot/app, final-link/static, image
and manifest checks together with GE/fonts/GIF/vendor widgets/AICP/player/APNG/
barcode/SPI and 90-degree display rotation. Component `557a2ee`, SDK `7cc0503a`;
all source checkouts clean. Image SHA256:
`1827fd9d7b163cc2b1d38f6c1ccc0cd504b82789d4705c7f112fef9e70124198`.
See [current validation](validation.md) for exact pins and the `-demos-music`
evidence profile. This supersedes the earlier unbuilt-combination boundary.
Demo activation and physical runtime acceptance remain application/board tasks;
no auto-launch, music audio playback or benchmark performance claim is added.
