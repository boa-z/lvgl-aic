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
Heap before/after is diagnostic only; SDK CMA accounting and visible pixel/alpha
correctness remain board acceptance requirements. GE2D remains disabled.
