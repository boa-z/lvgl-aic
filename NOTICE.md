# NOTICE

This repository is an independent adaptation layer. It does not relicense
LVGL upstream or ArtInChip Luban-Lite.

## LVGL

- Project: LVGL
- Source: https://github.com/lvgl/lvgl
- Fixed tag: `v9.6.0`
- Fixed commit: `80ca777e37a2b176770726a02e07a6fb79ef0b39`
- License: MIT (as distributed by the LVGL project)

LVGL source is consumed as a separate dependency and is not stored in this
repository.

## ArtInChip reference material

- Project: ArtInChip Luban-Lite
- Source repository used for the initial porting reference:
  `https://github.com/boa-w/luban-lite`
- Reference revision:
  `c5807f9e7d18292f920dafaa018b8174635085c4`
- Original ArtInChip upstream revision recorded by the task baseline:
  `f2ee295b1a098b4f6e7d9e78133ed972b58d4ce7`
- Relevant original paths:
  - `packages/artinchip/lvgl-ui/lvgl_v9/lv_drivers/`
  - `packages/artinchip/lvgl-ui/lvgl_v9/lvgl/`
  - `packages/artinchip/lvgl-ui/aic_drivers/`

Files or code sections migrated from those paths must retain their original
`Copyright`, `SPDX-License-Identifier`, and `Authors` notices. The final source
file history must identify the original path and the material modification.

## Vendored ArtInChip SDK demos

`demos/official/` holds unmodified copies (their SDK `SConscript` files
excluded) of `packages/artinchip/lvgl-ui/aic_demo/{meter_demo,dashboard_demo,
slide_demo,multi_lang_demo,image_demo,demo_hub}` from the Luban-Lite SDK at
`fb5e9f61`, including their image, font and language assets (`demo_hub`:
1024x600 set only). Their original ArtInChip notices are kept in each file;
adaptation is limited to `demos/official/compat/` and the build files.
`compat/aic_ui_sdk.h` is the SDK `aic_ui.h`, unmodified.

This file must never contain credentials, access tokens, or private keys.
