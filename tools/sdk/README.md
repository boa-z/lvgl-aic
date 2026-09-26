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
build/lvgl-evidence/{gate1,mpp}. The shell entry builds and verifies in output/.
Python helpers accept LVGL_AIC_SDK_ROOT; check_integration.py takes --root.
No entry flashes hardware. All board testing uses D50T-2-Lite.
