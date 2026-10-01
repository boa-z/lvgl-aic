# GE2D draw unit

The synchronous backend evaluates FILL, IMAGE and LAYER, not only Phase 3A fills.

- FILL: solid, unrounded, non-gradient, supported/addressable destination.
  Partial opacity supports RGB565/RGB888/XRGB8888; partial ARGB8888 stays with
  software pending alpha-destination validation. Opaque ARGB8888 remains supported.
  The executor checks buffer size/stride and clips to task, clip and layer bounds.
- IMAGE: RGB565/RGB888/ARGB8888/XRGB8888, global/per-pixel alpha, bounded scales,
  right-angle rotation and unscaled arbitrary-angle rotation. Arbitrary-angle
  plus scale remains software work.
- LAYER: plain composition plus bounded scale/right-angle rotation. Ordinary
  D13x heap buffers fall back because GE cannot address them.
- YUV, masks, recolor and tiling stay software work. Small or unsafe scale
  regions and D13x split-risk cases fall back.

No owned render thread or buffers. Submit/emit/sync are synchronous and checked.
Engine failures are never retried as software blends over possibly modified
pixels. Cache prep is region-local; no global allocator/handler replacement.

Core evaluation/dispatch is in lv_draw_aic_ge2d.c; fill/image executors have
namesake files. Scale/rotation helpers are separate, address/cache helpers live
in lv_draw_aic_ge2d_utils.c, format mapping in common/lv_aic_pixel_format.c.

See [capabilities](../../docs/capabilities.md), [validation](../../docs/validation.md)
and [transform gates](../../docs/phase3c-transform.md).

The new fill path is a development candidate. Host contracts exercise the real
evaluator/executor with SDK ABI headers and mocked hardware. The optional manual
check adds 12 offscreen CMA pixel probes (three formats, four opacities), including
nonzero origins, clipping and padded-stride guards. Board execution is NOT_RUN.
