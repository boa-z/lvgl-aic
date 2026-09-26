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
`lv_draw_sw_unit_t`, which coupled the hardware unit to the software unit's
thread, sync and saved layer/clip fields. The new component does not do that.

Phase 3A (`draw/ge2d/`) owns an independent
`lv_draw_aic_ge2d_unit_t { lv_draw_unit_t base_unit; lv_draw_task_t *task_act; }`
and drives v9.6 task states explicitly: `evaluate` sets the preference score,
`dispatch` moves the task to `IN_PROGRESS`, the GE call runs synchronously, and
the task ends `FINISHED` or `FAILED`. There is no `LV_DRAW_TASK_STATE_READY` in
v9.6; the vendor port's use of it was wrong. Clip geometry comes from the task's
own `clip_area` and `target_layer`, never from a field cached on the unit.

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

Phase 2 boundary (`image/mpp/`): `lv_aic_mpp_decoder_init/deinit` own a single
`lv_image_decoder_t`; `lv_aic_init/deinit` wire it as display -> input ->
decoder with reverse teardown. The LVGL <-> MPP translation lives in
`common/lv_aic_pixel_format.*`, shared with the GE2D unit so the switch exists
exactly once; `lv_aic_mpp_format.*` keeps only the decoder's acceptance policy
and is still the only thing that defines what the decoder will request or
accept. Stream helpers use `lv_fs` FILE sources only.
Decoded buffers must be built with `lv_draw_buf_init()` (valid `data`,
`unaligned_data`, `handlers`, `stride`, `data_size`); YUV/metadata hacks are
forbidden. CMA ownership is `allocation_base` -> `aligned_data` ->
`draw_buf.data`, freed from the base pointer on close.

Phase 3A boundary (`draw/ge2d/`): the unit owns no buffer and no thread. It
reads `layer->draw_buf->data` and `header.stride` directly and must not assume
`stride == width * bpp`. Destination cache is cleaned and invalidated per row
for the touched region only; the global LVGL draw-buffer handlers are not
overridden. The GE address window (`>= 0x40000000` on D13x/G73x) is checked
before every task.
