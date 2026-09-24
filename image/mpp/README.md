# MPP image decoder (Phase 2A boundary)

- `lv_aic_mpp_format.*`: GE2D-independent LVGL <-> MPP mapping (SW RGB only).
- `lv_aic_mpp_stream.*`: `lv_fs` FILE stream helpers.
- `lv_aic_mpp_decoder.*`: owns one `lv_image_decoder_t`; info/open/close with
  no cache, no YUV hack, no GE2D.
- Lifecycle: `lv_aic_init` -> display -> input -> decoder; `lv_aic_deinit`
  reverses decoder -> input -> display.
- Buffers: CMA `allocation_base` -> `aligned_data` -> `draw_buf.data` via
  `lv_draw_buf_init()`; close frees the base pointer.
- Phase 2A supports `LV_IMAGE_SRC_FILE` JPEG/PNG only; all other sources and
  corrupt/missing files must fail safe with `LV_RESULT_INVALID`.

## Board test assets (non-product)

Place three files on the target DFS before judging decode:

```text
/data/mpp_test/a.jpg  # baseline JPEG, e.g. 800x480 truecolor
/data/mpp_test/b.png  # RGB PNG, e.g. 320x240 color_type 2
/data/mpp_test/c.png  # RGBA PNG, e.g. 320x240 color_type 6
```

The manual page shows all three at once (`lv_image` A/B/C) to prove decoder
sessions are independent. Missing files render as LVGL placeholders; the page
must never assert, hang, or leak on `invalid extension / corrupt / zero-byte /
missing / unsupported` inputs.

## Lifecycle and stress

- `lv_aic_init/deinit` stays repeatable with the decoder registered (no list
  duplication, double free, or dangling callback).
- Each decode session owns its stream, MPP handle, CMA buffer, and draw_buf;
  `close` releases CMA base and any post-process heap buffer.
- Stress target: >= 1000 create/load/render/delete cycles with heap/CMA/PSRAM
  sampled before/during/after; no growth in MPP handles or LVGL objects.
- Stats per decode (`decode_time_ms`, decoded/CMA bytes, w/h/cf) come from
  `lv_aic_mpp_decoder_last_stats()` for JPEG-vs-SW comparisons later.
