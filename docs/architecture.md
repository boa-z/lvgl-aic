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

## Phase 3A

Phase 3A adds an independent GE2D draw unit, registered by `lv_aic_init()`
through `lv_draw_aic_ge2d_init()`. It claims exactly one kind of work —
`LV_DRAW_TASK_TYPE_FILL` that is opaque, has `radius == 0`, no gradient, a
destination format GE2D can write, and a buffer inside the GE address window —
and leaves every other task to the software renderer.

The unit runs synchronously on the dispatching thread: `ge_fillrect` ->
`mpp_ge_emit` -> `mpp_ge_sync`, all three return codes checked. There is no
render thread and no task queue, so the unit owns nothing but the task it is
executing, which is why it is an independent `lv_draw_aic_ge2d_unit_t` rather
than an alias of `lv_draw_sw_unit_t`. A GE failure marks the task FAILED, never
FINISHED.

It is gated on `mpp_ge_open()`: if the device is unavailable the unit is still
registered and declines every task, so the software renderer keeps the display
alive. The destination cache is prepared inside the backend for the touched
region only; the global LVGL draw-buffer handlers are deliberately left
untouched.

IMAGE, LAYER, scaling, rotation and asynchronous execution are Phase 3B and
later. MPP decoding, the FreeType cache, encoder and mouse remain separate.

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
