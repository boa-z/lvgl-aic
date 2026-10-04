# Native SVG image resources

`AIC_LVGL_USE_SVG=y` opts into the pinned LVGL 9.6 SVG parser/image decoder and
selects `AIC_LVGL_USE_VECTOR`. `tools/sdk/build.ps1 -WithSvg` selects both and adds
`-vector-svg` to the profile suffix. The default remains off.
This uses LVGL's native C parser and custom-draw path over software vectors;
ThorVG's separate SVG loader remains excluded; its Lottie loader is controlled
by the independent [Lottie option](lottie-stage.md). The SDK v9 baseline has
vector/ThorVG options but no equivalent native `LV_USE_SVG` Kconfig entry, so
this is a native 9.6 resource capability beyond the baseline configuration.

Use `lv_image_set_src` with a `.svg` path on an application-registered LVGL drive,
or an immutable `lv_image_dsc_t` with width/height, header magic, RAW format,
encoded bytes and exact `data_size`. Keep variable descriptors/data alive while
used by the widget/cache, just as for other native LVGL image sources. File
headers supply their intrinsic dimensions; VARIABLE headers supply theirs.
The source must start with `<svg` or `<?xml` as required by the native decoder.

The native decoder publishes `LV_IMAGE_FLAGS_CUSTOM_DRAW`; LVGL expands it to
vector tasks before GE image scheduling, so the parser's internal representation
is not submitted as GE pixel storage. Image transforms and clipped/offset child
layers are now corrected and covered by the [transform stage](svg-transforms-stage.md).
Image-specific opacity/recolor/tiling, richer SVG documents, embedded bitmap/font
callbacks and animation require further work; this
stage does not claim complete browser SVG compatibility, SVG animation, a new
GE rasterizer or a bounded SVG allocation budget.

Host **74/74 PASS** with SVG and vector enabled alongside the GE/widget suite.
The real image widget/decoder/ThorVG contract compares FILE and VARIABLE output
byte-for-byte across 20 create/render/delete/cache-drop cycles, checks intrinsic
32x32 dimensions and custom-draw flags, verifies red rectangle/blue circle pixels
and untouched black background, and balances file open/close counts. Full-cover
blending retains native one-level channel rounding. Non-SVG VARIABLE data is
verified to bypass the SVG parser; the generic BIN decoder can still accept that
descriptor, so metadata/open success is not proof of valid SVG content.

The vector/SVG configuration now defaults to LVGL builtin printf formatting,
so enabled floating-point values are printable with `%f`; the RT-Thread formatter
cannot provide this. A native float-format regression and target C++ probe cover
that choice.

Logs: `output/svg-{config,build,tests}.log`. Combined D13x target compilation,
final-link/live-symbol checks, image and clean-source manifest validation **PASS**;
see [exact source pins and image hash](validation.md). The firmware combines
GE/fonts/GIF/widgets/AICP/player/APNG/barcode/SPI/demos/music/vector/SVG at
90-degree rotation. Hardware rendering, memory pressure and performance are
**NOT_RUN**; no flashing or board configuration changes.
