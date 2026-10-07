# Bounded YUV color-key composition

The YUV GE path now accepts LVGL color keys when the destination is a straight
ARGB draw buffer. GE performs the existing immutable-frame CSC, clipping,
scaling, rotation and tile work into the private ARGB staging surface. After
GE completion, a bounded CPU pass applies LVGL's inclusive RGB range to the
converted pixels and clears alpha for matching pixels; the existing opaque
coverage tail then leaves those destination pixels untouched.

The key is evaluated on the final CSC RGB values, so the same rule applies to
orthogonal and arbitrary supported transforms, partial refreshes and repeated
tiles without changing producer storage. Cache invalidation occurs before the
CPU read and the normal alpha tail invalidates again before the final
destination clean. The YUV source lease and staging surface remain owned until
the GE submission has completed or is quarantined after an uncertain failure.

This is an application-owned composition extension. ArtInChip's SDK comparator
is a packed single-value RGB operator and does not provide a YUV key operator;
the implementation therefore does not claim direct YUV key acceleration.
Non-ARGB destinations, recolor+key, unsupported blend modes and malformed
staging buffers retain the existing software fallback.

## Host contract

`lvgl_aic_ge2d_yuv_contract` checks an exact inclusive key on a direct I420
image and on repeated tiles. Matching pixels preserve the pre-existing target,
while an adjacent nonmatching CSC pixel is rendered. The full configured host
suite passes **71/71**. The final D13x GE2D/widgets/player build, static gates,
image checks and manifest also pass; its clean evidence is recorded under
`output/lvgl-evidence/ge2d-widgets-player-yuv-colorkey-final` with image
SHA256 `b3d4c2e72b1d4c42274b497e4423e858d3b41aa870e86ddd353a905c800f2841`.
Physical CSC, cache, timing and panel acceptance remain **NOT_RUN**.
