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
