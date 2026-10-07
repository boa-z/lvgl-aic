# Native vector source-in, destination-in and bounded replacement

The pinned software backend treated SRC_IN and DST_IN as source-over. NONE used
the native replacement blender without a separately verified coverage contract.
The new regression records the former SRC_IN failure in
`output/vector-coverage-before.log`. This stage supplies these three modes in
the application-owned generated backend; all nine native vector enum values
now have analytical and real-raster coverage in the tested scope.

This is software vector correctness, not new GE vector acceleration. The SDK
and upstream LVGL sources remain unchanged. The source hash guard, final-link
owner check and generated-source provenance still apply.

## Geometry and composition

The new modes operate within each native subtask's covered geometry, intersected
with its scissor and the outer task clip. A separate coverage pass records path
fill/stroke coverage in A8 independently of paint alpha. Transparent paint inside
a path can clear the target; an even-odd hole or pixel outside the path cannot.
A fill/stroke whose descriptor opacity is zero contributes no geometry, consistent
with the native path enablement convention. A queued task with opacity zero, a
solid color alpha zero or transparent gradient/image pixels still distinguishes
covered from uncovered pixels.

With premultiplied source S, destination D, source alpha Sa, destination alpha Da
and geometric coverage q normalized to 0..1, the bounded operators are:

| Mode | Output premultiplied color | Output alpha |
|---|---|---|
| SRC_IN | S * Da + D * (1 - q) | Sa * Da + Da * (1 - q) |
| DST_IN | D * (Sa + 1 - q) | Da * (Sa + 1 - q) |
| NONE | S + D * (1 - q) | Sa + Da * (1 - q) |

S and Sa already include coverage. Interior behavior follows the native LVGL/
VG-Lite source-in, destination-in and replacement formulas; the uncovered
destination is retained at antialiased edges. Small raster rounding is bounded
by enforcing q >= Sa. All paints within one vector subtask form its source before
composition, including fill/stroke overlap. This is not a claim of SVG document
group-isolation or unbounded whole-canvas Porter-Duff semantics.

Pattern coverage uses the same native picture sampler and clip as the actual
ARGB image. A geometric rectangle alone was insufficient: rotated image-edge
coverage differed from native picture sampling. The opaque coverage picture
includes transparent source pixels but retains the image extent and transform.
Pattern pixels remain immutable.

## Storage and failure ownership

The existing ARGB source surface first holds coverage, whose alpha is copied to
a lazily allocated A8 buffer; the ARGB surface is then cleared and reused for
actual paint. Target/source/A8 buffers share `AIC_LVGL_VECTOR_SURFACE_BYTES`
(default 4 MiB). A full 800x480 view needs 3,456,000 bytes for these three buffers
before allocator alignment. Pattern coverage temporarily uses the source storage
for an opaque picture when it fits; otherwise a temporary packed image buffer
must also fit the same budget. This limit does not include decoder/cache allocations
or ThorVG's internal copied picture/path/gradient storage.

Budget rejection, checked allocations, coverage/source raster failures, target
rebinds and pattern decode/load/clip/push failures preserve the complete original
target, even after earlier successful subtasks. Failed tasks are consumed with a
warning. No second renderer fallback is introduced.

Pinned ThorVG C APIs take ownership of valid paint arguments to canvas push and
clip attachment even on failure. Cleanup now honors that contract; it cannot
delete the transferred object a second time. Fault tests simulate both immediate
consumption and errors reported after the canvas/parent retained the object.
Internal ThorVG allocations/assertions are not comprehensively fault-injected.

## Verification

- Combined host **91/91 PASS** (45.71 s), disabled baseline **81/81 PASS**
  (21.97 s), minimal SVG without FreeType **7/7 PASS** (3.25 s).
  After adding fill/stroke overlap, refreshed vector/SVG **7/7 PASS** (1.66 s)
  and refreshed minimal SVG **7/7 PASS** (1.45 s).
- **7,084 rectangular scenes**, all nine modes, seven target encodings, backdrop/
  paint/task opacity including zero task opacity, independent split scissors and
  normal/operator/normal ordering.
- **972 geometric renders** (324 source/coverage/operator triples) cover fractional
  triangles, rounded holes, variable gradients, transparent patterns, dashed
  round-cap strokes, rotated/cropped image extents, fully transparent solid/
  gradient fills and fill/stroke overlap. The floating-point oracle uses separate
  real source and opaque-coverage rasters. Premultiplied output tolerance is one
  level; native filtered source-over allows two due to its extra rounding step.
- **11,556,400 channel comparisons**, **1,083 injected failure cases**, and
  **13 invalid/empty/budget cases**, including A8-budget and oversized pattern
  workspace rejection. Staging ownership balances and patterns remain immutable.
- **217 board probes** include all nine modes and exact transparent-fill/hole
  behavior in seven formats. Actual probe source passes ten host canvas/task
  lifetimes (**2,170 scenes**) with balanced layer lists/memory. Expected marker:
  `PASS 217 vector surface probes`.
- Logs: `output/vector-coverage-{before,build,focused,tests,baseline-build,baseline-tests,minimal-build,minimal-tests,final-build,final-tests,minimal-final-build,minimal-final-tests}.log`.
- Full D13x boot/app, final-link/static, image and clean-source manifest **PASS**.
  All 31 artifact hashes, three source pins and SDK gitlink independently checked.
  Live renderer/probe symbols and allocated 217-probe/hole/failure markers verified.
  Nine image CRCs and 41 fixture/provenance hashes pass. Exact source pins and
  image/ELF hashes are recorded in [validation](validation.md).
- Physical pixels, memory pressure and performance: **NOT_RUN**. No flashing.

The later [SVG opacity stage](svg-opacity-stage.md) implements bounded group/
element isolation. General SVG document semantics, native internal allocation
failure coverage and physical renderer throughput remain open. Budgeted staging adds
raster and synchronization work; host correctness is not hardware acceptance.
