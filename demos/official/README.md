# Official SDK demos (verbatim)

The SDK's `packages/artinchip/lvgl-ui/aic_demo/<demo>` sources, copied
unmodified, running on LVGL 9.6 through this component. The rule is simple:
**never edit a file under `<demo>/`**. Differences from LVGL 9.6 or from the
SDK's own driver layer are absorbed in `compat/` and in the build, so a demo
can be refreshed from a newer SDK by copying it again.

| Demo | SDK source | Status |
|------|------------|--------|
| `meter_demo` | `aic_demo/meter_demo` @ SDK `fb5e9f61` | board PASS (800x480 via virtual 1024x600) |
| `dashboard_demo` | `aic_demo/dashboard_demo` @ SDK `fb5e9f61` | board PASS (800x480 via virtual 1024x600) |
| `slide_demo` | `aic_demo/slide_demo` @ SDK `fb5e9f61` | board PASS (render; swipe needs touch, not yet run) |
| `multi_lang_demo` | `aic_demo/multi_lang_demo` @ SDK `fb5e9f61` | board PASS (Chinese UI from `lang/*.ini`; switching needs touch) |
| `demo_hub` | `aic_demo/demo_hub` @ SDK `fb5e9f61` (1024x600 assets only) | board PASS (launcher; sub-apps need touch) |
| `image_demo` | `aic_demo/image_demo` @ SDK `fb5e9f61` | board PASS (FreeType lyrics on the component canvas, see below) |

Copied: every file except the SDK `SConscript`s (they reference SDK Kconfig
symbols; this component builds the demos itself). `diff -rq -x SConscript`
against the SDK directory must stay empty.

## How a verbatim demo builds

- **LVGL API**: LVGL 9.6 keeps the v8 and 9.0/9.1 API maps (`lv_img_*`,
  `lv_scr_act`, `lv_obj_clear_flag`, `_lv_ll_*`, ...). Names it no longer maps
  are renamed in `compat/lv_aic_v8_compat.h`, which is force-included
  (`-include`) into every demo source because some files never include
  `aic_ui.h`: `lv_style_set_arc_img_src`, `lv_mem_alloc/lv_mem_free`, and
  `lv_font_simsun_16_cjk` (LVGL 9 ships Source Han Sans SC instead; `lv_conf.h`
  enables it when `multi_lang_demo` is selected). For `demo_hub`: the v8 image
  API (`lv_img_decoder_get_info`, `lv_obj_set_style_bg_img_opa`;
  `lv_img_cache_invalidate_src` drops both LVGL's and the component MPP
  decoder's cache entry; `lv_img_cache_set_size` is a no-op, LVGL 9 sizes the
  cache in bytes), `lvgl_private.h` (it dereferences theme/draw-task structs)
  and `aic_player.h` (the SDK widget header included it for
  `struct av_media_info`).
- **Symbols**: each demo's `ui_init` and font are renamed per demo; other
  globals two demos share are listed in `OFFICIAL_RENAMES` (SConscript) and
  the matching block in `tests/host/CMakeLists.txt` (`demo_hub`'s
  `dashboard_ui_init`).
- **Includes**: every source directory is an include root (`demo_hub` uses
  `"./components/x.h"` relative to `app/common`). Host GCC 14 builds the
  demos with `-Wno-error=implicit-function-declaration` (vendor files miss
  `<string.h>`), matching what the target toolchain accepts.
- **`aic_ui.h`**: `compat/aic_ui.h` wraps the SDK file (`compat/aic_ui_sdk.h`,
  unmodified). It overrides `LVGL_STORAGE_PATH` (the board `rtconfig.h` sets
  `/rodata/lvgl_data`) with `/rodata/lvgl_data/<name>`, so demos whose asset
  file names collide (`mileage/0.png`, `bg/`, `point/`, ...) coexist in one
  image, and pulls in the v8 compat.
- **`lv_port_disp.h`**: `compat/lv_port_disp.h` provides `fbdev_draw_fps()`
  from the display port (`lv_aic_display_fps()`: frames presented in the last
  full second, the SDK semantics).
- **Per-demo build group** (`SConscript`, `tests/host/CMakeLists.txt`):
  `-Dui_init=lv_aic_official_<name>_ui_init`,
  `-Dui_font_regular=lv_aic_official_<name>_font` (every demo defines both)
  and `-DAIC_OFFICIAL_DEMO_NAME=<name>`; assets install to
  `rodata/lvgl_data/<name>`.
- **Resolution**: the demos are 1024x600 designs. On the 800x480 D50T panel
  the build enables `AIC_LVGL_VIRTUAL_RES` (see
  [docs/virtual-resolution.md](../../docs/virtual-resolution.md)): LVGL renders
  1024x600 and the display scales each frame once.

## Running

`lv_aic_official_demo.c` is the portable runner. `show` gives the demo a fresh
screen, hides the host UI's top-layer overlays and records the LVGL timers the
demo creates. `close` first loads the previous screen (so the demo's own
unload handlers run with its objects alive; `demo_hub` deletes its timer
there), silences animation callbacks on the demo's objects, then deletes every screen
added while the demo ran (`slide_demo`, `multi_lang_demo` and `demo_hub`
create and load their own), then whichever recorded timers still exist, and
restores the overlays. Shell (smoke app):

    lv_aic_demo list
    lv_aic_demo show meter        # or dashboard, slide, multi_lang, demo_hub, image
    lv_aic_demo status            # FPS, MPP cache, GE2D counters
    lv_aic_demo close

