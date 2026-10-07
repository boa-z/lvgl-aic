# Bounded CPU recolor with GE composition

Ordinary RGB IMAGE and LAYER tasks can now combine recolor with the existing GE
copy, scale, rotation and tile paths. This is **native CPU recolor preparation
followed by GE transformation/composition**, not hardware recolor arithmetic.
The SDK reference rejects recolor; this stage extends the port rather than
closing a missing SDK baseline feature.

## Format, budget and lifetime

The pinned native LVGL recolor kernel is exposed through the existing generated
software-image translation unit. Its SHA guard still requires explicit review
when upstream changes. One row at a time is copied into aligned CMA storage;
RGB565, RGB888, XRGB8888, straight ARGB8888 and explicit/flagged premultiplied
ARGB8888 retain their native recolor rounding and format semantics. The original
decoder buffer is never changed, and the original decoder descriptor closes the
source. Full-source staging gives each tile and partial refresh identical input.

`AIC_LVGL_GE2D_RECOLOR_BYTES` defaults to 1 MiB; zero declines recolor preparation.
This is additional to the 2 MiB multipass transform budget. Dimensions, stride,
footprint, format, allocation and physical address are checked before DMA.
Tiled transform geometry is checked before transform allocations and commands;
CPU recolor storage can already exist at that point. A preparation rejection
uses the original native software descriptor. Uncertain submit/emit/sync retains
all scratch storage plus the decoder/task/target until reboot, with no replay.

Recolor combined with IMAGE bitmap masks, rounded image clips and special blend
modes remains software. Color keys now precede recolor through a separate
bounded staging path; see [combined stage](ge-recolor-colorkey-stage.md). LAYER
masks can now precede recolor on a private source copy; see [layer mask stage](ge-layer-mask-stage.md). Direct executor calls enforce these boundaries
as well as the scheduler. YUV recolor remains unsupported by the GE frame path;
the SDK `.fake` pseudo-fill continues to ignore image recolor deliberately.

## Pixel and validation boundaries

Native software normally transforms before recoloring; GE preparation recolors
before filtering. Quantization and repeated filtering can therefore differ,
especially RGB565, alpha/color boundaries and multipass transforms. This stage
does not promise arbitrary-edge pixel equivalence or improved frame time.

The descriptor-driven CPU GE model covers 432 IMAGE/LAYER scenes: six source
encodings, six tint opacities, two global opacities, native/scale/scale-plus-rotate
geometry, and both quadrants and color/alpha ramps. It compares 4,580,280 channel
values with actual native SW: all pixels at native size, constant interiors and
smooth interior ramps after transforms. Maximum observed error is 3 for native
size/constant interiors and 9 for transformed ramps (budget 10 includes one
RGB565 step). Edge/filter-halo equivalence is outside this comparison. Whole and
split refreshes are exact in the CPU model; this does not establish GE hardware
filter arithmetic.

Additional contracts cover one recolor allocation shared by nine tiles, a late
invalid tile rejected before DMA, original-source immutability, invalid bridge
footprints/formats and padding preservation, allocation/address failures, and
18 copy/scale/multipass submit/emit/sync failures. Direct key/rounded/special
blend fallbacks compare byte-for-byte with native SW.

Eighteen offscreen board probes require an actual engine outcome and compare
native size or constant transformed interiors to native SW across all six
formats. They also check clip guards. The final link gate requires the recolor
bridge. Physical execution and timing remain **NOT_RUN** for the unified board
validation stage. Combined host **86/86 PASS** (40.30 s) and the
FreeType/vector/SVG/Lottie-disabled baseline **79/79 PASS** (31.67 s).
Full D13x boot/app, final-link/static, image and clean-source manifest gates
**PASS**. All 29 file hashes and three source pins were independently verified.
Exact build identities are in [validation](validation.md). No SDK/core source
changes or physical execution were required.
