# SPI RGB565 near-unity GE conversion

The SPI converter now shares the ordinary RGB executor's preflighted stripe
planner. Near-unity enlargement uses balanced 16..31-pixel commands instead
of returning INVALID and selecting CPU packing. All four orthogonal rotations
and byte orders work through the existing converter/session API.

The scaler runs before rotation. Explicit steps are floor(source * 65536 /
output) on the source axes. Initial phase matches the SDK normal and CMDQ
INIT_PHASE rule: half-step for enlargement, half-step minus 32768 otherwise.
Other scaling intervals continue using the original SDK descriptor.

## Edge storage, budget and lifetime

The final enlarged source-X sample lies beyond the last original pixel center.
A duplicated final pixel in every owned source row supplies that filter tap
without exposing caller padding. The published GE width includes this edge;
all strip source crops and phases stay inside the owned stage. This changes
the source allocation formula to `align64((max_width + 1) * 2) * max_height`.
The destination remains `align64(output_width * 2) * output_height`. The caller's
budget must cover their sum; tightly sized callers can need an extra 64 bytes
per source row when the old row ended at an alignment boundary. Maximum actual
GE width stays 4096: striped enlargement always starts below the output width.

Every descriptor is validated before staging/cache/submission. GE reads and
writes only converter-owned CMA. All commands must complete before output
invalidation and CPU packing. Failure at any submit/emit/sync retains both
stages and the client. Original borrowed source/output remain reusable after
return; caller output remains unchanged on failure. A composed session keeps
its bus/tx claim, sends no SPI transfer and never replays a partially converted
frame through CPU packing. Panel initialization is still an application binding.

## Evidence

- Combined host **83/83 PASS**, disabled vector/SVG/Lottie baseline **78/78 PASS**.
- Real converter against descriptor-driven CPU model: 44 scenes / 93,328
  RGB565 pixels, all orthogonal rotations, both byte orders, Q16 phase seams,
  final-edge samples and 4095-to-4096 conversion. Every output pixel is written
  once. Input staging bytes and duplicated edges are checked.
- Nine DMA failure points in a three-strip sequence retain the allocations,
  leave caller output unchanged and reject further work. A real session failure
  after its first strip suppresses transport and retains its bus/tx claim.
- Eight new board probes exercise 63-to-64 RGB565 conversion offscreen for four
  rotations and both byte orders. They need no SPI bus/panel and check 512 pixels
  each plus guards, with one native RGB565 channel step tolerance.

Host modeling is not physical GE filtering evidence. Board execution, SPI panel
output and GE/SPI concurrency/performance remain **NOT_RUN**. Firmware identities
are recorded in [validation](validation.md). YUV chroma alignment/phase support
is the next separate stripe-planner extension.
