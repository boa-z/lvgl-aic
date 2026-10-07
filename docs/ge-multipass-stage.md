# Bounded GE scale and arbitrary rotation

The SDK ROTATE operation explicitly disables its scaler. The application draw
unit now combines separate commands for ordinary RGB IMAGE and LAYER requests
that use both scale and a non-orthogonal angle:

1. Copy/convert the decoded image into a zeroed ARGB8888 buffer with a two-pixel
   transparent border. Straight input uses SDK `GE_PD_SRC` with premultiplied
   output; already-premultiplied input copies its stored bytes unchanged.
2. Scale those stored premultiplied channels with explicit Q16 steps and phases,
   without alpha conversion. The scaled buffer has its own two-pixel transparent
   border, including when the input border becomes subpixel during downscaling.
3. Rotate/composite the scaled buffer, applying the requested global opacity
   once, at the final destination. Tiled requests share steps 1/2 and the two
   allocations across all cells.

The source center is an integer on the scaled grid. The phase carries the
fractional pivot residual; the final destination center remains the original
image origin plus original pivot. Neither scratch dimensions nor placement
depend on the refresh clip. Coordinates and phase planning use 64-bit arithmetic.

## Bounds and lifetime

`AIC_LVGL_GE2D_TRANSFORM_BYTES` limits the combined, simultaneously owned CMA
allocations including row alignment and both borders. Default: 2 MiB; zero
disables preparation. Both allocations and physical address checks must succeed
before commands. All tile geometry is preflighted before transform allocation/cache/DMA,
including the final clipped cell. Allocation/geometry rejection uses native
software without a partial hardware blend.

Source dimensions must be 4..4092; the scaled storage and ROTATE destination
must fit the SDK's 4..4096 limits. Scale remains 1/16..16, and the known scaler
split-risk interval now uses the subsequent [RGB stripe planner](ge-stripes-stage.md). The resampled source center and cropped
destination center must fit the conservative signed 14-bit policy. Tiny clips,
excessive memory, unsupported formats, keys, masks, skew and special
blend modes retain the existing software paths. Recolor now uses the separately
bounded [CPU preparation stage](ge-recolor-stage.md); its storage may exist
before transform geometry preflight.

Every command, emit and sync is checked. If completion is uncertain, both
scratch allocations remain alive with the existing decoder/source lease and
destination/task quarantine. Subsequent GE requests are refused until reboot.
There is no attempt to replay a possibly modified target through software.
After success the two allocations are freed before the decoder closes.

## Evidence boundary

The new host test runs the real executor against a descriptor-driven CPU model
of copy/premultiply, bilinear scale and rotation/composition. It exercises 288
IMAGE/LAYER combinations (RGB565/RGB888/XRGB8888, straight and both premultiplied
ARGB encodings), nonuniform/fractional scales, three angles and two opacities.
104,520 interior pixels are compared against an independent inverse transform
and alpha blend, with a tolerance of two values per color channel. Full and split refreshes
are byte-identical. Nine tiles share one preparation; a later one-pixel cell
declines before any allocation or command. Both allocations, address rejection
and all nine submit/emit/sync failure points are exercised. Geometry probes also
check the inverse sampling relation and the combined budget threshold.

The actual SDK normal and CMDQ alpha helpers are compiled and checked for the
preparation conversion, raw scale and final opacity control states. These tests
establish command construction and ownership, **not hardware pixel arithmetic**.
Repeated filtering can differ from LVGL's single-pass bilinear edges; exact edge
equivalence and target timing are not claimed.

Eight new offscreen board probes cover a 33-degree nonuniform scale and tiled
45-degree scale with RGB, straight ARGB and both premultiplied encodings. They
require an engine outcome, independent ramp pixels and intact clipping guards.
They run with the existing GE scale probes in the smoke firmware; the
2026-10-07 board run passed all eight (`max_error<=2`, engine outcomes and
intact clip guards).

Combined host **81/81 PASS** and vector/SVG/Lottie-disabled baseline **76/76
PASS**. Full D13x boot/app compilation, live-symbol checks, image and clean-source
manifest gates also pass. Exact source/image identities are recorded in
[validation](validation.md). No SDK or upstream LVGL source is modified.
