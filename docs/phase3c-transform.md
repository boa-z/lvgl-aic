# Phase 3C2-3C5 transform gates

2026-09-27. Base: phase3-ge2d / 45d1f03 (3C1 board blend probe).
The earlier supplied log with mpp_ge_open failure predates this baseline;
it is not evidence about the scale candidate. Keep GE_PD_NONE / mixed alpha.

## 3C2 - board gate PASS (2026-09-27)

The existing image executor prepares one RGB scale descriptor, with integer
16.16 inverse steps (65536*256/scale), independent x/y phase, decoded stride,
LVGL 9.6 transformed/clip rectangle and layer-relative destination coordinates.
Intermediate mapping uses int64. RGB565/RGB888/ARGB8888/XRGB8888 share the
existing alpha block. LAYER policy and the global allocator remain unchanged.
Bitblt/emit/sync failures return INVALID, rather than repeating a possibly
partly blended operation in software. Unsupported geometry falls back first.

A source crop or destination smaller than 4x4, ratios outside [16,4096],
out-of-buffer sampling, rotation, skew, tile, recolor and masks are not GE work.
For fractional boundary rounding that cannot be represented safely, the entire
task falls back; no partial image is silently dropped. D13x source filter crop
includes the extra tap where available, clamped to the actual decoded extent.

D13x split optimization deferred. Vendor calculate_split_params has the outer
condition 59392 < dx_16 < 65536 with destination width >=32. The requested
0.5/1.5/2 and 1.5x0.75 cases do not hit it. Until a split probe is accepted,
that entire interval falls back safely (including 264/256).

Host contract compiles the real executor/evaluator and actual SDK descriptors,
mocking GE/cache calls. Checks four RGB formats, phases, nonzero layer origin,
pivot/clip, invalid scales, minimum sizes, overflow and each submission failure.
It does not emulate hardware filtering.

The board probe calls the production image executor with temporary CMA buffers
and a linear asymmetric colour ramp. It requires ENGINE outcome; compares
interior pixels to an independent rational mapping with tolerance 3/255, and
checks the entire outside-clip guard unchanged. The 3-pixel source edge band is
excluded from numerical comparison (filter differences); inspect it on panel.
ARGB uses per-pixel 128 multiplied by global 128. No byte-equivalence to the
different LVGL SW filter is claimed. The existing 3C1 probe stays unchanged.

The upper-right test-page strip shows R .5 / R 1.5 / R 2 / A .5 / A 1.5 /
A 2 / XY / clip. Existing rows/button stay in place. XY is 1.5x0.75; clip adds
pivot (7,9). A tiles use global opacity 128 on white. The scheduler's
scaled_image_engine delta must be positive and errors zero.

Run from SDK root:

    & packages/custom/lvgl-aic/tools/sdk/build.ps1 -Phase ge2d -Jobs 8 -AllowComponentDirty

Archive: SDK output/lvgl-evidence/ge2d. Attach image SHA256, complete serial log
and panel/touch observations to the gate. Candidate changes remain uncommitted
until the board gate passes; then commit 3C2 independently.

| Gate | Requirement | Current evidence |
|---|---|---|
| 3C2 | RGB/ARGB 0.5,1.5,2 engine and pixels | PASS on D50T-2-Lite corrected image |
| 3C2 | nonuniform + clipped pivot | PASS on board |
| 3C2 | invalid ratio/min size/split fallback | PASS on board |
| 3C2 | scaled scheduler count >0, errors=0 | PASS: engine=10, errors=0 |
| 3C2 | panel edges, alpha, display/touch | framebuffer PASS; physical photo/touch operator confirmation pending |
| 3C3 | 90/180/270, pivot/clip/alpha | **PASS** - operator visually confirmed D50T-2-Lite page and rotation output |
| 3C4 | 0.5+90, 1.5+90, 2+180, ARGB | **NEXT** - implementation and numeric probes |
| 3C5 | light counters + GE ON/OFF timing | NOT_STARTED until 3C4 passes |

### 2026-09-27 board feedback and correction

User-provided log reports image engine=19, image software=0, errors=0 and
3C1 blend deviations=0, followed by the first RGB scale 128/256 probe returning
SOFTWARE (outcome=1). The page remains visible; the user sees scaled images.
The log does not identify an image SHA256. It is not a completed 3C2 gate:
later scale probes and the scaled scheduler assertion were not reached.

Reproduced on host through the real LVGL bin decoder: a 32-pixel RGB888 source
has deliberately padded stride 128, whereas default stride normalization wants
96. The decoder copies this immutable CMA source to ordinary LVGL heap; the
GE address check then rejects it. The file-decoded MPP page images do not
exercise that same variable-source normalization path.

The GE helper now explicitly requests stride_align=false and premultiply=false,
preserving decoded layout/straight alpha. Hardware consumes decoded byte stride.
The regression permits only original source/destination pointers as GE memory,
and exercises the full executor for four formats at all three scales. It failed
before the correction and passes afterwards; previous callback-only coverage
could not detect this. No address-window relaxation or global allocator change.

10/10 host tests and the corrected D50T image checks pass. Board numeric pixels,
edge/alpha/touch inspection and the new UART snapshot remain NOT_RUN.
See [framebuffer capture](framebuffer-capture.md) for the new manual-test command;
it copies a presented UI framebuffer, without changing LAYER rendering policy.
See [UART upgrade](d50t-uart-upgrade.md) for the already-enabled Bootloader route.

### 3C3 board gate (2026-09-27)

The D50T-2-Lite operator confirmed the on-screen Next/Prev controls and
visually inspected the 0°, 90°, 180° and 270° fixtures; rotation output was
reported correct. This closes the visual portion of 3C3. Numeric pivot/clip
comparison is covered by the host contract and remains separate from the
operator observation. The next implementation must combine the GE rotation
flags with the existing scaler while preserving source crop, pivot and alpha
semantics; it must first add host submission contracts before another board
image is produced.

Each stage needs its own evidence and commit. No arbitrary rotation, CMA layer
copies, async thread, YUV, tile, recolor or allocator replacement. Stop after
3C5; subsequent work is real D50T UI integration and workload profiling.
