# Luban-Lite integration

This document describes the intended superproject integration. It is not a
patch to the Luban-Lite SDK.

## Layout

```text
packages/
├── artinchip/
│   └── lvgl-ui/       # legacy reference; keep unchanged
├── third-party/
│   └── lvgl/          # LVGL v9.6.0 upstream
└── custom/
    └── lvgl-aic/      # this repository
```

## Submodules

1. Add `packages/third-party/lvgl` as a submodule pinned to LVGL commit
   `80ca777e37a2b176770726a02e07a6fb79ef0b39`.
2. Add `packages/custom/lvgl-aic` as a submodule pinned to the reviewed
   `lvgl-aic` commit.
3. Do not copy LVGL source into `lvgl-aic` and do not use symlinks.

## Kconfig

Source the component Kconfig from the superproject's custom integration layer,
and source it only when the explicit new-LVGL implementation choice is active.
Do not source it unconditionally: the legacy ArtInChip LVGL package and the
new upstream path must remain mutually exclusive.

Enable the new path only when the legacy ArtInChip LVGL path is disabled:

```text
CONFIG_LPKG_USING_LVGL=y
CONFIG_LVGL_V_9=y
CONFIG_LPKG_LVGL_IMPL_AIC=y
CONFIG_LPKG_MPP=y
CONFIG_AIC_LVGL_PORT=y
CONFIG_AIC_LVGL_USE_DISPLAY=y
CONFIG_AIC_LVGL_USE_TOUCH=y
CONFIG_AIC_LVGL_TOUCH_DEVICE="gt911"
```

Luban-Lite's generated `rtconfig.h` uses the Kconfig symbol name without a
`CONFIG_` prefix (for example, `AIC_LVGL_TOUCH_DEVICE`). The touch port
therefore reads `AIC_LVGL_TOUCH_DEVICE`, not a guessed `CONFIG_...` macro.

## SCons

The superproject's `packages/custom/SConscript` owns the single upstream LVGL
core group and invokes the `lvgl-aic` submodule's port group. It must:

- glob `packages/third-party/lvgl/src/**/*.c` exactly once;
- exclude upstream RT-Thread entry points, examples, demos, and optional
  C++ sources;
- force-include the custom build bridge
  `packages/custom/lvgl_aic_build_config.h` for the target group;
- the bridge defines the explicit compiler macros
  `LV_CONF_PATH=lvgl_aic_target_config.h` and
  `LV_CONF_KCONFIG_EXTERNAL_INCLUDE=lv_conf_kconfig_external.h`;
- add `packages/custom/lvgl-aic/compat` to the include path. The unique wrapper
  header includes `../lv_conf.h`, so the selected file is explicit without
  relying on `lv_conf.h` include order;
- point `LV_CONF_KCONFIG_EXTERNAL_INCLUDE` at the component's unique
  `lv_conf_kconfig_external.h` bridge, so upstream RT-Thread/Kconfig defaults
  cannot become a second configuration source;
- add the LVGL public include roots and RT-Thread configuration include root;
- leave `packages/artinchip/lvgl-ui` out of the link when the new choice is
  active.

The component's own `SConscript` compiles only its port sources. Neither
SConscript may compile LVGL upstream a second time.

## Link verification

Before building, verify that the link input contains exactly one LVGL source
tree and one of:

- legacy `packages/artinchip/lvgl-ui/lvgl_v9/lvgl`; or
- `packages/third-party/lvgl` plus `packages/custom/lvgl-aic`.

Never link both paths.

## Configuration

The target SCons group passes `LV_CONF_PATH` as a compiler definition, not as a
CMake cache variable. It also checks the `LV_AIC_LV_CONF_MARKER` in the
selected file before defining the group. The wrapper is needed for the
Windows command-line toolchain, whose shell strips quotes from an absolute
`-D` value. Do not use the old LVGL 9.1 CMake variables as if they were v9.6
options.
