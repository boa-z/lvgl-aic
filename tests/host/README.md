# Host smoke test

This test is intentionally separate from the D50T product application. It
builds the pinned LVGL checkout supplied by the caller, compiles the
`lvgl-aic` public API and Phase 1 sources, and runs a small platform-only UI
smoke page. It does not emulate the ArtInChip framebuffer or touch hardware.

```sh
cmake -S tests/host -B build/host -G Ninja \
  -DLVGL_ROOT=/path/to/lvgl-9.6.0
cmake --build build/host
ctest --test-dir build/host --output-on-failure
```

The expected result is a successful build and `lvgl_aic_platform_smoke`.
AIC BSP/board validation remains a separate pending step.
