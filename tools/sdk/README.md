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
output/lvgl-evidence/<phase>[-<switches>][-<EvidenceTag>] (for example
`ge2d-od-dashboard-meter-cancap-canota-bench`). Image filenames are unchanged.
The shell entry builds and verifies in output/. Do not flash historical copies
under build/lvgl-evidence; those are no longer refreshed.
Python helpers accept LVGL_AIC_SDK_ROOT; check_integration.py takes --root.
No entry flashes hardware. All board testing uses D50T-2-Lite.

GE2D profile: build.ps1 -Phase ge2d -Jobs 8 -AllowComponentDirty.
The GE2D manual screen has a top-right Next / Prev button to switch between
the baseline (1/3), rotation (2/3) and combined-transform (3/3) pages
without a shell command.
Flash image: `<evidence>/images/d13x_D50T-2-Lite_page_2k_block_128k_v1.0.0.img`.
Burn it whole with the SDK's `tools/scripts/upgcmd.exe -p image <img>` and
`upgcmd shcmd reset` (board shell `aicupg` enters USB upgrade mode);
`artinchip-flash` 0.1.0 never writes rodata/data, so staged assets go stale.
For real framebuffer export see [capture instructions](../../docs/framebuffer-capture.md).
For the existing development UART upgrade route and defconfig requirements see
[UART upgrade](../../docs/d50t-uart-upgrade.md).

## Build switches (build.ps1)

Each switch overlays the isolated smoke defconfig for one build and restores
it afterwards; the effective defconfig is archived with the evidence, and
`check_integration.py` verifies that the enabled set matches the live link.

| Switch | Effect |
|--------|--------|
| `-Phase gate1\|mpp\|ge2d` | software / + MPP decoding / + GE2D draw unit |
| `-OfficialDemos meter,dashboard,slide,multi_lang,demo_hub,image` | verbatim SDK demos ([official demos](../../demos/official/README.md)); implies `-VirtualRes`; `demo_hub` implies `-WithPlayer`, `image` implies `-WithFonts` and the canvas; `demo_hub` and `image` do not fit in rodata together |
| `-VirtualRes` | LVGL renders 1024x600, GE2D scales to the panel ([virtual resolution](../../docs/virtual-resolution.md)) |
| `-WithMeter`, `-WithDashboard` | shorthands for `-OfficialDemos meter` / `dashboard` |
| `-WithCanCapture` | CAN screenshot stream ([CAN capture](../../docs/can-capture-stage.md)) |
| `-WithCanOta [-OtaVersion X]` | the smoke app's CAN OTA endpoint (`AIC_LVGL_SMOKE_CAN_OTA`; lives in lvgl-aic-smoke, not in this component) |
| `-WithFonts`, `-WithGif`, `-WithAicp`, `-WithApng`, `-WithPlayer`, `-WithCamera`, `-WithBarcode`, `-WithSpi` | optional decoders, media and peripherals (see the stage documents) |
| `-WithWidgets`, `-WithDemos`, `-WithMusic`, `-WithVector`, `-WithSvg`, `-WithLottie` | native/adapted widgets, upstream demos, ThorVG vector/SVG/Lottie |
| `-Rotation 0\|90\|180\|270` | display rotation profile |
| `-FbFormat rgb888\|rgb565` | framebuffer pixel format (`AICFB_*`); suffix `-fb565` ([RGB565 and touch](../../docs/rgb565-touch-stage.md)) |
| `-TouchRange 800x480` | `AIC_TOUCH_X/Y_COORDINATE_RANGE` (SDK default 1024x600, the product sets the panel size); suffix `-tr800x480` |
| `-EvidenceTag <tag>` | suffix of the evidence directory |

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
`-WithFonts` also enables SDK-compatible FreeType cache statistics/selective
purge and the busy-entry/rebuild board probes. It does not set a global font
byte budget. The shell entry does not yet expose a font variant. See
[font stage](../../docs/font-stage.md) for ownership and board criteria.

## Native GIF candidate

Add -WithGif to an mpp/ge2d build.ps1 profile; combine -WithFonts for the full
candidate. It stages bulb.gif and its upstream license, checks the GIF widget
and shell entry in the live link map, and verifies packaged resource hashes.
The combined profile writes output/lvgl-evidence/ge2d-fonts-gif. Its board
command is lv_aic_gif show|pause|resume|restart|status|close; commands are handled
by the UI thread. The shell build entry does not expose this optional variant.
See [GIF stage](../../docs/gif-stage.md) for lifecycle and acceptance criteria.
