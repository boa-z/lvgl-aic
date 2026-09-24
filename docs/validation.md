# Validation record

## Status

This file is intentionally explicit about unverified work.

| Date | Commit | SDK commit | LVGL commit | Board | Result | Notes |
|---|---|---|---|---|---|---|
| 2026-09-24 | `531cb8138b0ae60445814ba671a26c2607a7f3fb` | `c5807f9e7d18292f920dafaa018b8174635085c4` | `80ca777e37a2b176770726a02e07a6fb79ef0b39` | none | partial | External LVGL 9.6 host configure/build and CTest smoke test PASS; real D13x target build and board validation pending |

## Required Phase 1 evidence

- [ ] LVGL 9.6 compile
- [ ] LVGL 9.6 link
- [ ] framebuffer display
- [ ] 800x480 software-rendered surface
- [ ] touch read callback
- [ ] continuous refresh
- [ ] VSync/PAN behavior
- [ ] rotation behavior or documented limitation
- [ ] cache coherency check
- [ ] no legacy `lvgl-ui` dependency
- [ ] no LVGL upstream modification

## Host build evidence

The following checks were run against an external temporary checkout of the
pinned LVGL source; no LVGL source is stored in this repository. The
reusable command is documented in [`tests/host/README.md`](../tests/host/README.md):

- LVGL 9.6.0 CMake configure/build: PASS (external checkout, 2026-09-24);
- `lvgl_aic` public API and Phase 1 port sources: PASS;
- platform-only manual smoke page: PASS;
- CTest `lvgl_aic_platform_smoke`: PASS (1/1);
- host component targets use `-Wall -Wextra -Werror` on GCC/Clang;
- `.github/workflows/host.yml` checks out the pinned LVGL commit and runs the
  same host smoke test;
- compile-time rejection of the repository's LVGL 9.1 header: PASS;
- AIC BSP target compile: pending the real D13x build below; host stubs are
  not counted as target evidence.

The full LVGL upstream build emits existing MSVC code-page and enum warnings;
those warnings are not attributed to `lvgl-aic`. The component sources were
additionally checked with `-Wall -Wextra -Werror` using the target-facing
stubs.

## Phase 1.5 target build evidence

The real target build was run in the isolated Luban-Lite integration checkout,
not in the parent working tree with its unrelated uncommitted changes:

- SDK baseline: `c5807f9e7d18292f920dafaa018b8174635085c4`;
- LVGL: `80ca777e37a2b176770726a02e07a6fb79ef0b39` (`v9.6.0`);
- component: `84467055fc0d33fdd6d2040b8d1599a9684434cd`;
- application defconfig:
  `d13x_d50t-2-lite_rt-thread_lvgl-aic-smoke_defconfig`;
- bootloader prerequisite defconfig:
  `d13x_d50t-2-lite_baremetal_bootloader_defconfig`;
- toolchain: Xuantie-900 GCC V2.6.1 B-20220906, GCC 10.2.0;
- bootloader `d13x.elf`: PASS;
- application `d13x.elf`: PASS, 9,017,864 bytes;
- application image generation: PASS;
- static link-map check: PASS (`lv_init`, `lv_display_create`, and
  `lv_obj_create` resolve to `packages/third-party/lvgl/src`; no
  `packages/artinchip/lvgl-ui` or `lvgl_v9/lvgl` object appears);
- compile-time `lv_conf.h` marker probe: PASS;
- Kconfig touch symbol: generated as
  `#define AIC_LVGL_TOUCH_DEVICE "gt911"`;
- forbidden Phase 1 symbols: `AIC_LVGL_USE_GE2D`,
  `AIC_LVGL_USE_MPP_DEC`, and `AIC_LVGL_USE_FT_CACHE` remain disabled.

The target build enables `LPKG_MPP` only for framebuffer/BSP infrastructure;
it does not add an LVGL MPP image decoder or GE2D implementation. The build
uses the platform-only smoke application, not the D50T product UI.

The only compiler diagnostics observed in the LVGL OSAL are pre-existing
`%d`/`rt_err_t` format warnings from upstream `lv_rtthread.c`; the build is
not warning-clean because the pinned upstream source is intentionally not
modified.

## Hardware policy

If no physical D133ECS board is available, report:

```text
Build validation: PASS or FAIL
Hardware validation: PENDING
```

Do not convert a host simulator result into a hardware claim.