Limits: a demo that creates objects on `lv_layer_top()` itself, or starts
animations on non-object variables, is not cleaned up by `close`; no current
demo does. Heap a demo allocates without a free path (`multi_lang_demo`'s
language context, `meter_demo` statics) is not reclaimed; showing a demo
repeatedly leaks that small amount each time. The gate raises the MPP decoded-image cache to 4 MiB while
a demo runs (full-screen JPEG backgrounds must stay cached).

## Adding a demo

1. Copy `aic_demo/<demo>` without its `SConscript`.
2. Add `(Kconfig symbol, name, directory)` to `OFFICIAL_DEMOS` in the component
   `SConscript`, the same pair to `AIC_OFFICIAL_DEMOS` in
   `tests/host/CMakeLists.txt`, a Kconfig bool under `AIC_LVGL_OFFICIAL_DEMOS`,
   a registry line in `lv_aic_official_demo.c`, and the name to the
   `-OfficialDemos` set in `tools/sdk/build.ps1` and `known` in
   `tools/sdk/check_integration.py`. Build with
   `build.ps1 -OfficialDemos meter,dashboard,slide,multi_lang,demo_hub`
   (implies `-VirtualRes`; `demo_hub` also implies `-WithPlayer`). Sources and
   include directories are found at any depth (e.g. `multi_lang_demo/screen`);
   `assets/` installs to `rodata/lvgl_data/<name>`, or as `official_installs()`
   in the SConscript says (`demo_hub`: `assets/1024x600/lvgl_data`, 8.8 MB, all
   five demos fill 12.4 of the 14 MB rodata; `image`: a hard-coded font path).
3. Build the host `official_demo_contract`; add any missing v8 name to
   `compat/lv_aic_v8_compat.h` rather than editing the demo.

## image_demo and the SDK's prebuilt canvas

`image_demo`'s own `image_ui.h` defines `CANVAS_IMAGE_EXAMPLE`, so its default
path is `canvas_image_example.c`: FreeType lyrics drawn into two
`lv_aic_canvas` objects that animate in turn. The SDK ships that widget only as
`aic_widgets/aic_canvas/libaic_canvas_v9_<cpu>.a` (D13x links `e907fdp`). The
archive holds one object, `v9/lv_aic_canvas.c`, built by GCC 10.2 at `-O2 -g2`
against LVGL 9.1; its DWARF and about 1 KB of code give the full behavior:

- `lv_aic_canvas_class`: base `lv_image_class`, `instance_size` 124
  (`lv_image_t` 92 + `lv_draw_buf_t *` + a 24-byte static `lv_draw_buf_t` +
  `struct lv_mpp_buf *`), name `aic_canvas`; empty constructor; the
  destructor drops the image cache entry and frees the MPP buffer.
- `alloc_buffer(w, h)`: `lv_mpp_image_alloc(w, h, ARGB8888)` (CMA), wraps it in
  the static draw buffer and sets it as the image source. A second call leaks
  the first buffer.
- `init_layer`/`finish_layer`: 9.1 copies of `lv_canvas_init_layer` and
  `lv_canvas_finish_layer` (zero a 60-byte `lv_layer_t`, set buffer, format and
  areas; dispatch until the layer has no tasks).
- `draw_text(x, y, max_w, dsc, txt)`: `lv_draw_label` into
  `{x, y, x+max_w-1, h-1}`, no cache flush, no invalidate.
- `draw_text_to_center(dsc, txt)`: GE-fill the buffer transparent, measure the
  text unwrapped at zero spacing, center it, draw, flush the cache, invalidate.

It cannot link into 9.6: the machine code hard-codes 9.1 layouts (`lv_image_t`,
`lv_layer_t`, `text`/`font` at offsets 28/32 of `lv_draw_label_dsc_t`) and
calls `_lv_log_add`, renamed in 9.2. The component's
`widgets/lv_aic_canvas.c` (`AIC_LVGL_USE_CANVAS`) is a source implementation
of the same API on 9.6 (base `lv_canvas_class`; CPU clear instead of GE fill;
also flushes after `draw_text`; frees the old buffer on re-allocation), so the
demo builds against it unmodified. `-OfficialDemos image` implies
`-WithFonts` and `AIC_LVGL_USE_CANVAS`. Its images install under
`lvgl_data/image`, and `DroidSansFallback.ttf` (3.2 MB) at the path the demo
hard-codes, `lvgl_data/font/`. With `demo_hub` it exceeds the 14 MB rodata, so
`build.ps1` rejects that combination.

The demo restarts its lyric chain from an animation `deleted_cb`, which fires
when the canvases are deleted. `close` therefore clears the completed/deleted
callbacks of every animation on a demo object before deleting its screens;
without that the host contract crashes. Each `show` also creates a new
FreeType font that the demo never deletes.

## Flashing assets

Assets live in the `rodata` partition. `artinchip-flash burn` (0.1.0) writes
only `spl`, `env` and `os`: it mis-parses META partition names whose following
size bytes are not UTF-8 (`rodata`'s 0x00E00000), so `rodata`/`data` are never
selected. Use the SDK's own upgrader for a full image, as `scons --burn` does
(`tools/scripts/upgcmd.exe` needs the 32-bit MinGW runtime on `PATH`, e.g.
MSYS2 `mingw-w64-i686-gcc-libs`):

    aicupg                                   # board shell: enter USB upgrade
    upgcmd -p image <image>.img              # writes every target, incl. rodata
    upgcmd shcmd reset
