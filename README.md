# lvgl-aic

Independent ArtInChip adaptation for LVGL 9.6.x, consumed as an application-owned
third-party dependency. This repository contains no upstream LVGL source,
product pages, vehicle protocols or product assets.

## Integration and baselines

The application contains sibling third_party/lvgl and third_party/lvgl-aic
submodules and invokes this component's Kconfig/SConscript. The application owns
the UI thread and task loop; the component owns platform adaptation and tools.
SDK RT-Thread, OSAL, framebuffer/touch and MPP remain platform dependencies.
Disable LPKG_USING_LVGL; do not register this port under SDK packages/ or patch
the SDK kernel. See [integration](docs/integration-luban-lite.md).

- LVGL v9.6.0: 80ca777e37a2b176770726a02e07a6fb79ef0b39.
- SDK v1.3.2 reference: c5807f9e7d18292f920dafaa018b8174635085c4.
- Primary target: D13x / D133ECS, D50T-2-Lite, RT-Thread.
- Compile guard accepts 9.6.x; the tested upstream pin remains 9.6.0.

## Current implementation

Framebuffer/PAN/VSync, software display rotation and touch are implemented.
Optional backends include FILE and RAW/RAW_ALPHA memory JPEG/PNG decoding,
a bounded decoded-image cache (512 KiB default), and synchronous GE2D
FILL/IMAGE/LAYER dispatch. FILL includes partial opacity on RGB565/RGB888/XRGB8888
(12 numeric board probes and operator visual acceptance passed in the resource stage). IMAGE supports bounded scaling, orthogonal
rotation and combined transforms. Ordinary D13x heap layers fall back to software.

Target OS integration is LV_OS_CUSTOM with RT event-based binary notifications.
No semaphore-value-limit kernel backport is required. Encoder and USB mouse
inputs are available through application-owned provider callbacks; the board
sampling protocol remains outside this component. The vendor AIC FreeType cache
remains a separate gap. Optional native FreeType now
provides dynamic sizes/styles, Chinese fallback and native glyph caching through
application-owned font lifetimes; see [font stage](docs/font-stage.md).

The caller first calls lv_init(), then lv_aic_init(), and runs lv_timer_handler()
in its own serialized UI loop. lv_aic_init() creates display/touch and enables
configured decoder/draw backends; it does not create product pages. Stop all UI
use and close decoder readers before lv_aic_deinit(). See the
[resource stage](docs/resource-stage.md) for cache invalidation and source lifetimes.

## Capability and evidence references

- [Current capabilities and SDK gaps](docs/capabilities.md)
- [Architecture](docs/architecture.md)
- [Compatibility](docs/compatibility.md)
- [Porting notes](docs/porting-notes.md)
- [Build tools](tools/sdk/README.md) and [host contracts](tests/host/README.md)
- [Historical validation](docs/validation.md)

The resource-stage image has board numeric probes and operator visual acceptance.
Direct resource/cache logs and explicit timed-running evidence remain incomplete.
The new font-stage candidate requires its own board acceptance; never transfer
historical hardware acceptance to a new image.

See [NOTICE.md](NOTICE.md); retain ArtInChip copyright, SPDX and author notices.
