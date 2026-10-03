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

## Worker MPP PNG adapter (2026-10-03)

`compat/lv_aic_apng_decoder.h` / `port/lv_aic_apng_decoder.c` add a
serialized worker-only decoder under `AIC_LVGL_USE_APNG`. It depends on
application port + MPP + VE, independently of the SDK media player/audio.
Each extracted standalone PNG is CRC/structure checked before submission;
expected dimensions, disjoint caller storage, strides and capacities are
checked before decoding. This is not a sandbox for untrusted compressed data.

A fresh SDK PNG decoder is used for each frame because its reset method is
currently a TODO. Output uses the existing native ARGB8888 contract (BGRA
bytes on E907), then explicitly swaps R/B into straight-alpha RGBA8 for the
software compositor. Caller row padding is preserved. Frames with error flags,
wrong format/dimensions/crop, invalid stride/address or unowned allocation are
rejected before CPU copying. Allocator acquisition invalidates the CPU cache
before reading decoded pixels; the pin is released before SDK frame return.

Frame CMA and aligned packet-size limits are explicit. The frame limit does
not cover SDK internal VE scratch, the bitstream allocation or caller-owned
file/rectangle/canvas/PREVIOUS storage. The adapter does not allocate a second
full-frame CPU copy. Caller output is publishable only on a true result; a
failed frame return can occur after pixels were written. Such failure keeps
the decoder/frame/context alive and rejects new decode requests until close
succeeds. Destroy is retryable and must never be replaced by a forced free.

Validation: **36/36 host contracts PASS**, including the new adapter linked
against real SDK MPP headers, the real bounded allocator and mocked MPP/OSAL.
Tests cover 100 cycles, 3x2 pixels with independent source/destination padding,
straight alpha and channel order, create/control/init/packet/decode/get-frame
failures, invalid frames, input CRC/shape/span/alias rejection, a 31-byte budget
rejecting a 32-byte allocation, and failed put-frame followed by safe retry and
reuse. Mock pixels do not establish physical PNG codec correctness.
Strict E907 compilation **PASS**, `output/lvgl-apng-decoder.o` SHA256:
`89c6b017d5ff8c579ac05c562a9673b88c8cabf57e7f77652e35ca5e86e4a09b`.

This stage adds no new firmware image. File loading, background orchestration,
immutable publication, widget/backend switching and physical playback remain
open; the existing player backend is unchanged.

## Integrated serialized stream (2026-10-03)

`compat/lv_aic_apng_stream.h` / `port/lv_aic_apng_stream.c` connect the container,
MPP decoder, compositor and timeline into a worker-owned playback core. Open
copies the input bytes, validates limits and preallocates the maximum extracted
PNG and decoded rectangle plus canvas/PREVIOUS storage. `cpu_budget` bounds
those stream allocations and the stream context; malloc overhead and the
separate decoder/allocator metadata are excluded. CMA output and SDK packet
limits remain separate. Stream open does not decode or publish a frame.

Each tick decodes/composes at most one due frame, never skips a disposal-dependent
frame, and reads the monotonic clock again after blocking decode/composition.
This starts the first frame's delay after it is ready and accounts for later
decode cost without drifting the deadline. A default poster is excluded;
each play resets canvas/history and finite completion retains the final frame.
Pause/rational rate are supported; replay resets the timeline and play count
while preserving requested pause/rate and monotonic publication sequence.

The returned RGBA canvas is **borrowed, mutable worker storage**. It must be
copied into immutable publication storage before any asynchronous UI/GE reader
uses it. Non-frame tick results retain the last canvas, or NULL before the first
frame. False leaves the result structure unchanged but latches stream fault;
restart cannot clear it. Failed frame return keeps stream/decoder ownership
alive across repeated close attempts. A worker must never force free it.

Validation: host **36/36 PASS**, extending the real-MPP-ABI adapter test through
the real stream/parser/extractor/allocator/compositor/timeline stack with mocked
engine pixels and a controlled clock. Covers static PNG, excluded poster,
finite/infinite loops, loop canvas reset, final hold, source-copy ownership,
CPU/packet/minimum-delay limits, pre-start pause, 2x rate, restart preserving
pause/rate, post-decode clock sampling, late ordered catch-up, clock reversal,
decode fault latching and failed frame-return cleanup retries.
Strict E907 compile **PASS**; `output/lvgl-apng-stream.o` SHA256:
`8f9349133bef79c070c48d1f6a966fae014e718f8e1661d5ca6687e323dd11c4`.

Remaining: file loading, OSAL worker/control mailbox, immutable frame publication,
widget/backend selection and board PNG/playback validation. No new firmware
image or physical-playback claim accompanies this core-only stage.

## Immutable frame publication (2026-10-03)

`compat/lv_aic_apng_frames.h` / `port/lv_aic_apng_frames.c` bridge the worker's
borrowed RGBA canvas into native ARGB snapshots. The pool preallocates 2..8
frames under an explicit metadata+pixel budget (malloc overhead and LVGL
image/decoded caches excluded). One serialized producer copies/converts outside
the mutex; one LVGL owner polls and creates an RGB image on its own thread.
Only unconsumed READY storage may be replaced by newer frames. Sequence numbers
increase after successful publication; invalid spans, pool-backed input aliases,
closed publication and all-slots-retained backpressure reject publication.
The stream must still compose every frame when publication is dropped.

Image creation retains a slot until the image owner and all decoder/native/GE
readers release it. Close discards unconsumed frames but never frees readers or
an in-flight copy/poll. Destroy is permitted only after producer/poll calls have
stopped and returns false until those slots are released. An uncertain GE DMA
lease therefore keeps the pool alive rather than allowing overwrite/free.

Validation: **37/37 host contracts PASS**, including real LVGL RGB image leases,
channel/alpha conversion, producer source mutation after copy, latest-frame
replacement, initialization failure, invalid capacity/stride/sequence/alias,
reader backpressure, close/discard and delayed release. A pthread producer runs
1000 publications concurrently with owner polling and pixel validation.
Strict E907 compile **PASS**; `output/lvgl-apng-frames.o` SHA256:
`f805c3b8cb6ee3a89edb302c652ac81a70564512622cb396367f4062568c7d90`.

OSAL background orchestration, file loading and widget/backend selection still
remain. This standalone bridge has no worker integration or board acceptance yet.
