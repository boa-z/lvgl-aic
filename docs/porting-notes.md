# Porting notes

This file records the differences that matter when moving from the ArtInChip
LVGL 9.1 reference implementation to LVGL 9.6.

## Configuration

- LVGL 9.6 uses `LV_COLOR_FORMAT_DEFAULT`; do not copy the old
  `LV_COLOR_DEPTH` assumption into new port code.
- `LV_USE_THORVG` and `LV_USE_THORVG_INTERNAL` are separate v9.6 options.
- The upstream CMake interface uses `LV_BUILD_CONF_PATH` and
  `CONFIG_LV_BUILD_*` options. The Luban-Lite SCons integration must provide
  the matching configuration path rather than relying on old CMake variables.

## Display

The v9.6 display API uses:

- `lv_display_create()`;
- `lv_display_set_color_format()`;
- `lv_display_set_flush_cb()`;
- `lv_display_set_buffers_with_stride()` when the BSP stride is authoritative;
- `lv_display_set_rotation()`;
- `lv_display_flush_ready()`.

Buffer sizes passed to LVGL are bytes. The ArtInChip physical stride remains a
BSP property and must not be confused with the LVGL source stride.

## Input

The v9.6 input API uses `lv_indev_create()`, `lv_indev_set_type()`,
`lv_indev_set_read_cb()`, and `lv_indev_set_display()`. Touch event state is
copied into the LVGL read callback; the callback must not retain a pointer to
a temporary BSP event.

## Draw unit boundary

The old ArtInChip implementation aliased its GE2D unit to
`lv_draw_sw_unit_t`. That is not acceptable for the new component. GE2D must
have an independent unit structure and must explicitly handle v9.6 task states
(`WAITING`, `QUEUED`, `IN_PROGRESS`, `FINISHED`, and `FAILED`). This work is
not part of Phase 1.

## ArtInChip framebuffer details

`aicfb_screeninfo.smem_len` describes one visible plane on the D13x BSP even
when `AIC_PAN_DISPLAY` has reserved two planes. The port therefore computes
`plane_size = height * stride` and must not reject PAN mode by comparing
`smem_len` with `2 * plane_size`.

Component-owned rotation buffers use `aicos_malloc_align(MEM_CMA, ...)` and
`aicos_free_align()`. They do not call the LVGL-9.1-era
`aicos_malloc_try_cma()` path, which is coupled to the legacy image-cache
decoder in the old SDK port.

## MPP and cache

Image decoder work must document buffer ownership, cache maintenance, decoder
lifecycle, and fallback behavior. No MPP decoder source is part of the Phase 1
baseline.
