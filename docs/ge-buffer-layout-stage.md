# GE RGB buffer layout preflight

The application-owned GE2D path now validates each packed RGB source and
destination draw buffer as a complete descriptor before cache maintenance or
DMA submission. IMAGE, LAYER and whole-display rotation use this same helper.
The row stride must contain one complete pixel row, fit the GE's 16-bit pitch
field, and describe an allocation large enough for every row. Width and height
remain within the bounded 4096 pixel GE domain, and the existing 32-bit
address-window check is still applied to the same descriptor.

This closes a boundary that the SDK leaves to the GE register programming:
malformed `stride` or `data_size` values could previously pass the address
check and reach cache/DMA setup. Lazy child layers still defer the check until
LVGL allocates their draw buffer. YUV frames retain their separate validated
plane/layout path.

## Validation

`ge2d_scale_contract` covers valid padded buffers, a short row, zero stride and
an allocation one byte short of the advertised footprint for both destination
and source descriptors. `ge2d_display_contract` covers the same malformed
inputs before whole-frame rotation. The full GE and no-GE host suites, SDK
GE2D build and manifest remain the acceptance gates; physical GE cache/pixel
and panel output remain **NOT_RUN**.
