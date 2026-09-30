# D50T-2-Lite SDK integration tools

Keep development tools in lvgl-aic; the SDK owns board configuration, minimal
application glue and submodule pins. Invoke the component tools directly;
no LVGL-specific wrappers are maintained under SDK tools/.
Use a dedicated SDK checkout: these builds change its active configuration.
The component must be installed at application/rt-thread/lvgl-aic-smoke/third_party/lvgl-aic in that checkout.
The MPP defconfig stays in SDK target/configs because the SDK config loader
uses that filename to select the project output directory.

Windows (from SDK root):

```powershell
& application/rt-thread/lvgl-aic-smoke/third_party/lvgl-aic/tools/sdk/build.ps1 -Phase mpp -Jobs 8 -AllowComponentDirty
# Explicit integration checkout:
& application/rt-thread/lvgl-aic-smoke/third_party/lvgl-aic/tools/sdk/build.ps1 -SdkRoot C:/path/to/sdk -Phase gate1
```

Linux / configured SDK shell:

```sh
LVGL_AIC_SDK_ROOT=/path/to/sdk PHASE=mpp ALLOW_COMPONENT_DIRTY=1 bash application/rt-thread/lvgl-aic-smoke/third_party/lvgl-aic/tools/sdk/build.sh
```

Dirty component builds are opt-in. Windows archives logs, image, ELF, map,
configuration, source patches, untracked sources and hashes under SDK
output/lvgl-evidence/{gate1,mpp,ge2d}. Image filenames are unchanged.
The shell entry builds and verifies in output/. Do not flash historical copies
under build/lvgl-evidence; those are no longer refreshed.
Python helpers accept LVGL_AIC_SDK_ROOT; check_integration.py takes --root.
No entry flashes hardware. All board testing uses D50T-2-Lite.

GE2D profile: build.ps1 -Phase ge2d -Jobs 8 -AllowComponentDirty.
The GE2D manual screen has a top-right Next / Prev button to switch between
the baseline (1/3), rotation (2/3) and combined-transform (3/3) pages
without a shell command.
Windows flash image: output/lvgl-evidence/ge2d/images/
d13x_D50T-2-Lite_page_2k_block_128k_v1.0.0.img.
For real framebuffer export see [capture instructions](../../docs/framebuffer-capture.md).
For the existing development UART upgrade route and defconfig requirements see
[UART upgrade](../../docs/d50t-uart-upgrade.md).

## Application-owned integration (2026-09-30)

Place this repository at the consuming application's third_party/lvgl-aic,
with LVGL v9.6.0 pinned alongside at third_party/lvgl. Source this component's
Kconfig and SConscript from the selected application. Disable the vendor
LPKG_USING_LVGL package and enable RT_USING_EVENT. Board profiles remain in
SDK target/configs; packages/, kernel/ and global Kconfig are unchanged.

The custom OS adapter uses a native RT event bit for binary notifications;
duplicate signals coalesce and receive clears the bit atomically. Host contract
tests cover errors and lifecycle with a fake RT API. The smoke application's
sync selftest must still run on the board to validate scheduling and wakeup.
SDK and host builds do not establish hardware acceptance.

## Resource-stage candidate

MPP and GE2D builds link the memory/cache board probes and cache-control APIs.
AIC_LVGL_MPP_CACHE_BYTES defaults to 524288; zero disables retained images.
The automatic pre-page checks include file-memory JPEG/PNG pixel parity,
active-reader invalidation, 100 cache hits per PNG without new CMA allocation,
and the existing 1000 uncached decode cycles. See the
[resource-stage board gate](../../docs/resource-stage.md). Firmware build and
link success leave board acceptance NOT_RUN.

## Native font candidate

Use build.ps1 -Phase ge2d -WithFonts -AllowComponentDirty -Jobs 8. This Windows
entry overlays only the isolated smoke defconfig and restores its exact bytes
on success/failure; generated .config retains the effective build. The new
output/lvgl-evidence/ge2d-fonts directory records original/effective defconfigs,
30 font/resource/license assets, live font symbols and image payload checks.
The shell entry does not yet expose a font variant. See
[font stage](../../docs/font-stage.md) for ownership and board criteria.
