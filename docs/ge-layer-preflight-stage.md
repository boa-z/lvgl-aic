# GE LAYER source preflight

The application-owned GE evaluator now applies the same source-side format
gate as the ArtInChip SDK before claiming an `LV_DRAW_TASK_TYPE_LAYER` task.
The child layer's declared format must be one of the four direct RGB formats
that the GE blitter can read: RGB565, RGB888, XRGB8888 or ARGB8888. Explicit
`ARGB8888_PREMULTIPLIED` is also accepted for LAYER sources; the executor maps
it to ARGB8888 with the MPP premultiplied flag. A child draw buffer, when
already allocated, must also agree with that declaration.

Child layers are allocated lazily by LVGL, so evaluation intentionally does not
require a buffer address, stride or cache-safe storage. Dispatch still performs
the existing address, footprint and effect preparation checks after allocation.
An empty layer therefore remains a successful no-op, while an unsupported YUV
layer or a stale layer/buffer format stays with the software renderer from the
start. This keeps draw-unit preference and `layer_accepted` counters truthful
and avoids opening a decoder only to discover a source-format mismatch.

The SDK reference performs the equivalent `ge2d_src_fmt_supported()` check in
its LAYER evaluator. This stage closes that parity gap without changing the
supported pixel set or touching the upstream LVGL submodule.

## Validation

`ge2d_scale_contract` now covers supported child formats, lazy child buffers,
unsupported YUV declarations and stale layer/buffer descriptors. GE and no-GE
host suites remain the acceptance boundary for this stage; board GE pixels,
cache coherency and panel output remain **NOT_RUN**.
