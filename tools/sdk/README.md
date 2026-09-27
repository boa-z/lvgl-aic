# D50T-2-Lite SDK integration tools

Keep development tools in lvgl-aic; the SDK owns board configuration, minimal
application glue and submodule pins. Invoke the component tools directly;
no LVGL-specific wrappers are maintained under SDK tools/.
Use a dedicated SDK checkout: these builds change its active configuration.
The component must be installed at packages/custom/lvgl-aic in that checkout.
The MPP defconfig stays in SDK target/configs because the SDK config loader
uses that filename to select the project output directory.

Windows (from SDK root):

```powershell
& packages/custom/lvgl-aic/tools/sdk/build.ps1 -Phase mpp -Jobs 8 -AllowComponentDirty
# Explicit integration checkout:
& packages/custom/lvgl-aic/tools/sdk/build.ps1 -SdkRoot C:/path/to/sdk -Phase gate1
```

Linux / configured SDK shell:

```sh
LVGL_AIC_SDK_ROOT=/path/to/sdk PHASE=mpp ALLOW_COMPONENT_DIRTY=1 bash packages/custom/lvgl-aic/tools/sdk/build.sh
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
the baseline (1/2) and rotation (2/2) pages without a shell command.
Windows flash image: output/lvgl-evidence/ge2d/images/
d13x_D50T-2-Lite_page_2k_block_128k_v1.0.0.img.
For real framebuffer export see [capture instructions](../../docs/framebuffer-capture.md).
For the existing development UART upgrade route and defconfig requirements see
[UART upgrade](../../docs/d50t-uart-upgrade.md).
