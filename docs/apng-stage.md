# APNG backend port / APNG 后端移植

## Container and standalone frame extraction

`common/lv_aic_apng.[ch]` is a pure C immutable-memory parser/extractor. The
caller supplies file/frame-PNG byte limits, canvas pixel and frame-count limits;
the parser allocates no memory and bounds dimensions to 4096. It validates PNG
signature, all chunk CRCs, ordering, APNG sequence/count, frame rectangles,
blend/dispose values and palette/transparency structure. A normal PNG produces
one static frame; an APNG default poster without fcTL is excluded from animation.
A zero delay denominator becomes 100; zero delay numerator remains zero for the
playback policy to resolve. Loop count is preserved (zero means indefinite).

Frames become standalone PNG files with frame-sized IHDR, retained global color
chunks, fdAT converted to IDAT, and regenerated CRCs. Caller-owned storage and
explicit encoded output capacity are required; overlap with source is rejected.
Parser success establishes container structure, not zlib/pixel validity. Keep
source bytes and document/frame metadata immutable for their lifetime.

SDK private dcTL is CRC-checked but its optimization hints are ignored. Standard
APNG data remains sufficient; no SDK-private LVGL structs are imported.

## Evidence (2026-10-03)

- Host contract checks static/animated PNG, included/excluded default image,
  palette+tRNS, fragmented IDAT, extraction capacity/aliasing, truncated input,
  CRC/sequence errors, invalid rectangles/ops and explicit resource limits.
- `tests/host/apng_sdk_probe.py` invokes the C extractor against the existing
  SDK clock/world-cup/ayanami_rei examples. It verifies PNG CRCs with Python zlib,
  exact compressed frame payload equality, dimensions, palette/transparency and
  PNG rectangle decoding with Pillow: **100/100 frames PASS**. SDK assets remain
  in the SDK checkout and are not copied into this component repository.
- Strict E907 compilation through check-player-session.ps1: **PASS**.
- Integrated worker scheduling, widget/backend selection, real MPP PNG decode
  and GE/board execution are still **NOT_RUN / not integrated**. The existing
  media firmware is not an APNG playback image.

## Straight-alpha canvas composition

`common/lv_aic_apng_compose.[ch]` adds a CPU RGBA8 reference canvas with explicit
caller-owned canvas, stride/capacity and scratch storage. It allocates nothing.
SOURCE replaces rectangle pixels; OVER uses straight-alpha source-over arithmetic.
Before each new frame, previous NONE/BACKGROUND/PREVIOUS disposal is applied.
PREVIOUS backs up only the required rectangle into canvas-sized scratch storage.
The first PREVIOUS restores the initial transparent background. Last-frame
pixels remain visible until another frame/reset; disposal is not applied early.
Reset clears pixels and history while preserving row padding.

Input rectangles, byte spans and source/storage overlap are validated before
modification. Invalid input preserves canvas/history. This API expects decoded
frame-rectangle pixels, not an already-composed APNG image. Its RGBA byte order
is explicit and must be converted appropriately when publishing LVGL/native MPP
formats. Scratch/canvas access is serialized by the future backend owner.

Validation: **34/34 host tests PASS**, including SOURCE/OVER alpha arithmetic,
PREVIOUS/BACKGROUND restoration, first-frame PREVIOUS, transparent source and
padding/capacity guards. Strict E907 compilation **PASS**. The SDK asset probe
now feeds Pillow-decoded rectangles into the C compositor and independently
composes them with Pillow alpha_composite/paste/disposal: **100/100 frames PASS,
maximum channel error 0** for clock, world-cup and ayanami_rei.

Command (Python with Pillow, separate from the MSYS compiler PATH):

```powershell
python tests/host/apng_sdk_probe.py --sdk ../../../../.. --exe output/lvgl-host-ge/lvgl_aic_apng_contract.exe --output output/apng-sdk-probe
```

Evidence is output/apng-sdk-probe/result.json and per-frame PNG/RGBA files.
Assets are local SDK inputs and are not published in this repository. This is
software reference evidence only: file/MPP decode worker, bounded asynchronous
frame publication, APNG timeline integration, source/backend selection,
seek/groups and hardware GE/board tests remain unfinished. The currently saved
media image predates this APNG foundation and does not exercise it.

## Worker timeline foundation (2026-10-03)

`common/lv_aic_apng_timeline.[ch]` supplies serialized, allocation-free control
for a future APNG worker. It returns FRAME/WAIT/PAUSED/ENDED and a conservative
microsecond wait; it never sleeps. Every disposal-dependent frame is requested
in sequence even after late decode. The caller may skip publishing intermediate
results during catch-up, but must still decode/compose them. Each play begins
with reset_canvas so PREVIOUS/BACKGROUND cannot leak across animation loops.
Finite plays end only after the final frame's delay. Static PNG ends after its
single commit while the consumer retains its pixels.

Rate is an explicit rational numerator/denominator, each <=1,000,000 and ratio
0.1..10. No floating-point clock drift is introduced. Media progression carries
fractional rate time; frame deadlines use 32 fractional bits below a microsecond.
Pause freezes media time, rate changes preserve the current position, and an
explicit minimum delay (1..1,000,000 us) handles zero/too-short frame delays.
Backward time, invalid rates and overflow fail without changing clock/output.
A rate-denominator change may truncate less than one microsecond of fractional
media time. Wait hints round conservatively; actual due decisions include the
fractional deadline. The existing SDK-paced video worker does not use this clock.

Host **35/35 PASS**: finite/infinite loops, final hold, zero-delay floor, static
PNG, pause/idempotent pause/resume, 0.1x/2x/rational rates, late decode ordering,
6000 frames at 60 Hz without whole-microsecond-per-frame drift, clock endpoints,
invalid-rate/backward/overflow transactions. Strict E907 compilation **PASS**;
output/lvgl-apng-timeline.o SHA256:
`65b3dc0a153a2fa284f35675e64b88f04c2e239e117d11555804950474861495`.

MPP integration review: SDK PNG supports ARGB/ABGR/RGBA/BGRA8888 output, but the
existing LVGL MPP image decoder owns LVGL objects and must not be called directly
from a background APNG worker. A separate bounded MPP PNG adapter, explicit
RGBA/native byte-order handoff, asynchronous immutable publication and widget
backend selection remain to be implemented. No new APNG firmware image or
physical playback acceptance is claimed by this stage.
