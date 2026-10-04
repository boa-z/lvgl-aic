# GE2D draw unit

The synchronous backend evaluates FILL, IMAGE and LAYER.

- FILL: solid, unrounded, non-gradient tasks. Partial opacity supports
  RGB565/RGB888/XRGB8888; partial ARGB8888 remains software work.
- IMAGE: RGB565/RGB888/ARGB8888/XRGB8888, straight and premultiplied alpha,
  bounded scale, right-angle and arbitrary-angle rotation plus scale,
  clipped transformed tiling and supported exact color keys.
- LAYER: shares the RGB image executor, including scale and rotation. Default
  draw buffers can use the bounded CMA allocator; inaccessible/fallback heap
  storage stays with software.
- Immutable YUV frames use a separate validated lease/geometry path, including
  bounded scaling, orthogonal rotation and transformed tiling.
- Recolor, masks, rounded clips, non-normal blends and unsafe small/scaler-split
  geometry stay with software.

ROTATE requires source and clipped destination dimensions within 4..4096.
Its source and crop-relative destination centers use a conservative signed
14-bit domain. Translation is computed in 64 bits and rejected before cache
maintenance or submission if it cannot be represented. Native software still
has its own far-pivot precision limits; see [center checks](../../docs/ge-center-stage.md).

Arbitrary rotation plus scale uses [bounded multipass preparation](../../docs/ge-multipass-stage.md)
with a shared 2 MiB scratch budget, transparent input/output borders, Q16
phase planning and all-tile preflight. Submit/emit/sync are synchronous and checked. An uncertain DMA failure retains
the affected decoder/source lease and destination/task lifetime until reboot;
it never replays a software blend over potentially modified pixels. No SDK
source change is required. Per-buffer CMA handlers preserve allocation ownership.

Implementation: evaluation/dispatch in lv_draw_aic_ge2d.c, RGB and YUV executors
in their corresponding files, scale/rotation helpers separately, cache/address
guards in lv_draw_aic_ge2d_utils.c, format mapping in common/lv_aic_pixel_format.c.

The supplied board logs passed the original fill/blend/scale/CMA probes, and
the operator accepted the interface. Subsequent rotation, premultiplication,
YUV, tiling and other increments require consolidated physical validation.
Host mocks prove routing and descriptors; they do not prove GE pixel arithmetic.

See [capabilities](../../docs/capabilities.md) and
[exact validation records](../../docs/validation.md).
