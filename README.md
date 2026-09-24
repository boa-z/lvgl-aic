# lvgl-aic

`lvgl-aic` is an independent ArtInChip platform-adaptation component for LVGL 9.6.x.

It connects the LVGL public API to Luban-Lite/RT-Thread and the ArtInChip display,
touch, framebuffer, cache, and MPP interfaces.

## Scope

This repository **does not contain LVGL upstream source** and is not a D50T product
UI project. It also does not contain product pages, CAN/CANopen, vehicle logic,
Path2D, PSD tooling, or product assets.

The component is designed to be consumed as:

```text
D50T Application
        |
      LVGL
        |
 packages/custom/lvgl-aic
        |
 ArtInChip Luban-Lite BSP / MPP
```

## Fixed baseline

- LVGL: `v9.6.0`, commit `80ca777e37a2b176770726a02e07a6fb79ef0b39`
- Luban-Lite reference: `v1.3.2`, commit
  `c5807f9e7d18292f920dafaa018b8174635085c4`
- Primary target: ArtInChip D13x / D133ECS, RT-Thread

The component intentionally rejects LVGL versions other than 9.6.x at compile
time. This is a platform baseline, not a compatibility layer for every LVGL
minor release.

## Current implementation status

Phase 0/Phase 1 bring-up is under development:

- repository/build skeleton;
- LVGL 9.6 version guard and configuration;
- RT-Thread software-renderer display baseline;
- ArtInChip framebuffer/VSync/rotation integration;
- touch input baseline;
- GE2D and MPP decoder intentionally remain disabled until their gates pass.

No hardware validation is claimed by the repository until the validation record
contains board-specific evidence.

## Integration

The intended superproject layout is:

```text
packages/
├── artinchip/
│   └── lvgl-ui/       # legacy reference; do not link with the new path
├── third-party/
│   └── lvgl/          # LVGL v9.6.0 upstream submodule
└── custom/
    └── lvgl-aic/      # this repository
```

The host application must choose exactly one of the legacy ArtInChip LVGL path
or the LVGL 9.6 + `lvgl-aic` path. Both LVGL implementations must never be
linked into the same image.

See [`docs/integration-luban-lite.md`](docs/integration-luban-lite.md) for the
Kconfig, SCons, submodule, and legacy/new selection procedure.

## Minimal initialization

The caller owns the LVGL lifecycle and the LVGL task loop:

```c
lv_init();

if (lv_aic_init() != 0) {
    /* Stop startup and report the platform initialization error. */
}

for (;;) {
    lv_timer_handler();
    rt_thread_mdelay(10);
}
```

`lv_aic_init()` creates only the display and input integration. It does not
create product pages, start protocol threads, or own the application UI loop.

## Documentation

- [`docs/architecture.md`](docs/architecture.md)
- [`docs/compatibility.md`](docs/compatibility.md)
- [`docs/porting-notes.md`](docs/porting-notes.md)
- [`docs/integration-luban-lite.md`](docs/integration-luban-lite.md)
- [`docs/validation.md`](docs/validation.md)

## License and source notices

See [`NOTICE.md`](NOTICE.md). Code migrated from ArtInChip sources must retain
its original copyright, SPDX identifier, and author information.
