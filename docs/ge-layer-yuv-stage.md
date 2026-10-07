# GE YUV LAYER adapter

The ArtInChip SDK accepts `I420`, `I422`, `I444` and `I400` as LAYER source
formats. Its decoder path relies on an internal convention where
`lv_draw_buf_t.data` points to a physical `struct mpp_buf`, while the generic
LVGL 9.6 layer allocator creates an ordinary packed byte buffer. Treating an
unbound application layer as that SDK descriptor would read arbitrary bytes as
plane addresses.

The application port now exposes the convention explicitly through
`lv_aic_yuv_layer_attach()` in `lv_aic_yuv_layer.h`. The caller supplies an
immutable planar `lv_aic_yuv_frame_t`, matching layer color format, and the
same retain/release callbacks used by published YUV images. The adapter checks
the planar layout and capacity before retaining the producer. Detach retires
the binding and releases it after any synchronous GE reader completes.

The GE evaluator admits a YUV layer only while that binding exists and its
planes pass the MPP physical address, stride, size and color-space checks.
Dispatch reuses the existing YUV IMAGE submit path, including clipping,
orthogonal rotation, bounded per-axis scaling, cache preparation, straight
ARGB masks/color keys and fault quarantine. An ordinary YUV layer with no
binding remains with LVGL software and is never interpreted as an MPP
descriptor. RGB layer behavior is unchanged.

## Validation

`ge2d_yuv_contract` submits an attached I420 LAYER and verifies the MPP format,
plane address and lease release. `ge2d_scale_contract` verifies that an
unbound YUV layer is declined, an attached physical frame is admitted, and a
stale packed layer descriptor remains declined. Both targeted GE contracts
pass. Board GE pixels, cache coherency, timing and panel acceptance remain
**NOT_RUN**.
