# Current capabilities and SDK gaps

Maintained inventory, 2026-09-30. Application integration baseline:
2294faf; candidate development branch codex/sdk-basic-capabilities. Comparison: the SDK's ArtInChip LVGL 9.1.0 implementation. Earlier
phase documents are historical; source presence and switches are not board proof.

| Area | Implementation | Remaining scope |
|---|---|---|
| Integration | App-owned pins; LV_OS_CUSTOM RT events | Board regression after app/OS refactor |
| Display | One framebuffer, DIRECT, PAN/VSync, software screen rotation | GE screen rotation, SPI/multi-display, extended cache/VSync tests |
| Touch | Worker, mapping, diagnostics, optional recovery | Encoder/USB mouse are placeholders |
| MPP | FILE and RAW/RAW_ALPHA memory JPEG/PNG; RGB888/ARGB8888; CMA tracking | Resource success inferred from test ordering; direct parity log pending; AICP/BMP/fake and YUV remain absent |
| Image cache | Component LRU, byte/entry bounds, decode-option keys, referenced-reader lifetime and explicit invalidation | Resource success inferred; direct cache-hit log pending; not transparent generic LVGL cache invalidation |
| GE FILL | Solid rectangles; partial opacity on RGB565/RGB888/XRGB8888, no radius/gradient | 12 board numeric probes and operator visual acceptance PASS; partial ARGB8888 still software |
| GE IMAGE | Four RGB/ARGB/XRGB formats, alpha, bounded scale, right-angle rotation plus scale | Arbitrary angles, YUV, tiling/recolor/masks |
| GE scale | Nominal 1/16..16; pivot/clip/per-axis handling | Small/unsafe geometry and D13x split interval fall back |
| GE LAYER | Plain/right-angle composition, no scale | Ordinary D13x heap source falls back; not general HW layers |
| Scheduling | Synchronous, error/task counters, bounded refresh timing | Async work and paired GE ON/OFF board timing |
| Fonts | Optional native FreeType bitmap fonts: dynamic sizes/styles, Chinese fallback and native glyph LRU; real host render/lifecycle tests | New font image needs board validation; vendor AIC cache and global font-byte budget absent |
| Optional core | GIF/vector/demo are configuration choices | GIF disabled here, enabled in vendor configuration |
| Vendor widgets | Not integrated | Camera/player/video-window/canvas/roller/swipe as separate app dependencies |

Declined drawing normally stays with software. Unsupported compressed resources
do not imply another decoder can read them. Whole-screen rotation and IMAGE
rotation are different paths.

## Evidence and sequence

Earlier images have display/touch, decoder stress, GE FILL/IMAGE and selected
transform board observations. LAYER was composited in software. Rotation and
combined-transform visual confirmations do not replace numeric coverage of all
pivot/clipping cases. 3C5 timing code exists; paired board timing remains open.
Current candidate: numeric fill/blend/scale and CMA stress PASS in the supplied
serial excerpt; the operator confirms normal interface appearance. Resource
logs and explicit touch/timed-running evidence remain incomplete. Async log
overflow and timing format compatibility are corrected in the new font candidate;
board confirmation remains required. See [font stage](font-stage.md).

1. Keep current docs aligned while preserving dated validation records.
2. Translucent FILL numeric probes and operator visual acceptance have passed.
   Preserve the geometry/address guards and the 12-probe regression coverage.
   Keep partial ARGB8888 on software until separately verified.
3. Complete the [resource-stage board gate](resource-stage.md): memory inputs
   and bounded cache now include ownership, invalidation, pressure/failure, LRU
   and teardown contracts plus file-memory pixel parity and cache-hit probes.
   SDK allocators remain unchanged. Development checks are in validation.md.
4. Validate the new native FreeType stage on board, including complete probe logs.
5. Remaining priorities: whole-display GE rotation and required encoder/mouse
   inputs; compressed vendor formats and media/widgets need separate scope.

This sequence supersedes the old instruction to stop after 3C5. It does not
waive hardware verification or authorize flashing.
