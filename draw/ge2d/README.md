# GE2D draw unit

The synchronous backend evaluates FILL, IMAGE and LAYER.

- FILL: solid and bounded two-stop horizontal/vertical PAD gradients on
  unrounded tasks. Partial opacity supports
  RGB565/RGB888/XRGB8888; straight ARGB8888 uses bounded GE staging plus
  native CPU composition. See [fill stage](../../docs/ge-fill-argb-stage.md).
- IMAGE: RGB565/RGB888/ARGB8888/XRGB8888, straight and premultiplied alpha,
  bounded scale, right-angle and arbitrary-angle rotation plus scale,
  clipped transformed tiling and supported exact color keys. The image
  executor prepares LVGL range/RGB565/premultiplied or filtered keys in a
  bounded ARGB8888 source copy before GE; direct descriptors still require the
  simple SDK comparator contract.
- LAYER: shares the RGB image executor, including scale and rotation. The
  evaluator applies the SDK-compatible child-source RGB format gate before
  claiming a task; allocated child buffers must agree with the layer format.
  Default draw buffers can use the bounded CMA allocator; inaccessible/fallback
  heap storage stays with software. Source and destination descriptors also
  pass the shared stride/footprint guard before cache or DMA. See [LAYER
  preflight](../../docs/ge-layer-preflight-stage.md) and [buffer layout
  preflight](../../docs/ge-buffer-layout-stage.md).
- Immutable YUV frames use a separate validated lease/geometry path, including
  bounded scaling, orthogonal rotation and transformed tiling. Straight-ARGB
  targets also apply bounded A8/L8 masks and inclusive LVGL color keys after
  CSC through the native alpha tail; direct orthogonal, repeated tiled and
  per-axis scaled masks are supported, including direct orthogonal
  rotated/scaled images, while rotated/scaled tiles and non-ARGB destinations
  remain software.
- Recolor and bounded A8/L8 masks are prepared by the application before GE;
  repeated IMAGE tiles reuse one source-sized mask staging copy. Rounded
  clips, non-normal blends and unsafe small/scaler-split geometry stay with
  software. YUV masks and color keys remain on the immutable frame path.

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
