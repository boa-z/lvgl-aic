# Manual platform tests

This directory is reserved for platform-only LVGL 9.6 tests. It must not depend
on D50T product pages, product text, or product assets.

The smoke page includes a small moving software-rendered marker so continuous
refresh, cache coherency, and frame timing can be observed without product UI.
The smoke application also repeats `lv_aic_init()`/`lv_aic_deinit()` before
entering its long-running page; watch the serial log for lifecycle results.

Current pages include solid/rounded/translucent fills, text, JPEG/PNG,
scaled and right-angle images, combined transforms, layer composition and
pointer interaction. These are probes; not every path is hardware accelerated.
Ordinary D13x heap layers use software. See the maintained
[capability inventory](../../docs/capabilities.md) for limitations.

## Pages and controls

The GE2D/widgets/list-menu/canvas/native-widgets image shows seven pages. The
shared header carries `< Prev` and `Next >` (both wrap around), the page title
and `n / 7` position, and - when the build enables fonts - a `Fonts` launcher
on the right; the overlay's `Close` button dismisses it and modality shields
the pages behind it while open.

1. `Overview` - `Test touch` sets the status label to `button event received`;
   the green marker moves continuously as a refresh observable. Fills, text,
   JPEG/PNG images and GE2D shapes on this page are probes, not controls.
2. `Image rotation` - right-angle image rotation probes (no controls).
3. `Rotation + scale` - combined transform probes (no controls).
4. `SDK widgets` - drag the image roller (four looping, snapping cards); swipe
   the four-position swipe_v1 area; `Next icon` advances the swipe with animation.
5. `List / menu compatibility` - scroll the 12-item list; the menu entry
   `Open details` loads the Details page and its back arrow returns.
6. `AIC canvas` - `Replace text` alternates the centered text to prove no stale
   glyphs remain; `Rebuild buffer` reallocates the canvas buffer and increments
   the generation shown in the status label.
7. `Native dynamic widgets` - `Pause / resume` stops or starts the animation and
   spinner; tapping the color imagebutton advances `Value` (scale needle and
   span text follow); `Open window` opens the scrollable SDK window and `X`
   closes it while the page value survives.

`lv_aic_capture`, `lv_aic_plane_test` and `lv_aic_apng_test` are shell gates
(`lv_aic_gif` exists only when the optional GIF build is enabled), not buttons;
they queue work for the UI thread. Touch interaction and visible pixels remain
board acceptance items.

With `AIC_LVGL_USE_MPP_DEC=1`, the application smoke owner calls
`lv_aic_mpp_test_run()` before the final page: accepted JPEG/PNG fixtures,
unsupported/corrupt/missing inputs, the resource-stage checks, then 1000
uncached decode/close cycles. Resource checks compare FILE and memory pixel
hashes for the padded JPEG and both PNG formats, verify multiple-reader
invalidation, and repeat 100 cached opens per PNG without new CMA allocation.
They temporarily use a 4 MiB cache limit, drop all entries and restore the
configured limit (512 KiB default). See the [stage checklist](../../docs/resource-stage.md).
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

- `fill accepted` — solid FILL tasks the unit claimed;
- `fill completed` — the tasks completed successfully;
- `fallback` — tasks the unit declined (not an error counter);
- `errors` — GE2D execution failures.

The check requires `errors == 0`, at least 8 accepted, `completed == accepted`
and at least one fallback. A unit that never opens the GE device fails the
check with `GE2D device unavailable` rather than passing vacuously. Counters
cover only the wrapper's own calls; GE work done inside the SDK MPP engine is
not visible here. Visible pixel correctness on the panel is still a board
acceptance requirement, and a host or link-map PASS proves nothing about it.

## Translucent fill candidate

lv_aic_ge2d_test_run() now starts with lv_aic_ge2d_fill_test_run(): 12 offscreen
CMA probes for RGB565/RGB888/XRGB8888 at opacity 64/128/192/255. They call the
production executor and compare pixels against independent source-over math,
with RGB565 quantization tolerance (R/B 9, G 5; RGB888/XRGB8888 2 per channel).
All 90 touched pixels per case are checked; outside pixels and stride padding
must remain unchanged. The layer origin is nonzero and task/clip extend outside
its bounds. Partial ARGB8888 remains a software task.

