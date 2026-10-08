# lvgl-aic

Independent ArtInChip adaptation for LVGL 9.6.x, consumed as an application-owned
third-party dependency. This repository contains no upstream LVGL source,
product pages, vehicle protocols or product assets. The only vendored UI
sources and assets are the ArtInChip SDK's own demos under `demos/official/`,
kept verbatim for comparison with the SDK's LVGL 9.1 stack.

## Integration and baselines

The application contains sibling third_party/lvgl and third_party/lvgl-aic
submodules and invokes this component's Kconfig/SConscript. The application owns
the UI thread and task loop; the component owns platform adaptation and LVGL
development tools. SDK RT-Thread, OSAL, framebuffer/touch and MPP remain
platform dependencies. Disable LPKG_USING_LVGL; do not register this port under
SDK packages/ or patch the SDK kernel. See [integration](docs/integration-luban-lite.md).

- LVGL v9.6.0: 80ca777e37a2b176770726a02e07a6fb79ef0b39.
- SDK v1.3.2 reference: c5807f9e7d18292f920dafaa018b8174635085c4.
- Primary target: D13x / D133ECS, D50T-2-Lite, RT-Thread.
- Compile guard accepts 9.6.x; the tested upstream pin remains 9.6.0.

Application concerns stay in the application. Firmware update over CAN, for
example, is a bench tool of the lvgl-aic-smoke application (its `ota/`
directory), not part of this component.

## Current implementation

- **Display and input**: framebuffer with PAN/VSync, software and GE display
  rotation, touch, plus application-owned encoder and USB-mouse providers.
  The display color format follows the framebuffer (`AICFB_*`). Optional
  virtual resolution renders a larger logical screen (1024x600 for the SDK
  demos) and scales it to the panel with GE2D, mapping touch back
  ([virtual resolution](docs/virtual-resolution.md)).
- **Decoding**: FILE and RAW/RAW_ALPHA memory JPEG/PNG through MPP with a
  bounded decoded-image cache (512 KiB default), AICP/BMP and SDK fake-image
  providers.
- **GE2D draw unit**: FILL (gradients, partial opacity), IMAGE (bounded scale,
  orthogonal and multipass arbitrary rotation, tiling, recolor, color keys,
  masks) and LAYER, plus YUV sources; unsupported or small cases fall back to
  software ([capabilities](docs/capabilities.md)).
- **Fonts**: native FreeType with dynamic sizes/styles and Chinese fallback,
  and an SDK-compatible FreeType cache API (`aic_lv_ft_cache_*`;
  [font stage](docs/font-stage.md), [FT cache](docs/ft-cache-stage.md)).
- **SDK widgets as source**: canvas (replacing the SDK's LVGL 9.1 prebuilt
  library), image roller, swipe, media player and video window, APNG, camera,
  barcode, SPI display pipeline, ThorVG vector/SVG/Lottie.
- **Development tools**: manual test pages, framebuffer screenshots over UART
  or CAN ([CAN capture](docs/can-capture-stage.md)), the official SDK
  demo runner (`lv_aic_demo`, [official demos](demos/official/README.md)),
  SDK build/verification scripts ([build tools](tools/sdk/README.md)) and host
  contracts ([host tests](tests/host/README.md)).

Target OS integration is LV_OS_CUSTOM with RT event-based binary notifications.
No semaphore-value-limit kernel backport is required.

The caller first calls lv_init(), then lv_aic_init(), and runs lv_timer_handler()
in its own serialized UI loop. lv_aic_init() creates display/touch and enables
configured decoder/draw backends; it does not create product pages. Stop all UI
use and close decoder readers before lv_aic_deinit(). See the
[resource stage](docs/resource-stage.md) for cache invalidation and source lifetimes.

## Status

Most features are build- and host-verified; board acceptance is recorded per
image and is not transferred to a new image. Board-verified so far include the
GE2D scale/rotation matrix, the decoder and cache probes, FreeType text, the
virtual-resolution display and six official SDK demos; the open items
(touch-driven checks, physical player/video-plane scanout, APNG/GIF shell
gates, SPI panels) are tracked in [capabilities](docs/capabilities.md),
[validation](docs/validation.md) and the [SDK parity roadmap](docs/sdk-parity-roadmap.md).

## References

- [Current capabilities and SDK gaps](docs/capabilities.md)
- [Architecture](docs/architecture.md)
- [Compatibility](docs/compatibility.md)
- [Porting notes](docs/porting-notes.md)
- [Official SDK demos](demos/official/README.md) and [porting goals](docs/aic-demo-goals.md)
- [Build tools](tools/sdk/README.md) and [host contracts](tests/host/README.md)
- [Historical validation](docs/validation.md)

See [NOTICE.md](NOTICE.md); retain ArtInChip copyright, SPDX and author notices.
