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
- Scheduling, widget/backend selection, real MPP PNG decode
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
frame publication, APNG delay/loop/rate scheduling, source/backend selection,
seek/groups and hardware GE/board tests remain unfinished. The currently saved
media image predates this APNG foundation and does not exercise it.