Expected serial evidence: twelve PASS fill lines followed by
PASS 12 solid-fill numeric probes. If a GE operation fails, the test retains
its 1536-byte CMA allocation and blocks further fill probes until reboot:
a failed sync does not establish DMA completion, so freeing would be unsafe.
Successful probes free their allocations normally. This test is NOT_RUN on
board for the new candidate; a firmware build or host PASS does not close it.

Existing IMAGE probes check blend/scale arithmetic. Refresh logs distinguish
accepted tasks, engine-completed work, software composition and errors.
Panel rendering/touch and paired GE ON/OFF timing remain separate checks.

## Native fill admission and destination alpha

The native fill runner (`lv_aic_native_fill_test_run`) submits 72 probes:
36 RGB gradient (ARGB8888/RGB888/RGB565, both directions, forward/reverse,
blend off/on, constant and ramped alpha), 20 YUV400 solid raw-byte probes and
16 YUV400 raw-byte gradients. On GE v1.1 builds (`AIC_GE_DRV_V11`) the SDK
fillrect gate
admits RGB and YUV400 destinations only, so the other 11 linear YUV layouts
are asserted locally: 236 probes must return `LV_RESULT_INVALID` with no GE
submission, no quarantine and untouched guard bytes. A GE regression that
submits one of those layouts surfaces as a faulted probe instead.

Blended ARGB8888 probes hard-check the destination alpha against the
board-measured blend-unit result `A_out = (A*A + 255*(255-A) + 127)/255`
(tolerance 1); blend-disabled probes still require 255. Production ARGB8888
partial opacity keeps using the staged path; the probe only pins the raw
engine semantics.

YUV400 fill is a board-measured raw path. The diagnostic boot
(`board-2026-10-06-yuv400-csc`) printed `got`/`want`/active-pixel `range` for
all 20 solids: 16 failed and every one wrote the raw red byte of the
ARGB8888 fill color in all four SDK color spaces; black and white passed in
the two full-range spaces only because the red byte equals their CSC2 luma
there. GE v1.1 ignores the configured CSC2 output stage on this fillrect
path, so the probe models solids as `(color>>16)&255` and gradients as the
interpolated red byte; both are host-verified and Board run 6 confirmed them
on hardware (`PASS YUV400 20 raw red-byte solid probes guards=OK`, 16
`PASS YUV gradient ... error=0` lines). Mismatches still print `got`, `want`
and the active-pixel `range`.

## YUV image GE admission and decoded fallback

The YUV image probes run `lv_draw_aic_ge2d_yuv`, which on GE v1.1
(`AIC_GE_DRV_V11`) mirrors the SDK `ge_bitblt()` gate (RGB/YUV400 source,
RGB destination only; the SDK otherwise prints `bitblt not support yuv
format, except src format yuv400`). The eleven non-YUV400 ARGB-target probes
must decline locally and print `SKIP YUV ARGB fmt=... 4 probes; GE v1.1
bitblt admits YUV400 src only`; a regression that submits one of them
surfaces as the SDK printf plus `FAIL YUV ARGB DMA` and GE quarantine. The
I420 and packed rotation/scale/tile probes accept either executor outcome -
`GE v1.1 declined; <probe> decodes and blits RGB on the engine` (CPU decode
plus an admitted RGB blit) or `... draws RGB in software` - each reported
once, with the pixel oracles unchanged. Non-v1.1 builds keep the strict
engine expectation.

When the tiled rotation probe fails, its FAIL line reports the worst channel
(`over3` counts channel comparisons beyond the 3-step tolerance, `xy`, `got`
and `want` locate the pixel, `aa` shows the filtering mode), a `DIAG I420
tile sw open=... cf=... stride=...` line shows whether the software re-decode
still opens, and one `aa=0` re-run separates transform edge coverage from a
fallback that never painted.

