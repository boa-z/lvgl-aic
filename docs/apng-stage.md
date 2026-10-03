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
- APNG composition, scheduling, widget/backend selection, real MPP PNG decode
  and GE/board execution are still **NOT_RUN / not integrated**. The existing
  media firmware is not an APNG playback image.
