# Manual platform tests

This directory is reserved for platform-only LVGL 9.6 tests. It must not depend
on D50T product pages, product text, or product assets.

The smoke page includes a small moving software-rendered marker so continuous
refresh, cache coherency, and frame timing can be observed without product UI.
The smoke application also repeats `lv_aic_init()`/`lv_aic_deinit()` before
entering its long-running page; watch the serial log for lifecycle results.

Phase 1 should eventually provide tests for:

- solid fill;
- rounded rectangle software fallback;
- small and large images;
- ARGB image;
- scaling and rotation;
- text;
- touch down/move/release.

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
remain board acceptance requirements. GE2D remains disabled.
