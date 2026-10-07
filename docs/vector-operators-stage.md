# Native vector destination-over, additive and erasure

The pinned software backend mapped DST_OVER and SUBTRACTIVE to ordinary
source-over, while native additive produced incorrect translucent alpha/color.
The expanded regression records the old additive failure in
`output/vector-operators-before.log`: source blue 32 at opacity 64 on a transparent
backdrop returned blue 8 instead of 32.

The application-owned generated backend now implements these three operators
using the existing bounded source/target staging. This is software vector
correctness, not GE acceleration or an already supported ArtInChip GE primitive.
No SDK or pinned LVGL source is modified.

## Defined semantics

The mapping follows native LVGL
`src/draw/vg_lite/lv_draw_vg_lite_vector.c:lv_blend_to_vg` and the formulas in
`src/libs/vg_lite_driver/inc/vg_lite.h` at the pinned LVGL revision.
S/D below are premultiplied color and Sa/Da are alpha, normalized to 0..1.

| Vector mode | Premultiplied color | Alpha |
|---|---|---|
| DST_OVER | D + S * (1 - Da) | Da + Sa * (1 - Da) |
| ADDITIVE | min(1, S + D) | min(1, Sa + Da) |
| SUBTRACTIVE | D * (1 - Sa) | Da * (1 - Sa) |

Vector SUBTRACTIVE maps to VG_LITE_BLEND_SUBTRACT, a destination-out eraser.
It is distinct from ordinary LVGL image-color subtraction and the driver enum
VG_LITE_BLEND_SUBTRACT_LVGL. Erasure uses source alpha, including raster coverage;
source hue does not affect erasure. Transparent holes and pixels outside path/
clip leave the destination unchanged. Destination-over preserves opaque backdrops.

The native `LV_OPA_MIX2(255,255)` returned 254. Local vector solid, gradient and
pattern opacity products now use rounded division by 255, preserving full opacity
so an opaque eraser leaves zero residue. The global LVGL opacity macro is untouched.
On RGB565/RGB888/XRGB8888 destinations, reduced-alpha results are stored against
implicit black; unpremultiplying them would incorrectly undo erasure. ARGB and
RGB565A8 retain their alpha and original straight/premultiplied representation.

The optional source surface shares `AIC_LVGL_VECTOR_SURFACE_BYTES` with the target
surface. It is reused across operators. Checked allocation/target-rebind/raster
failure preserves the original target even after earlier successful subtasks.
Shape fill and stroke compose into a single source before its operator. This
does not implement general SVG isolation/group opacity.

## Evidence and limits

- Combined host **91/91 PASS** (46.39 s), disabled baseline **81/81 PASS**
  (21.72 s), minimal SVG without FreeType **7/7 PASS** (2.95 s).
- **4,543 analytic rectangular scenes**, six tested modes, seven target encodings,
  four backdrop alphas, solid/gradient/stroke/pattern opacity, independent split
  scissors and ordered normal/operator/normal drawing.
- **192 geometric renders** (96 source/operator pairs) additionally cover
  fractional triangles, rounded even-odd holes, varying color/alpha gradients
  and patterns with transparent pixels. A normal real-ThorVG source raster feeds
  an independent floating-point composition oracle; premultiplied output tolerance
  is one channel level. Every zero-source pixel preserves the destination exactly.
- Combined **6,953,308 channel comparisons**, **462 allocation/canvas failure
  cases**, nine malformed/empty/budget cases. Full solid erasure requires exact
  zero, independent of ordinary pixel tolerances. Source-pattern data is immutable.
- **133 board probes** now cover six modes plus exact opaque erasure on all
  seven formats. Their actual source runs through native canvas/task dispatch
  over ten host lifetimes (**1,330 scenes**) with balanced layer lists/memory.
  Expected summary: `PASS 133 vector surface probes`.
- Logs: `output/vector-operators-{before,build,focused,tests,baseline-build,baseline-tests,minimal-build,minimal-tests}.log`.
- Full D13x boot/app, final-link/static, image and clean-source manifest **PASS**.
  All 31 artifact hashes, three clean source pins and SDK gitlink independently
  checked. The live renderer and 133-probe runner, plus exact-erase markers, are
  present in allocated ELF sections. Nine image CRCs and 41 fixture/provenance
  hashes pass. Exact commits/artifacts are in [validation](validation.md).
- Physical pixels, heap pressure, synchronization cost and throughput: **NOT_RUN**.

At this implementation pin, SRC_IN/DST_IN still mapped to source-over and NONE
was not certified. The later [coverage stage](vector-coverage-stage.md) supplies
these modes with independent geometry coverage, preserving holes while allowing
transparent paint to clear covered pixels. General SVG group/document semantics,
renderer-internal allocation failures and physical acceptance remain open.
