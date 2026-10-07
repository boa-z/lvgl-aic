# Straight ARGB targets: GE geometry and native final composition

Ordinary RGB IMAGE/LAYER tasks targeting straight ARGB8888 now use a bounded
intermediate surface. GE performs copy/conversion, scale, orthogonal rotation,
arbitrary rotation and multipass transformation without destination blending.
LVGL's native software blend then combines those transformed pixels with the
original target, applying global opacity once and retaining native output-alpha
semantics. Source recolor can precede the GE work as in the recolor stage.

This is hybrid rendering. ENGINE still means GE commands were executed; it does
not assert that all work was performed in hardware. The SDK normal/CMDQ control
helpers establish input and coefficient selection, not correctness of direct
hardware composition onto a partially transparent target. This path removes
that unresolved assumption from ordinary RGB IMAGE/LAYER target composition.
It does not establish that the SDK hardware datapath is defective.

## Bounds, caching and completion

`AIC_LVGL_GE2D_ALPHA_BYTES` defaults to 2 MiB and bounds the aligned surface.
Its extent is the intersection of the image's transformed bounds (or tile task
bounds), the refresh clip and the destination layer. Format, stride, footprint,
dimensions and address are checked before commands. Zero budget or insufficient
storage falls back to the original native SW descriptor. This budget is separate
from recolor (1 MiB) and multipass transforms (2 MiB), so all three paths can
own up to 5 MiB by default. No SDK allocator or LVGL checkout is modified.

The surface starts transparent. Raw commands clear premultiplication conversion
flags and disable GE blending; the stored representation is tracked separately
for the native final blend. Straight source alpha stays straight; transformed
premultiplied bytes stay premultiplied. All tile cells clip to their nonoverlapping
native grid rectangles and share a single staging surface.

The original target cache is prepared before any commands. After all GE syncs,
the intermediate cache is invalidated before native CPU blending; final CPU
target writes are cleaned before returning ENGINE so immediate readback or
display DMA sees them. Until that
point the original target is unchanged. Unsupported geometry falls back before
any target write. Uncertain DMA retains the intermediate, transform and recolor
storage plus the decoder/task/target; there is no replay or early release. Even
failure on a later tile preserves the original target and blocks further GE work.

Premultiplied **destination** formats are not introduced here. The later
[YUV target stage](ge-yuv-argb-stage.md) shares this staging helper. SDK `.fake`
pseudo-fills retain their separate path. The later [solid-fill stage](ge-fill-argb-stage.md)
adds bounded partial ARGB FILL/pseudo-fill staging with native XRGB composition.
Direct GE alpha composition remains separate work, outside this stage's claims.

## Evidence boundary

The descriptor-driven CPU GE model now tests 1,680 IMAGE/LAYER combinations:
six RGB/source-alpha encodings, seven recolor opacities including disabled,
two global opacities, native/scale/orthogonal-scale/multipass transforms,
quadrants and smooth color/alpha ramps, and XRGB/straight ARGB destinations.
ARGB also covers arbitrary/orthogonal rotation without scaling.
ARGB backgrounds include alpha 0/64/128/255 with different RGB values. Actual
native LVGL software drawing is the pixel oracle, including output alpha.

Across 14,820,456 compared channel values, maximum error on ARGB targets is 2
at native size, 4 on constant transformed interiors and 14 on transformed ramps.
The tests cap these at 3/5/16 respectively and alpha error at 3. GE filtering,
multiple resampling passes and recolor/filter order differ from native SW;
small errors can grow when low-alpha premultiplied colors return to straight
representation. Arbitrary edges and hardware arithmetic are not pixel-equivalent
claims. Whole/split refreshes are exact in the model.

Four allocation failures, address/oversized-budget rejection, all 19 staged
copy/scale/multipass/late-tile DMA failures and delayed CPU target writes are
covered. Nine transformed tiles share one surface; late invalid geometry is
rejected before DMA. Existing opaque destination and SDK control tests remain.
The previous descriptor-only scale test now explicitly uses XRGB destinations;
ARGB target behavior is exercised by the real pixel model rather than its
allocation-refusing command capture mock.

Eighteen additional offscreen board probes use straight ARGB backgrounds and
compare native/scale/multipass results to native SW, including output alpha and
clip guards. They run beside the existing eighteen XRGB recolor probes. Physical
GE/cache/CMA/filter precision, timing, memory peaks and panel acceptance remain
**NOT_RUN** pending the unified board session. Combined host **86/86 PASS**
(22.01 s), and FreeType/vector/SVG/Lottie-disabled baseline **79/79 PASS**
(20.26 s). Full D13x boot/app, final-link/static, image and clean-source
manifest gates **PASS**. All 29 file hashes and three clean source pins were
independently verified. Exact identities are in [validation](validation.md).
No SDK/core source changes, flashing or physical execution occurred.
