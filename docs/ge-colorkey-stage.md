# Bounded IMAGE color-key preparation

The SDK `lv_drivers/lv_ge2d` path exposes a single RGB comparator. LVGL's
`lv_image_colorkey_t` is wider: it accepts an inclusive RGB range, and the
software renderer applies that comparison after filtering and before the final
blend. The GE path therefore keeps the SDK comparator only for the simple
single-value RGB888/XRGB8888/straight ARGB8888 copy case.

For RGB565 sources, key ranges, premultiplied sources, antialiased images and
any scaled or rotated image, the application-owned executor copies the full
decoded source into a bounded CMA ARGB8888 buffer. The generated software-image
unit uses LVGL's `lv_color_is_in_range()` and RGB565 conversion, clears matching
pixels to transparent, and preserves the source alpha for the remaining
pixels. GE then performs the requested scale, right-angle or arbitrary-angle
rotation and destination composition from that immutable staging buffer.

The source is staged once per decoded image, so repeated IMAGE tiles and split
refreshes see identical key results. `AIC_LVGL_GE2D_COLORKEY_BYTES` defaults to
1 MiB; invalid format, stride, footprint, address or allocation declines the
GE path and replays the original descriptor through LVGL software. An uncertain
GE submission retains the decoder and staging buffer under the existing fault
quarantine rule. Recolor combined with a key, rounded clips, special blend
modes and YUV keys remain software.

This closes the application-level key semantics gap without claiming that the
GE hardware comparator supports ranges or a particular packed RGB565 space.
The direct descriptor path still rejects RGB565 and premultiplied keys when it
is called without the executor's preparation step.

## Host contract

`lvgl_aic_ge2d_scale_contract` now checks that ranges and transformed keys are
accepted by the draw unit, that the direct unprepared RGB565 descriptor still
declines safely, and that a RGB565 magenta range is converted to transparent
ARGB while a nonmatching green pixel remains opaque. The GE profile passes
71/71 tests and the no-GE profile passes 35/35. These contracts mock GE
submission and do not prove hardware comparator behavior, cache coherency or
panel pixels; those remain board validation items.
