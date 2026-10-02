# Current capabilities and SDK gaps

Maintained inventory, 2026-10-03. Development branch: codex/sdk-basic-capabilities.
Comparison: the SDK's ArtInChip LVGL 9.1.0 implementation. Earlier
phase documents are historical; source presence and switches are not board proof.

| Area | Implementation | Remaining scope |
|---|---|---|
| Integration | App-owned pins; LV_OS_CUSTOM RT events | Board regression after app/OS refactor |
| Display | One framebuffer, DIRECT, PAN/VSync, software screen rotation | GE screen rotation, SPI/multi-display, extended cache/VSync tests |
| Touch / input | Touch worker, mapping, diagnostics and optional recovery; application-owned encoder and mouse providers create native LVGL indevs | Board-specific encoder/USB mouse sampling and board acceptance remain application scope |
| MPP | FILE and RAW/RAW_ALPHA memory JPEG/PNG; RGB888/ARGB8888; CMA tracking | Resource success inferred from test ordering; direct parity log pending; AICP/BMP/fake and YUV remain absent |
| Image cache | Component LRU, byte/entry bounds, decode-option keys, referenced-reader lifetime and explicit invalidation | Resource success inferred; direct cache-hit log pending; not transparent generic LVGL cache invalidation |
| GE FILL | Solid rectangles; partial opacity on RGB565/RGB888/XRGB8888, no radius/gradient | 12 board numeric probes and operator visual acceptance PASS; partial ARGB8888 still software |
| GE IMAGE | Four RGB/ARGB/XRGB formats, alpha, bounded scale, right-angle rotation plus scale, unscaled arbitrary-angle rotation; exact color key for RGB888/XRGB8888 and non-antialiased ARGB8888 without scaling or arbitrary rotation | Color-key ranges/RGB565/filtering, arbitrary-angle plus scale, YUV, tiling/recolor/masks |
| GE scale | Nominal 1/16..16; pivot/clip/per-axis handling | Small/unsafe geometry and D13x split interval fall back |
| GE LAYER | Plain composition, bounded 1/16..16 scale with right-angle rotation, and unscaled arbitrary-angle rotation when the child buffer is accessible | Ordinary D13x heap source and ROTATE regions outside 4..4096 fall back; arbitrary-angle plus scale, YUV and general HW layers remain absent |
| Scheduling | Synchronous, error/task counters, bounded refresh timing | Async work and paired GE ON/OFF board timing |
| Fonts | Optional native FreeType bitmap fonts: dynamic sizes/styles, Chinese fallback and native glyph LRU; real host render/lifecycle tests | New font image needs board validation; vendor AIC cache and global font-byte budget absent |
| GIF | Optional native LVGL 9.6 widget; FILE/RAW playback, pause/resume/restart; host pixel/lifecycle tests; board CLI panel | Default off; new GIF candidate needs board acceptance; no general GIF byte budget |
| Optional core | Host official demo selection; vector remains disabled | Target vector/demo choices and vendor extensions need separate integration |
| Native widgets | Optional upstream canvas/chart/dropdown/roller/slider/table/tabview/textarea/tileview plus arc/button/buttonmatrix/calendar/checkbox/keyboard/led/line/msgbox/spinbox/switch contracts | Board rendering/input acceptance still pending; deprecated list/menu and vendor camera/player/video-window remain outside this profile |

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
4. The operator confirms the current UI layout and page switching are normal.
   Retain complete native FreeType/resource probe logs for numeric acceptance.
5. Validate the optional [native GIF stage](gif-stage.md) on board. Its decoding
   and lifecycle host coverage does not establish DMA/cache or panel behavior.
6. Remaining priorities: whole-display GE rotation, board input providers and
   compressed vendor formats/media widgets as separate scopes.

This sequence supersedes the old instruction to stop after 3C5. It does not
waive hardware verification or authorize flashing.
