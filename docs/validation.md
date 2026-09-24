# Validation record

## Status

This file is intentionally explicit about unverified work.

| Date | Commit | SDK commit | LVGL commit | Board | Result | Notes |
|---|---|---|---|---|---|---|
| 2026-09-24 | local bootstrap | `c5807f9e7d18292f920dafaa018b8174635085c4` | `80ca777e37a2b176770726a02e07a6fb79ef0b39` | none | partial | LVGL 9.6 host compile/link and platform-only smoke page passed; no AIC BSP hardware |

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

- LVGL 9.6.0 CMake configure/build: PASS;
- `lvgl_aic` public API and Phase 1 port sources: PASS;
- platform-only manual smoke page: PASS;
- compile-time rejection of the repository's LVGL 9.1 header: PASS;
- AIC BSP target compile: syntax-checked with API stubs only; not a board build.

The full LVGL upstream build emits existing MSVC code-page and enum warnings;
those warnings are not attributed to `lvgl-aic`. The component sources were
additionally checked with `-Wall -Wextra -Werror` using the target-facing
stubs.

## Hardware policy

If no physical D133ECS board is available, report:

```text
Build validation: PASS or FAIL
Hardware validation: PENDING
```

Do not convert a host simulator result into a hardware claim.
