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

## Interactive SDL2 smoke window

The optional `lvgl-aic-sdl-smoke` target reuses the same
`tests/manual/lv_aic_manual_test.c` page with the pinned LVGL 9.6 checkout. It
opens an 800x480 SDL2 window, animates the green marker, and maps the SDL
mouse to an LVGL pointer device. It is a host UI check only; it does not
emulate the ArtInChip framebuffer, PAN/VSync, cache, or GT911 hardware.

```sh
cmake -S tests/host -B build/host-sdl -G Ninja \
  -DLVGL_ROOT=/path/to/lvgl-9.6.0 \
  -DAIC_BUILD_SDL_SMOKE=ON
cmake --build build/host-sdl
./build/host-sdl/lvgl-aic-sdl-smoke
```

On Windows, run the configure/build commands from an MSYS2 UCRT64 shell (or
use its CMake/Ninja toolchain), with the SDL2 package installed:

```sh
pacman -S --needed mingw-w64-ucrt-x86_64-SDL2
```

The executable also accepts `--frames N` for a bounded run,
`--screenshot PATH` to save a BMP of the first rendered frame, and
`--self-test` to inject a mouse click and verify the status label changes to
`button event received`. When SDL2 is built, CTest also runs the bounded
self-test with SDL's dummy video driver.
