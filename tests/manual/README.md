# Manual platform tests

This directory is reserved for platform-only LVGL 9.6 tests. It must not depend
on D50T product pages, product text, or product assets.

The smoke page includes a small moving software-rendered marker so continuous
refresh, cache coherency, and frame timing can be observed without product UI.
The smoke application also repeats `lv_aic_init()`/`lv_aic_deinit()` before
entering its long-running page; watch the serial log for lifecycle results.

Coverage that exists today: solid fill (software, and GE2D when
`AIC_LVGL_USE_GE2D=1`), rounded-rectangle software fallback, the moving marker,
JPEG/RGB-PNG/RGBA-PNG decoding, and touch down/move/release.

Still missing and expected from later phases:

- small and large images through the GE2D IMAGE path;
- scaling and rotation;
- text;
- layer composition.

With `AIC_LVGL_USE_MPP_DEC=1`, the SDK smoke owner calls
`lv_aic_mpp_test_run()` before the final page: accepted JPEG/PNG fixtures,
unsupported/corrupt/missing inputs, then 1000 uncached decode/close cycles.
Failure skips stress but still leaves the page available. Watch `lvgl.mpp.test`
logs; a successful open must belong to the MPP decoder, not a software fallback.

The 1000-cycle loop is measured with the decoder's own CMA lifecycle counters
(`lv_aic_mpp_cma_stats()`). The loop resets them first, then requires
`current_cma_bytes == 0` and `alloc_count == free_count` afterwards, plus
`alloc_count >= 1000` so a bypassed allocator cannot make the check vacuous.
These counters cover only the wrapper's own `MEM_CMA` buffers; CMA held inside
the SDK MPP engine is not visible here. Heap before/after stays diagnostic.

The page renders `a.jpg` (JPEG), `b.png` (RGB PNG) and `c.png` (RGBA PNG). Both
32x32 PNG fixtures are scaled 4x from pivot (0,0), and `c.png` sits on a white
swatch: its 0..255 alpha ramp then reads as white at the transparent corner and
as its own colour at the opaque corner, making alpha blending unambiguous on the
panel. Visible pixel/alpha correctness and the Gate 1 display/touch baseline
remain board acceptance requirements.

With `AIC_LVGL_USE_GE2D=1`, the page gains a Phase 3A section: one large, one
medium and six small opaque rectangles with square corners, plus one rounded
rectangle. The first three groups satisfy every GE2D precondition and are drawn
by the engine; the rounded rectangle is deliberately unclaimable and must fall
back to the software renderer. The smoke owner then calls
`lv_aic_ge2d_test_run()` once, which invalidates the screen, forces a full
refresh and reads the draw unit's own counters as deltas:

- `fill accepted` — opaque FILL tasks the unit claimed;
- `fill completed` — the subset that reached FINISHED;
- `fallback` — FILL tasks the unit declined (not an error counter);
- `errors` — GE2D execution failures.

The check requires `errors == 0`, at least 8 accepted, `completed == accepted`
and at least one fallback. A unit that never opens the GE device fails the
check with `GE2D device unavailable` rather than passing vacuously. Counters
cover only the wrapper's own calls; GE work done inside the SDK MPP engine is
not visible here. Visible pixel correctness on the panel is still a board
acceptance requirement, and a host or link-map PASS proves nothing about it.
