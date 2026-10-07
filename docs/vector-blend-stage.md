# Native vector translucent multiply and screen

This stage corrects the application-owned native LVGL/ThorVG software renderer.
The pinned ThorVG special solid blenders assume opaque input channels and lose
the backdrop contribution when supplied premultiplied translucent colors.
An opaque backdrop with blue 48 and source blue 32 at opacity 128 returned 3
instead of approximately 27. The prior failure is retained in
`output/vector-translucent-blend-gap.log`; the same explicit
`lvgl_aic_vector_surface_contract.exe translucent-blend` now passes.

This is software renderer correctness, not new GE vector acceleration or a
claim that the SDK baseline provides these corrected semantics. The LVGL pin
and SDK sources remain unchanged; host and target use the same SHA-guarded
`tools/sdk/stage_vector.py` replacement.

## Composition and storage

MULTIPLY and SCREEN render each native vector subtask to a cleared premultiplied
source surface, then apply separable source-over equations against the accumulated
target surface. Both source and backdrop alpha participate. Normal subtasks
continue on the target surface in order. Shape fill/stroke within one subtask
compose into that subtask's source before the special blend. This does not
implement SVG document/group isolation or a general group-opacity engine.

Gradient stop alpha now multiplies fill/stroke opacity; image-pattern opacity
includes pixel alpha, image descriptor opacity, fill opacity and task opacity.
Previously gradient fill/stroke and pattern fill ignored their own opacity.
Constant-color linear/radial gradients, gradient strokes and translated ARGB
patterns are covered here; arbitrary gradient interpolation/transform/filtering
equivalence remains outside this stage.

The source surface is allocated lazily, reused across special subtasks, and
released after the canvas. Both clipped surfaces share
`AIC_LVGL_VECTOR_SURFACE_BYTES` (default 4 MiB). Allocation, checked canvas and
target rebind failure, or combined-budget rejection leaves the original target
unchanged, including when previous subtasks have modified staging. The task is
consumed with a warning and no alternate rasterizer. Internal ThorVG
shape/gradient/picture allocation failures are not comprehensively covered.

Seven target encodings retain their representation: RGB565, RGB888, XRGB8888,
straight ARGB8888, explicit/flagged premultiplied ARGB8888 and RGB565A8.
Clips, row padding, unused X bytes and untouched hidden RGB remain preserved.
Eight-bit premultiplication can amplify errors when unpremultiplying small alpha.

## Verification

- Combined host **91/91 PASS** (44.18 s); disabled baseline **81/81 PASS**
  (21.34 s); minimal SVG without FreeType **7/7 PASS** (2.73 s).
- Real ThorVG: **2,254 scenes and 3,254,776 analytic channel comparisons**.
  Includes normal/multiply/screen, four backdrop alphas, source/task opacity,
  independent split scissors, five additional paint styles and ordered
  normal -> special -> normal subtasks. The oracle uses floating-point
  source-over/separable equations, not the integer implementation.
- Tolerance four channel levels, nine for RGB565 quantization. Low-alpha paint
  tests additionally allow one premultiplied unit divided by output alpha in
  straight-color channels. This is not bit-exact arbitrary-edge certification.
- **210 allocation/canvas fault cases**, including second allocation and both
  source/target rebinds, preserve the complete original buffer. Nine malformed,
  empty or over-budget cases include a target that fits one surface but exceeds
  the combined budget after a successful normal subtask.
- Immutable image-pattern data remains unchanged after cache teardown.
  Wrapped staging buffer ownership balances across the complete matrix.
- **63 board probes** are compiled and run on host through actual native
  canvas/task dispatch over ten lifetimes (**630 scenes**), with balanced
  layer lists/memory. Solid, linear and radial paints each cover three blend
  modes and seven target encodings. Expected board marker:
  `PASS 63 vector surface probes`.
- Logs: `output/vector-blend-{build,focused,tests,baseline-build,baseline-tests,minimal-build,minimal-tests}.log`.
- Physical pixels, memory pressure and throughput: **NOT_RUN**. No flashing.

Full D13x boot/app, final-link/static, image and clean-source manifest gates
**PASS**. Thirty-one hashes, three source pins, SDK gitlink and live allocated
renderer/probe markers were independently verified. Exact commits and image/
ELF hashes are recorded in [validation](validation.md).
The later [operator stage](vector-operators-stage.md) adds destination-over,
additive and vector subtractive erasure and preserves opaque paint endpoints.
The later [coverage stage](vector-coverage-stage.md) supplies SRC_IN/DST_IN and
bounded replacement. SVG document semantics and target performance remain open.
