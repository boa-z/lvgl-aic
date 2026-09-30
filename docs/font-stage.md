# Native font stage

## Scope and ownership

This stage adds native LVGL 9.6 FreeType bitmap fonts: dynamic sizes, normal /
bold / italic styles, Chinese glyphs, explicit fallback and the native glyph
cache. It selects the existing standalone SDK FreeType library. Upstream LVGL,
SDK sources and the product application remain unchanged.

Enable AIC_LVGL_USE_FREETYPE in the consuming application; it selects
LPKG_USING_FREETYPE. AIC_LVGL_FREETYPE_GLYPHS is 8..512 (default 64). The native
L1 glyph cache is disabled so non-power-of-two capacities remain valid. The
native LRU limits glyph count per cache, not total bytes across fonts, faces or
sizes. Large glyphs and many live fonts still require a RAM budget. The old
AIC_LVGL_USE_FT_CACHE placeholder is not implemented.

lv_init/lv_deinit own native FreeType initialization; the platform adapter must
not initialize it again. Application code creates fonts with
lv_freetype_font_create(path, LV_FREETYPE_FONT_RENDER_MODE_BITMAP, size, style).
The library uses stdio paths such as /data/mpp_test/Lato-Regular.ttf, not LVGL
L: paths. Keep files available while faces are live. Set font->fallback to a
longer-lived font if needed. Detach fallback and remove every label/style using
the font before lv_freetype_font_delete; delete all fonts before lv_deinit.
No automatic CJK fallback is imposed on application fonts. With fonts enabled,
the component defaults LV_DRAW_THREAD_STACK_SIZE to 32 KiB as required by LVGL.
The smoke UI thread already has 32 KiB; other applications must budget their
font-call stacks too.

## Repeatable build

From the component directory in the dedicated SDK worktree:

    powershell -NoProfile -ExecutionPolicy Bypass -File tools/sdk/build.ps1 -Phase ge2d -WithFonts -AllowComponentDirty -Jobs 8

-WithFonts also supports the MPP profile, but not Gate 1. SCons reloads the
selected defconfig on each invocation: the script overlays only the isolated
smoke defconfig, saves original/effective copies in evidence, then restores its
exact original bytes in a finally block. The generated .config remains the
font-enabled build configuration; a later normal build reloads the baseline.
Do not run simultaneous SDK profile builds in this checkout.

Assets are taken from the pinned upstream LVGL checkout: Lato-Regular.ttf
(120196 bytes) and NotoSansSC-Regular.ttf (10560380 bytes), with both OFL license
files. The full CJK fixture is deliberately not a product font subset. It adds
about 10.7 MB of raw assets; production applications should choose licensed
fonts/subsets for their own storage and memory budgets. The resource inventory
contains 30 files with fonts, 26 without. Staging removes only obsolete entries
from its own previous SHA256 inventory. Image verification checks the packed
FAT payload against that inventory.

output/lvgl-evidence/ge2d-fonts contains image, ELF/map, effective config,
build/link/image checks and a manifest. The static gate requires live native
FreeType and board-probe symbols and rejects legacy lvgl-ui. Development builds
record diffs; a clean committed component may still need -AllowComponentDirty
when the parent SDK intentionally retains its published dependency pin.
No tool flashes a board.

## Host and board gates

The optional AIC_BUILD_FREETYPE_TESTS host target links real FreeType and pinned
LVGL. It rejects missing and corrupt font sources, checks fallback resolution,
nonempty bitmap glyphs, three sizes/styles and repeated cache churn (>64 unique
glyphs), then checks stable glyph hashes. Three full initialize/render/delete /
deinitialize cycles verify the four label rows produce white glyph pixels and
that Fonts/Close callbacks and failure cleanup preserve top-layer ownership.
This is functional lifecycle coverage, not a heap leak profiler or board proof.
The verified host provider is FreeType 2.14.3; the SDK provider is 2.10.4.
Host glyph hashes are diagnostic, not cross-version golden pixel hashes.

On the new board candidate, collect the full startup log. Expect three
PASS font size= lines (18, 28, 42), followed by:

    PASS native fonts: metrics, bitmap, fallback and cache churn

Open Fonts on the top bar. Check 18/28/42 px Latin/numerals and the Chinese row
“中文仪表 字体测试 0123456789”: no boxes, clipping or corrupted glyphs. Close and
reopen at least 20 times, switch all three GE pages, operate the existing touch
button and leave the UI running for at least five minutes. Record any latency,
crashes or visual corruption. This does not claim an automated leak measurement.
Also retain the resource parity/cache-hit, CMA1000, fill/blend/scale PASS logs.

## Logging correction

Finite manual probes flush ulog after each record to avoid flooding its async
queue. This is outside measured refresh intervals, IRQs and production render
paths. Long blend/resource records are split. Refresh duration is now printed as
s=<seconds> us_part=<six-digit remainder>, with ge2d_ready on its own line,
avoiding the board formatter's unsupported %llu. No global SDK ulog buffer
setting is changed. Confirm on hardware that records are complete and the
previous async-buffer warning no longer occurs during these probes.

New font-stage board acceptance remains NOT_RUN until the new image is tested.
The earlier resource-stage visual PASS applies only to that earlier image.

## Completed development handoff

Implementation commit: 3b7e090455013533df726de520917dcceea6a370, clean for all
three final firmware builds. SDK: 08b9f5f09bab4dea199f36b2bfefec3f773e2b6a;
LVGL: 80ca777e37a2b176770726a02e07a6fb79ef0b39, unmodified v9.6.0.
Remote main was fetched and remains 2294fafdbe36916d60ac311f49a1e32283cefbb8,
already an ancestor. The SDK retains its published component gitlink; do not
promote the unpublished candidate pin into the product release.

Final gates: fonts ON 9/9 host tests; fonts OFF 8/8 host tests; Gate 1,
GE2D baseline and GE2D+fonts build/link/image checks all PASS. The fonts-off
GE2D image contains 26 inventoried assets; the font image contains 30. The
original smoke defconfig was restored byte-for-byte, including after the first
failed build exposed the 32 KiB FreeType worker-stack requirement. The final
SDK tracked diff is only the component gitlink. No SDK core or LVGL edits.

Immutable evidence directory (SDK-relative):
output/lvgl-evidence/font-stage-3b7e090/ . The stage-index.json hashes 72 files,
including three firmware profiles and both host-test logs. Prior resource-stage
archives were not rewritten. The final board candidate is:

    ge2d-fonts/images/d13x_D50T-2-Lite_page_2k_block_128k_v1.0.0.img
    bytes: 12685824
    SHA256: af9096f3e60630e0240e8eec986a116d34938200ec34d85b46c27cf28f52f196

The font image has nine verified payload CRCs and all 30 asset hashes match the
packed FAT filesystem. Host FreeType is 2.14.3; target FreeType is SDK 2.10.4.
Do not compare their rasterization hashes as cross-version golden values.
New board acceptance: NOT_RUN. No flashing or push was performed. Start with
the Fonts panel and full serial checklist above before accepting this stage.
## UI layout follow-up

The manual test UI uses one fixed 64 px navigation header with page title, position and non-overlapping previous/next buttons. Content pages begin below that header. The Fonts launcher is aligned to the right on the top layer; its modal adds a full-screen input shield, so closing the panel cannot accidentally activate a page control. Host preview frames cover all three pages and the font modal lifecycle; target acceptance still requires touch confirmation.
