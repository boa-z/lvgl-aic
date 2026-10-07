# Bounded YUV bitmap masks

The YUV GE path now supports a narrow bitmap-mask extension for straight
ARGB destinations. GE performs the existing CSC, clipping and source geometry
into its private ARGB surface. A native alpha pass then multiplies each
converted pixel by an A8 or L8 mask before the existing LVGL source-over tail
composites it into the destination.

The GE-to-CPU handoff invalidates the staging cache before the mask read; the
existing alpha tail invalidates again after the CPU write and cleans the final
destination. The producer lease remains held across both cache boundaries.

The extension accepts an A8/L8 mask whose dimensions match the immutable YUV
frame. Direct images support the four orthogonal GE rotations; unscaled
repeated tiles use modulo source coordinates from the LVGL tile anchor. A
non-rotated, non-tiled scale reuses the exact Q16 source phase already sent to
GE, so the mask samples the same integer source pixel as the scaler. The mask
is interpreted in source coordinates, including a caller-supplied
`image_area`; clipped refreshes address the corresponding mask subrange. Direct
images also support all four orthogonal rotations combined with per-axis
scaling and non-zero pivots, using the same signed Q16 inverse phase as the GE
crop descriptor. Repeated tiles use that same inverse phase per cell for
orthogonal rotation and independent scaling. Overlapping transformed cells
follow LVGL's draw order, while rotated gaps retain their destination bytes.
Invalid dimensions, strides, storage, formats or coordinates decline before
CMA allocation, cache maintenance or GE submission. Recolored, keyed, rounded
or non-normal YUV requests retain the existing software fallback.

This keeps the SDK hardware boundary explicit. The SDK GE comparator and CSC
unit still do not provide YUV mask or key operators; the application-owned
alpha tail is a bounded composition extension and does not claim direct YUV
mask acceleration.

## Host contract

`lvgl_aic_ge2d_yuv_contract` covers a 32x16 I420 frame with an alternating A8
mask, verifying direct, orthogonally rotated, unscaled tiled, transformed tiled,
1.5x non-rotated and all four orthogonal rotated/scaled draws. A
rotated/scaled non-zero-pivot case checks that absolute destination coordinates
do not leak into the inverse phase. Zero-mask source
pixels preserve every destination byte and full-mask pixels render through the
raw GE CSC output. The scaled cases check the exact integer or signed-floor
Q16 mask phase; transformed tile overlaps and rotation gaps are checked against
the opaque GE reference.
The full configured host suite passes
**71/71**; physical CSC, cache, timing and panel acceptance remain **NOT_RUN**.
