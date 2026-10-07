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

The GE2D backend (`draw/ge2d/`) owns an independent
`lv_draw_aic_ge2d_unit_t { lv_draw_unit_t base_unit; lv_draw_task_t *task_act; }`
and drives v9.6 task states explicitly: `evaluate` sets the preference score,
`dispatch` moves the task to `IN_PROGRESS`, the GE call runs synchronously, and
the task ends `FINISHED` or `FAILED`. There is no `LV_DRAW_TASK_STATE_READY` in
v9.6; the vendor 9.1 task state must not be copied into 9.6. Clip geometry comes from the task's
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

## Kconfig bools in manual-test translation units

Luban-Lite emits enabled Kconfig bools as empty defines
(`#define AIC_LVGL_USE_X`); `lv_conf.h` normalizes each listed symbol to
`0/1`, but only for translation units that include the LVGL config chain.
A new `tests/manual/lv_aic_*_test.{h,c}` pair must therefore include
`lv_aic_manual_test.h` (which pulls `lvgl_aic.h`) from its own header
_before_ any `#if defined(AIC_LVGL_USE_*) && AIC_LVGL_USE_*` guard is
evaluated. Without that include the feature `.c` compiles to an empty
object while the already-normalized caller keeps its reference, and the
link fails with `undefined reference to ..._poll` (seen three times:
meter, CAN capture). Garbage-collected entry points additionally need an
explicit `-Wl,-u` keep-alive in `SConscript`, following the existing
`AIC_LVGL_SMOKE_APP` blocks.

## MPP and cache

Image decoder work must document buffer ownership, cache maintenance, decoder
lifecycle, and fallback behavior. The current backend accepts FILE and borrowed
RAW/RAW_ALPHA memory JPEG/PNG with a bounded component-owned LRU. YUV output
remains absent. Source mutation/release requires lv_aic_mpp_cache_drop();
see [resource stage](resource-stage.md) for keys, budgets and active readers.

Decoder boundary (`image/mpp/`): `lv_aic_mpp_decoder_init/deinit` own a single
`lv_image_decoder_t`; `lv_aic_init/deinit` wire it as display -> input ->
decoder with owned-resource teardown. The LVGL <-> MPP translation lives in
`common/lv_aic_pixel_format.*`, shared with the GE2D unit so the switch exists
exactly once; `lv_aic_mpp_format.*` keeps only the decoder's acceptance policy
and is still the only thing that defines what the decoder will request or
accept. Stream helpers support `lv_fs` FILE and bounded read-only memory.
Decoded buffers must be built with `lv_draw_buf_init()` (valid `data`,
`unaligned_data`, `handlers`, `stride`, `data_size`); YUV/metadata hacks are
forbidden. CMA ownership is `allocation_base` -> `aligned_data` ->
`draw_buf.data`, freed from the base pointer when its final reader and cache
retention both end. Post-process heap output follows the same session lifetime.

GE2D boundary (`draw/ge2d/`): the unit owns no buffer and no thread. It
reads `layer->draw_buf->data` and `header.stride` directly and must not assume
`stride == width * bpp`. Destination cache is cleaned and invalidated per row
for the touched region only; the global LVGL draw-buffer handlers are not
overridden. The GE address window (`>= 0x40000000` on D13x/G73x) is checked
before every task.
