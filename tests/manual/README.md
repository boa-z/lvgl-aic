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
probe with eight channel endpoints and one intermediate sample. It compares
keyed output against a no-key conversion, rejects an unchanged baseline, checks
matched background retention, nonmatched pixels and destination stride guards.
On uncertain DMA failure, 3072 bytes remain pinned until reboot. This diagnostic
bypasses production fallback only for its owned buffers; it does not enable
RGB565 key acceleration. Nine sample PASS lines would be finite evidence, not
exhaustive format parity. Hardware execution is NOT_RUN.

The widget-enabled manual UI now adds a `List / menu compatibility` page after
SDK widgets. Scroll the twelve-item list, open Details in the menu and use its
back arrow, then use the shared header to change pages. Host pointer hit-testing
covers entering/leaving Details, repeated shared-header navigation, rendering
and three create/delete cycles. Physical scrolling/touch/rendering is NOT_RUN.