The ARGB-target probes also pin the staging alpha lane. On the board the GE
v1.1 bitblt writes the source Y byte into the private surface's alpha lane
instead of 0xff (uniform Y=100 left alpha 100; at the probe's opa 64 that is
a 39-level error against the native fill oracle, Board run 9). The executor
re-marks every engine-written crop opaque before any CPU pass, so the mask
and color-key tails and the native blend see an opaque source. The SKIP lines
print the probe list's format values (32/33/34 are I420/I422/I444) while
PASS/FAIL lines print the probe index (index 3 is I400), and a mismatch
reports `xy`, `got` and `want`.

The rotated 2x strips must keep their inverse-mapped source crop at the
engine's 8x8 YUV minimum (`check_blit()`: "the min size of yuv is 8x8"), so
the 90/270 clips are 16 wide: Board run 10 showed an 8-wide strip inverse-
mapping to a 6-row window, which the executor correctly declined before any
cache handoff or write. Every probe failure path now prints its own line -
allocation, decoder and buffer setup, `rc=` from the executor or a decline,
clip-outside `xy`/`got`/`want`, and lease refcounts - so the summary line is
never the only evidence.

## Native FreeType and reliable probe logs

The -WithFonts build variant adds a Fonts button without replacing the three
GE pages. It runs native bitmap/size/style/fallback/cache probes and exposes a
Latin/Chinese panel; see [font acceptance](../../docs/font-stage.md). Remove
font users before deleting fonts. Firmware/host PASS leaves the new panel
NOT_RUN on board. Finite manual logs now drain the async queue after each
record and split long lines; refresh timing uses seconds plus microseconds
instead of unsupported 64-bit formatting. The next board log must confirm
complete records and no probe-generated async overflow.

## APNG acceptance overlay

Build with `tools/sdk/build.ps1 -Phase ge2d -WithApng` (other profile switches
can be combined). This stages three original generated fixtures under
`/data/mpp_test/`; no external SDK artwork is needed. No auto-play occurs.
On the board, queue `lv_aic_apng_test show` through FinSH. All LVGL operations
are executed by the existing UI timer, never the shell thread.

The overlay shows the master and one shared-frame slave side by side, with Pause, Resume, 0.5x, 1x, 2x, Replay, Next source and Close. Both views should follow the same frame without a second decoder.
Equivalent commands: `lv_aic_apng_test pause|resume|slow|normal|fast|replay|next|status|close`.
It cycles finite disposal animation (two plays), infinite loop and static PNG.
Each animated frame holds 500 ms at 1x:

1. Opaque red canvas.
2. Translucent green rectangle over red; resulting overlap stays opaque.
3. Green is restored to red (PREVIOUS), with a blue rectangle at lower right.
4. Blue is cleared to transparent (BACKGROUND, showing gray viewport), with a
   yellow rectangle near lower left. Finite playback holds this final frame.

Verify pause freezes frame and rate changes pacing, replay clears prior canvas,
Next source replaces safely, and Close returns to the existing smoke UI.
Repeated `show/close` must eventually report cleanup=0 before the next show;
worker/GE leases may delay this. Status counters are mailbox work, not panel
scanout proof. State 7 is a fault; retain raw logs and image SHA256. This overlay
does not automatically certify pixel accuracy, memory balance, timing or GE.

Host reference command (system Python with Pillow):
`python tests/host/apng_fixture_probe.py --exe output/lvgl-host-ge/lvgl_aic_apng_contract.exe --output output/apng-generated-probe`.
It checks the C extractor/compositor against independently decoded rectangles
and explicit alpha composition. Pillow's full animated-PNG seek path is not
the alpha oracle: with Pillow 12.3.0 it produced alpha 191 for this half-alpha
patch over opaque background, where straight-alpha OVER must remain 255.


### Explicit video-plane acceptance (RT-Thread combined player/APNG profile)

Use `lv_aic_plane_test show` after the manual UI starts, with other media tests
closed. Requires ARGB8888, at least 240x240 logical resolution and the packaged
`/data/mpp_test/apng-loop.png` fixture. The unified player uses APNG with explicit
decode budgets and a separate 4 MiB plane rotation peak budget. No test opens
a plane automatically at boot.

