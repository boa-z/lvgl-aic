# Architecture

## Boundaries

`lvgl-aic` is a platform-adaptation component. It owns no product UI and no
business state.

```text
D50T/application
      |
    LVGL API
      |
packages/custom/lvgl-aic
      |
Display / touch / cache / optional GE2D / optional MPP
      |
ArtInChip Luban-Lite BSP and MPP
```

## Phase 1

Phase 1 has one public integration entry point, `lv_aic_init()`:

1. the caller calls `lv_init()`;
2. the port sets the RT-Thread tick callback when the target OS is enabled;
3. the display port creates one LVGL display and installs the ArtInChip
   framebuffer flush callback;
4. the input port creates one LVGL pointer device and connects it to the
   ArtInChip touch device;
5. the caller owns `lv_timer_handler()` and all product UI code.

GE2D, MPP decoding, FreeType cache, encoder, and mouse are disabled for this
phase.

## Display ownership

The display port owns the `mpp_fb` handle, screen information, any temporary
software-rotation buffer, and the LVGL display object. The physical framebuffer
is owned by the BSP. The port must not free or reallocate it.

## Error handling

Initialization functions return a negative `lv_aic_result_t` on failure. They
must not print an error and continue with a partially initialized display or
input device.

The validation status is tracked in `docs/validation.md`; source inspection or
a host build is not hardware validation.
