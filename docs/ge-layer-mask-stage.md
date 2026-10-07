# Bounded native IMAGE/LAYER masking before GE composition

RGB/ARGB IMAGE and ARGB LAYER tasks can combine centered A8/L8 masks with GE copy, scale,
orthogonal/arbitrary rotation, global opacity, bounded recolor and straight-alpha
target composition. This is native CPU mask preparation on a private CMA copy,
followed by GE transforms/composition. The SDK reference rejects bitmap masks;
this stage is an extension, not a missing SDK baseline feature or hardware mask
arithmetic. YUV mask composition is documented separately in
[bounded YUV masks](ge-yuv-mask-stage.md).

## Storage and native semantics

`AIC_LVGL_GE2D_MASK_BYTES` defaults to 1 MiB; zero disables mask preparation.
It is additional to recolor, transform and destination-alpha budgets. Source
format, dimensions, stride, footprint, budget and aligned physical address are
checked before DMA. LAYER sources accept straight ARGB8888, explicit
premultiplied and flagged premultiplied ARGB8888. Ordinary IMAGE sources accept
RGB565, RGB888, XRGB8888, straight ARGB8888 and premultiplied/flagged ARGB8888;
they are converted to a private four-byte buffer before masking. At most five
staging buffers coexist when alpha composition, masks, recolor and arbitrary-angle
scaling are combined.

The existing SHA-guarded software-image generator exposes the pinned native
`apply_mask` through private bridges. IMAGE staging centers against the whole
`image_area`; LAYER staging retains the partial layer's `buf_area`. Premultiplied
RGB and alpha both receive coverage. Invalid mask footprints are rejected before
pixel access; missing, wrong-format or malformed masks follow native unmasked
behavior. No mask/image intersection produces NOTHING without DMA. YUV sources
retain software fallback. Tiled IMAGE masks use the same source-sized private
copy and are applied per repeated cell; the decoder and staging allocation stay
owned across the complete tile preflight/submission pair.

The accepted GE path leaves original source and mask unchanged, preventing
repeated alpha multiplication during split refresh. Pre-DMA rejection uses the
original software descriptor; native software may modify the original layer
in place. This does not promise immutable sources on software fallback.
Uncertain submit/emit/sync retains all staging and the source decoder/task/target
until reboot, without replay. Rounded clips, special blend, recolor plus color
key and layer tiling retain the existing restrictions.

## Validation and limits

The descriptor-driven CPU GE model compares 581 scenes against native software
using an independent reference source, with A8/L8 masks, three ARGB encodings,
three mask sizes/alignments, four geometries, two opacities, recolor on/off and
opaque/transparent targets. There are 4,855,032 channel comparisons; maximum
observed error is 7 (native tolerance 3, transformed tolerance 8). Native-size
comparison covers all pixels; transformed comparison covers smooth interiors.
Hard edges and hardware filter arithmetic are not certified by this model.
Whole and split refreshes match exactly, and original source/mask stay intact.

The ordinary IMAGE subset adds five decoded source formats and checks private
ARGB staging, mask coverage, source immutability and software pixel parity. Its
premultiplied comparison allows the existing eight-channel GE/software rounding
bound; RGB565/RGB888/XRGB8888/straight ARGB use the native three-channel bound.

Six preparation failures cover all five allocation positions and address
rejection, with exact native fallback pixels. Nine submit/emit/sync failures
retain all five buffers and leave the original alpha target unchanged, with no
subsequent replay. Only the synchronous mock drains quarantined DMA for cleanup.
Three unsupported-effect LAYER cases verify software routing and exact native
pixels. Bridge tests cover footprint, budget, format, alias, source-area and
scratch-address rejection, empty intersection and native invalid-mask policy;
ordinary IMAGE YUV/unsupported-source cases retain software routing. Tiled
IMAGE masks are covered by the follow-on [tile stage](ge-image-mask-tile-stage.md).

Combined FreeType/vector/SVG/Lottie/manual-preview host: **89/89 PASS** (46.42 s).
Disabled baseline: **81/81 PASS** (33.08 s). The added effect-boundary assertions
also pass the refreshed focused multipass contract.

Thirty-six offscreen board probes cover three encodings, two mask formats,
three geometries and two target formats. Each requires actual ENGINE output,
checks original-source immutability and clip guards, and compares native-size
pixels or constant transformed interiors with independent native software.
They run at the start of the existing scale probe suite and log `PASS mask`.
Physical execution/cache coherency/performance remain **NOT_RUN**. Firmware
build evidence is recorded separately in [validation](validation.md).

Final D13x boot/app, final-link/static, packaged image and clean manifest **PASS**.
All 30 artifact hashes, three source commits and the SDK gitlink were independently
verified; the mask bridge and probe runner are live, with pass/fault markers in
allocated ELF `.rodata`. The target pointer-initialization correction is folded
into the implementation commit; refreshed GE/premultiplied contracts pass
**10/10** (4.27 s). Nine image CRCs and 41 fixture/provenance hashes also pass.
Exact implementation/build pins and image hashes are in [validation](validation.md).