Commands: `pause`, `resume`, `rotate` (next 90 degrees), `pivot` (25%/25%),
`hide` (toggle), `status`, `close`. Prefix each with `lv_aic_plane_test`.
Shell commands only queue work; LVGL operations run on the manual UI timer.
Check the animated image, rotation direction, transparency against the gray
panel, pause/rotate behavior, hide/show and close/reopen. Request status after
close until cleanup is zero. A command result means acceptance, not hardware
PASS; record serial state and observed pixels. A GE fault may intentionally
retain resources until reboot. Do not run unmanaged plane/alpha writers.

`lv_aic_plane_test show /absolute/path` selects a native SDK media URI (maximum
127 bytes; no LVGL drive prefix). Close and wait for cleanup before choosing a
new file. The mailbox copies the path before acknowledging it, and all decoder/
widget calls remain on the UI owner thread. Media uses a 4 MiB CMA budget,
three extra frames and explicit BT.601 limited-range colorimetry. Use a fixture
with that colorimetry; this fixed test policy is not automatic color detection.
APNG retains its smaller decode budgets. Start with small supported media;
exceeding budgets or unavailable codecs should report FAULT, not be called a
successful decode. D13x's V30 profile does not include the SDK V10 H.264 codec.

The entry now permits RGB/YUV media testing, but neither codec output, A/V timing
nor whole-display rotation has been accepted on hardware. Hardware NOT_RUN.

The GE fill acceptance sequence now includes an offscreen raw-GE RGB565 color-key
measurement with eight channel endpoints and one intermediate sample. For every
sample it first records the engine's own unkeyed RGB565->ARGB8888 conversion of
the key color, classifies that conversion against the eight shift/replicate
channel combos, then tries the engine value itself, the replicating expansion
(`(c<<3)|(c>>2)`, `(c<<2)|(c>>4)`), LVGL's plain-shift expansion and both
zero-extended 16-bit placements as the color key. Keyed output must retain the
background on matched pixels, convert nonmatched pixels exactly like the unkeyed
baseline and leave destination stride padding untouched. The 2026-10-06 board run
matched `key=000000` but rejected LVGL's plain-shift white key `0xf8fcf8` while
the engine's own conversion wrote blue `0xff`, which is why the probe no longer
assumes one expansion. An unexplained sample is logged with all five candidate
values and no longer stops the remaining samples or the later GE2D blocks. On
uncertain DMA failure, 3072 bytes remain pinned until reboot. This diagnostic
bypasses production fallback only for its owned buffers; it does not enable
RGB565 key acceleration. Nine sample PASS lines with one common encoding are
finite evidence, not exhaustive format parity. Hardware execution is NOT_RUN.

The widget-enabled manual UI now adds a `List / menu compatibility` page after
SDK widgets. Scroll the twelve-item list, open Details in the menu and use its
back arrow, then use the shared header to change pages. Host pointer hit-testing
covers entering/leaving Details, repeated shared-header navigation, rendering
and three create/delete cycles. Physical scrolling/touch/rendering is NOT_RUN.

### Native-plane destination resize

`lv_aic_plane_test size WIDTH HEIGHT` submits checked decimal dimensions
(1..4096 each) to the LVGL owner thread. For example, after `show`, use
`pause`, `size 128 96`, `status`, then `resume`. Repeat at `size 96 64`,
with `rotate` and hide/show toggling. Check that the complete decoded image fills
the new transparent window, with no stale edges and no FAULT state. `status`
prints current window dimensions and both LVGL image scales (normally 256).
Keep the complete transformed window inside its parent/screen clip: dimensions
being syntactically valid do not establish visibility or hardware support.
If the geometry faults, close and reopen the test before continuing.

The shell only publishes copied scalar requests. Application and scanout occur
later on the UI thread; command success alone is not pixel/scanout acceptance.

## Native dynamic widgets

The widgets profile appends a page for native animated images, spinner,
image-button states, scale needle, styled spans and a scrollable window.
Use Pause / resume, tap the color bar to change value, and Open window to test
scroll/close behavior. Leaving the page stops its animations and closes the
window; values and the explicit paused state survive. See
[native widget validation](../../docs/native-widgets-stage.md).
