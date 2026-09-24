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

Source the component Kconfig from the superproject Kconfig, for example:

```text
source "packages/custom/lvgl-aic/Kconfig"
```

Enable the new path only when the legacy ArtInChip LVGL path is disabled:

```text
CONFIG_LPKG_USING_LVGL=y
CONFIG_AIC_LVGL_PORT=y
CONFIG_LPKG_MPP=y
CONFIG_AIC_LVGL_USE_TOUCH=y
```

The exact superproject symbols may be adapted to the target's existing naming,
but the selection must remain an explicit legacy/new XOR.

## SCons

The component's `SConscript` compiles only `lvgl-aic` sources and declares
include paths. The LVGL upstream repository remains responsible for compiling
LVGL itself. The superproject must provide the LVGL include directory through
its LVGL build integration.

## Link verification

Before building, verify that the link input contains exactly one LVGL source
tree and one of:

- legacy `packages/artinchip/lvgl-ui/lvgl_v9/lvgl`; or
- `packages/third-party/lvgl` plus `packages/custom/lvgl-aic`.

Never link both paths.

## Configuration

The upstream LVGL 9.6 build should receive the reviewed `lv_conf.h` through
its normal configuration mechanism. Do not use the old LVGL 9.1 CMake variables
as if they were v9.6 options.
