# GE premultiplied image sources

The application GE backend now accepts both explicit
`LV_COLOR_FORMAT_ARGB8888_PREMULTIPLIED` sources and ARGB8888 sources marked
`LV_IMAGE_FLAGS_PREMULTIPLIED`. This includes native canvas/Lottie storage and
post-processed decoded images. IMAGE and LAYER share the same handling; supported
BITBLT scaling/orthogonal rotation/tiling and arbitrary-angle ROTATE keep their
existing geometry, address and completion checks. No SDK or LVGL source checkout
is modified.

The SDK's normal and CMDQ backends already understand `MPP_BUF_IS_PREMULTIPLY`.
The port now publishes that flag alongside the physical ARGB8888 layout. With
full global opacity, source alpha mode 0 lets the SDK use premultiplied channels
directly with coefficients `(1, 1-As)`. Partial global opacity uses mixed mode 2,
SDK depremultiplication and `(As, 1-As)`. Selecting SRC_OVER alone would omit the
global colour scaling. Straight sources retain their prior equations.

This is image composition, not GE vector/Lottie rasterization. Premultiplied
source colour keys retain software comparison; premultiplied destinations are
not added in this stage. Ordinary straight-ARGB destination composition is now
handled by the later [GE staging/native blend stage](ge-argb-target-stage.md).
Hardware rounding, filtering and physical throughput still need board evidence. CPU-only or otherwise
unsupported sources retain the existing software fallback.

## Software fallback consistency

Pinned LVGL 9.6 software drawing selected blending from the format enum alone,
even when a decoder published the premultiplied flag. `stage_sw_premult.py`
generates a build-local correction at the four native source-format selections:
plain images, rounded clipping, recolour and transformed recolour. The source
SHA256 and patch-site count are checked; an upstream change fails with a review
request. Both SCons and host CMake compile this correction and preserve upstream
copyright headers. No pixel bytes or caller descriptor are mutated by the fix.
Target checks verify generated contents and the linked `lv_draw_sw_image` object;
the generated source is included in the image manifest. Python is required for
host CMake configuration as well as the SDK build.

The Lottie wrapper still publishes an explicit format before canvas caching, so
its representation is also correct for consumers outside this software path.

## Validation and board probes

Combined host suite **77/77 PASS**; baseline **74/74 PASS**. The GE contract traverses actual
decoding and captures engine commands for both representations, three global
opacities, image/layer, scaled orthogonal rotation, arbitrary rotation and tiling.
Address-rejected sources render real software pixels. These capture mocks do not
simulate or prove hardware pixels. The software contract compares explicit/flag
pixels across opacity, rounded clipping, recolour, rotation/scaling and colour key.
Actual SDK normal/CMDQ alpha helper functions are compiled and run at all 256
global opacity values; this proves control selection, not the hardware datapath.

`lv_aic_ge2d_scale_test_run` now includes six native executor probes comparing
real GE output to native software for both source representations at global
opacity 64/128/255. Sources contain alpha 0/64/128/255; all colour/alpha channels
and exact outside-clip guards are checked, with three levels of channel tolerance.
Existing scale/tile/orthogonal-rotation ramp probes also include both premultiplied
representations. An uncertain DMA completion retains probe storage until reboot.
These probes are compiled into the smoke image; physical execution is **NOT_RUN**.

Combined D13x boot/app, final-link/static, image and clean-source manifest gates
**PASS**. Exact pins and image SHA256 are in [validation](validation.md).
The build helper accepts
`-EvidenceTag premult` to preserve this candidate separately from the preceding
Lottie-stage image. Hardware validation remains deferred; no flashing.
