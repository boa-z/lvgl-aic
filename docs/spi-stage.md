# Application-owned SPI display migration

## Scope and SDK reference

SDK `aic_widgets/aic_spi/lv_aic_spi.c` provides direct RGB565 blit, exclusive
producer claims, draw buffers, per-panel geometry and timing, plus LVGL display
integration. It copies/scales/byte-swaps into internal transmit buffers before
asynchronous `aic_spi_lcd_flush`; the preceding transfer is waited before another
submission. Its sample bus/panel/pin defaults are not adopted by this component.

## RGB565 transmit-frame preparation

`lv_aic_spi_pack_rgb565` is the first implementation layer. It uses caller-owned
CPU-coherent little-endian RGB565 input with explicit stride/capacity and writes
packed output with an explicit byte-swap flag. Clockwise 0/90/180/270 rotation is
followed by nearest-neighbour scaling to the requested dimensions. Pixel mapping
is `floor(output_coordinate * rotated_extent / output_extent)`. This software
path does not claim the same filtering as the SDK's GE resize implementation.
Dimensions are bounded to 1..4096; pointer arithmetic, minimal source span,
output capacity and overlap are checked before any output byte is written.
Input pixels/padding are unchanged. Output guards beyond the packed frame are
untouched. No heap allocation, SDK access, guessed bus mapping, panel commands
or default wire byte order occurs here. Large conversions belong on a worker.

The SDK SConscript already includes `common/*.c`, so the source is available to
application consumers; unused code can be discarded by the linker. It adds no
LVGL or SDK header dependency. Callers must ensure transmit storage is idle
before writing and handle DMA cache cleaning and completion before reuse.

## Evidence and remaining stages

Host **58/58 PASS**, including literal nonsquare rotation grids, both byte
orders, 2x scaling, downsampling, padded source rows, destination guards, invalid
geometry/angle, stride overflow, insufficient capacities and source/destination
overlap. Log: `output/spi-frame-tests.log`.
D13x compile **PASS** with `-std=c99 -Wall -Wextra -Werror
-march=rv32imafdcpzpsfoperand_xtheade -mabi=ilp32d -I include`;
SDK `output/lvgl-spi-frame-standalone.o` defines `lv_aic_spi_pack_rgb565`.

At the frame-packer stage these items were pending; subsequent sections record
implemented session ownership, bounded CMA allocation and checked completion.
Currently pending: concrete panel initialization, LVGL 9.6 display/worker and
blit-producer integration, power/TE handling, statistics and multi-display acceptance.
No sensor/panel hardware is inferred or initialized. No camera/SPI final firmware
link, physical SPI transfer, panel output or performance acceptance is claimed
by this stage; hardware status is **NOT_RUN**.


## Checked transfer-session core

`lv_aic_spi_transfer_create/submit/drain/close` now manage borrowed transmit
storage and checked transport callbacks. One owner worker calls these APIs;
callbacks may attempt reentry but receive BUSY. Concurrent thread access is not
supported. Each submit confirms completion of any previous transfer before
repacking. Accepted submission marks the pixels pending; close waits before
freeing metadata. The session never frees caller pixel storage. Invalid frame
parameters do not submit; they may drain an earlier pending transfer first.
A false start/wait callback permanently faults the session, retains metadata
and borrowed storage, and blocks later repack, submission and close. There is
no force-reset operation while DMA ownership is uncertain.

This core does not allocate DMA pixels, claim a bus, initialize a panel or
provide a real SDK transport adapter yet. The application callback must perform
cache handoff before DMA and return true from wait only when no DMA reader can
remain. SDK `aic_spi_lcd_wait_completion` returns void and discards the underlying
`rt_spi_wait_completion` status; using it as unconditional success would break
this contract. The D13x lower driver waits with a finite timeout, so the future
adapter must preserve that failure instead of silently releasing tx storage.

Host **59/59 PASS**, log `output/spi-transfer-tests.log`: repeated lifecycle,
wait-before-rewrite with retained byte snapshots, invalid submission, callback
reentry, submission failure and completion failure with sticky retention.
Real SDK SPI adapter, device exclusivity, panel setup, LVGL flush integration,
DMA timing and board output remain incomplete/NOT_RUN.


Transfer-core target regression: boot/app/static/image/manifest **PASS** after
moving the earlier standalone compile object out of the component source tree.
Target object defines all four transfer APIs. Clean component
`0bbe23786e801375fd1ee46f5597dfc847e3e6c2`, SDK
`d8ffb50f5ae0e0db4e1c4e07b9256c81d1e0e989`. Image SHA256
`a18bec5557969d8cef200b7e635443881b3140657b9e7c5235796e35b3b3f3e8`.
Evidence: SDK `output/lvgl-evidence/ge2d-fonts-gif-widgets-aicp-player-apng-barcode`.
Unused SPI APIs may be discarded from the final firmware; this regression
establishes target compilation/general integration, not SPI device linkage or
physical output. Hardware remains **NOT_RUN**.


## SDK checked completion bridge

Opt-in `AIC_LVGL_USE_SPI_SDK` provides `lv_aic_spi_sdk_wait_complete` as the
transfer core's wait callback. Its context is the underlying borrowed
`struct rt_spi_device *`, not the panel wrapper. It checks bus/ops pointers,
requires both wait_completion and gstatus, propagates `rt_spi_wait_completion`
failure, and then accepts only HAL OK or explicit TRAN_DONE. In-progress,
FIFO errors and unknown status bits all fail, even when a completion signal
was received. Device exclusivity and an accepted asynchronous submission are
preconditions; this callback does not acquire lifetime ownership of the bus.

Why not use the SDK panel wrapper directly: `aic_spi_lcd_wait_completion` is void
and drops the lower result; `rt_spi_get_transfer_status` defaults to OK when
no gstatus callback exists. Both would otherwise create false completion proof.
The lower D13x driver waits with a timeout and exposes HAL error bits through
gstatus. Also, SDK `spi_flush` returns zero for nonnegative transfer counts,
so its short-write path still needs a checked submission adapter. QSPI display
mode uses void submission operations and needs separate validation.

Host **60/60 PASS** includes missing operations, wait failure, normal completion,
all 31 nonterminal/error bits and each combined with TRAN_DONE. Strict D13x
compile **PASS** via `tools/sdk/check-spi-session.ps1`, which uses actual SDK
headers and checks references to both lower RT SPI functions. Object SHA256
`99d52305f8869fbc06d062908a988bc319874dc04b0211af0ef4399c9bc13919`;
logs `output/spi-sdk-tests.log`, `output/spi-sdk-target.log`.
No runtime SDK link/transfer, SPI panel initialization or board execution is
claimed. Checked submission, device ownership and display integration remain
next stages; hardware **NOT_RUN**.


## Checked SDK QSPI submission

`lv_aic_spi_sdk_submit_qspi` now checks exact acceptance from
`rt_qspi_transfer_message`, instead of treating a nonnegative high-level panel
flush result as success. It accepts an explicitly supplied 0..4-byte MSB-first
prefix, prefix lane count and pixel lane count. A zero-prefix transfer requires
zero prefix lanes; active lanes must be 1/2/4. It builds a fully zeroed extended
message, with no dummy/alternate stages or receive buffer. This is deliberate:
D13x `drv_qspi_send` reads QSPI extension fields even for one-lane transfers.

The borrowed device must be an actual configured `rt_qspi_device`; no object
casts from a smaller SPI device are required. The function verifies configure,
xfer, nonblock, wait and status operations, requires an idle/terminal status,
and checks the nonblocking-mode return before submission. Null/zero/oversized
payloads, 32-bit DMA-address overflow, malformed prefix/lane counts and partial
acceptance are rejected. A false submission may already have started hardware,
so the owner must preserve storage (the transfer core's sticky fault policy).

This is a checked low-level adapter, not panel initialization. Application code
must exclusively own the bus, provide panel-specific framing/D-C setup, clean
CPU cache before calling, and retain pixel storage until checked completion.
Special QSPI display mode and its void-return submission API are not connected.
The device claim manager and complete panel-to-transfer callback binding remain
open. No hardcoded SDK sample pins, panel choice or default byte order is used.

Validation: **60/60 host PASS**, plus focused missing-driver-op regression PASS;
strict real-header D13x submit/completion compile and all four SDK references
**PASS**. Target object SHA256
`b8138504a0f77f7862adb82a31d5cfbba498cfba18330f3d0d49465e0f6d1055`.
Logs: `output/spi-submit-tests.log`, `output/spi-submit-target.log`.
Actual transport linkage, panel commands on hardware and DMA timing remain
**NOT_RUN**. This stage does not claim a functioning SPI display yet.


## Composed SDK session and cache ownership

`lv_aic_spi_session_open/submit/drain/close` combine the frame packer, checked
transfer core and SDK submit/completion bridge. Applications provide an already
initialized `rt_qspi_device`, explicit frame geometry/framing/byte order and
dedicated 64-byte-aligned DMA storage. Capacity must include the whole cache-line
rounded packed frame. No pixel memory is allocated and no panel/bus/pins are
initialized. The session cleans exactly that bounded tx region before submission.
Successful close drains DMA before releasing metadata and component ownership.

A nonblocking atomic registry reserves one session per bus and rejects overlapping
cache-rounded tx regions even across different buses. Open rejects active/error
hardware status. Registry locking never spans completion waits. A busy registry
rejects open or requests close retry; each live session still requires a single
owner worker. Unmanaged SDK users remain the application's responsibility. They
must not access the reserved device/bus or mutate its configuration. Faults retain
both the bus claim and borrowed tx region indefinitely. There is no force release.

Host **61/61 PASS**, followed by focused active-bus admission regression PASS.
Tests use real mapped low-address storage, real frame/transfer/session/SDK bridge
code and mocked driver/cache calls: cache extent/order, non-square swapped pixels,
wait-before-rewrite, duplicate bus, shared-buffer rejection, separate bus admission,
normal reopen and timeout retention. Logs `output/spi-session-tests.log` and
`output/spi-session-target.log`. D13x strict compile and partial link **PASS**;
all component SPI references resolve within the combined object, while OS/driver
functions remain unresolved for the final application link. Combined object SHA256
`bad0ad4bb934790a09569282fd139f1e52daaaf3d6de54a3cf0709ab17746ff4`.

Still open: real panel initialization/D-C setup,
LVGL display/flush integration, SDK-shaped blit API, QSPI display mode and target
runtime acceptance. A provided preconfigured panel can use this composed session,
but no physical SPI transfer has been performed here. Hardware **NOT_RUN**.


## Owned CMA session storage

`lv_aic_spi_session_open_owned(config, pixel_budget)` provides application-owned
CMA allocation for the composed session. Set `tx=NULL` and `capacity=0`;
the budget covers RGB565 pixel bytes rounded up to 64 bytes, excluding metadata.
The buffer is allocated with `aicos_malloc_align(MEM_CMA, ..., 64)` and passes
through the same address, capacity, bus and overlap checks as borrowed storage.
Failed admission frees the temporary allocation. Successful close waits for
checked completion before freeing CMA; BUSY or FAULT retains owned storage.
A faulted session continues reserving its bus and buffer until reboot.

Validation: **61/61 host PASS**, including budget rejection, allocation failure,
misalignment and invalid configuration cleanup, ten balanced allocate/submit/close
cycles, and fault retention. A same-bus reopen uses a distinct temporary buffer
and frees only that rejected buffer. D13x real-header compilation and component
partial link **PASS**, including the new public API. Combined object SHA256:
`e90eca4707a6c506cbf0c7954a403ec9dc2ed23c7e7f3fb31847699c4c012e59`.
Logs: `output/spi-owned-tests.log`, `output/spi-owned-target.log`.
OS/driver references remain for final application linkage; no SPI panel runtime
or hardware acceptance is claimed (**NOT_RUN**).


## Panel preparation within the transfer lifetime

Session config now accepts optional synchronous `prepare(context, width, height)`
and `prepare_context`. This is the application hook for panel-specific full-frame
window commands, RAMWR and D/C sequencing: SDK single-lane `spi_flush` performs
RAMWR and D/C changes for every frame, so a pixel-only submission is insufficient.
The hook runs after the previous checked DMA completion and frame packing, before
cache clean and pixel submission. It must complete its own command transfers and
leave terminal bus status; the checked pixel bridge verifies that status again.
The claimed device bus is rechecked after the callback. No panel/pin defaults are
introduced and no unchecked SDK void completion result is treated as success.

A failed preparation faults the transfer conservatively, since panel command DMA
may already have started. No pixels are submitted, no retry is attempted, and
session storage/claims remain retained. Callback context must survive until
successful close, or reboot after a fault. Reentrant drain/close return BUSY.
Existing zero-initialized configs retain pixel-only behavior with a null hook.

Validation: **61/61 host PASS**, including two-frame prepare ordering, reentrant
close/drain rejection, failed prepare without pixel/cache handoff, sticky fault
and retained bus claim. Real-header D13x compile/component partial link **PASS**.
Combined object SHA256:
`77ec0b08db244ba16f06227761d27247b0524510c9672a61227667a1b5988cf5`.
Logs: `output/spi-panel-tests.log`, `output/spi-panel-target.log`.
Concrete panel command adapters, LVGL display/worker integration and physical
transfers remain open; hardware **NOT_RUN**.


## Checked command/data writes

`lv_aic_spi_sdk_write_qspi` composes exact-count submission with checked terminal
completion on the exclusive worker. It can be called from session `prepare` for
panel command and parameter payloads. For single-lane panels, set D/C to command,
write a persistent one-byte command payload with no prefix, then only after true
set D/C to data and write persistent parameters. Supply cache-clean DMA storage;
no stack command arrays. QSPI prefix framing uses the same explicit fields as
the pixel submit API. Empty/prefix-only transactions are not supported here.

Any false result stops the sequence: caller must return false from prepare and
retain command storage/context, because short submission or timeout may leave
DMA outstanding. This helper does not initialize a panel, toggle pins, claim a
bus, clean cache, cancel DMA or make the SDK command wrappers safe implicitly.
It closes the checked nonempty command-write gap while concrete panel adapters
and the display worker remain pending.

Validation: 61/61 host tests PASS, covering exact and short command submission,
wait timeout, in-progress/error status after accepted submission, explicit DONE,
active-bus rejection and null device. D13x real-header compile/partial link PASS;
combined object SHA256
`a94eddf1402b17f5f1afeef999fc0bbe19a6c790316e7f794b9f92ffcee42c44`.
Logs: `output/spi-write-tests.log`, `output/spi-write-target.log`.
Hardware NOT_RUN; this is not evidence of a working physical SPI display.


## Reusable panel command sequence

`lv_aic_spi_panel_create` snapshots up to 64 explicit command/data descriptors.
All payload spans, 64-byte alignment/cache-rounded capacity and wire lane/prefix
fields are validated before hardware activity. Payloads remain borrowed immutable
DMA storage. The application supplies synchronous checked D/C control, optional
for prefix-framed panels, and the final D/C level. Geometry is explicit; no panel
commands, pins, offsets or byte order are guessed.

Bind `lv_aic_spi_panel_prepare` and the returned panel context to session prepare.
Each step sets D/C, verifies terminal bus state, cleans the dedicated cache span,
then performs checked command write/completion before continuing. Descriptor
changes after create do not alter the snapshot. Geometry mismatch rejects without
hardware activity. A pin/transport/status failure becomes sticky and prevents
replay; close refuses busy or faulted state. Payload/context lifetime extends to
reboot on fault. First close the referencing session successfully, then the panel.
This adapter relies on the session/application's exclusive bus ownership; it does
not independently claim hardware or permit sharing a device with other clients.

Validation: **62/62 host PASS** including repeated frame command ordering,
preflight rejection, descriptor snapshot, geometry rejection, close reentrancy,
wait failure stopping later commands, pin failure without submission and sticky
fault retention. Strict D13x compile/component partial link PASS; combined SHA256
`551a29c63c779f2ba6907bb9ff6db6eefeded57a7502a2dac20753d0b091a546`.
Logs: `output/spi-script-tests.log`, `output/spi-script-target.log`.
This is the reusable sequence executor, not a specific panel's initialization
script. LVGL display/worker integration, device-specific configuration and actual
SPI execution remain open. Hardware **NOT_RUN**.


## Full regression and display-worker integration requirements

The standard GE/fonts/GIF/widgets/AICP/player/APNG/barcode profile completed
bootloader build, application link, static integration checks, image checks and
provenance generation with clean component `0623fafa229e3c6358564e8ee16fcd3fcbe46a86`
and SDK `ff711ba970a6c492ec0b2317ae3ec64a202d1cac`. Image SHA256:
`7857af1e1078c07d7e09c2966229c7a188ea3e78c2b9728ee060b61506eb9867`.
Manifest: SDK `output/lvgl-evidence/ge2d-fonts-gif-widgets-aicp-player-apng-barcode/manifest.json`.
This profile disables SPI transport: enabled SPI evidence remains the separate
real-header compile/partial link and host contracts, not final device linkage.

The SDK reference `aic_widgets/aic_spi/lv_aic_spi.c` uses `display_sem` and
`sync_ready` to hand a frame to `disp_thread`, two transmit buffers, a separate
blit producer claim, and per-panel timing/TE/power handling. The next integration
must implement the following, rather than treating callback registration as parity:

- Keep the LVGL source frame alive until the worker has finished reading it;
  release flush ownership exactly once, including validation and transport errors.
- Give one worker exclusive session access; bound queued work and prevent overwrite
  while packing or DMA is active. Keep large transforms outside LVGL refresh.
- Preserve source/command/tx lifetime on failure; completion of LVGL source use
  is distinct from successful physical presentation and must be recorded separately.
- Stop admission and join/drain a worker before deleting its display, session,
  command context or buffers. Faulted DMA resources cannot be force-released.
- Support an explicit exclusive direct-blit producer and prevent simultaneous
  LVGL ownership; validate geometry/stride and rotation at the boundary.
- Bind panel power, TE and initialization through explicit application configuration,
  and report submitted/completed/failed frame counts and per-panel timing.

Host concurrency/lifetime tests, enabled target compilation/final linkage and
multi-display board testing remain required. Hardware **NOT_RUN**.


## Producer/worker frame handoff

`lv_aic_spi_handoff` provides one bounded slot between one LVGL producer and one
exclusive session worker. `put` snapshots the frame descriptor and borrows its
immutable CPU source. `run` performs session submit and checked drain, publishing
completion with release/acquire atomics. `take` runs on the producer, consumes the
cookie/result exactly once and releases source ownership. No LVGL display calls
occur on the worker. Source may be reused after consuming even a failed result;
the session's distinct DMA transmit/command storage remains retained on fault.

`stop` permanently rejects new frames while preserving queued/completed work.
After that work has run and been consumed, join the worker before `close` frees
handoff metadata. Closing a faulted handoff does not free its borrowed session,
DMA buffers or panel context. Unexpected session BUSY also disables admission;
external session access violates the exclusive-worker contract. No pixel queue
allocation, implicit frame dropping or unbounded buffering is introduced.

Validation: **63/63 host PASS**, including a real pthread producer/worker test
with 1000 successive source handoffs, backpressure while completion is pending,
exactly-once results, queued stop/drain, fault reporting and invalid submit without
a completion wait. Strict D13x compile/partial link PASS, with no unresolved
atomic runtime helper. Combined object SHA256:
`07268990bc528a081777c19aff5cccf835cadb4727e3bd147c98e4c17052793c`.
Logs: `output/spi-handoff-tests.log`, `output/spi-handoff-target.log`.

This is the frame lifetime primitive. OS thread creation/wakeup/join, actual LVGL
display binding/flush-ready, direct-blit mode and enabled-device final linkage
remain to integrate. Host concurrency is not target scheduler or panel evidence;
hardware **NOT_RUN**.


## RTOS session worker

`lv_aic_spi_worker_create` starts an RT-Thread OSAL worker around the bounded
handoff. Stack size and priority are explicit (minimum stack 1024, priority must
fit RT-Thread's configured range). The worker alone calls session submit/drain.
Producer submit/take/stop/close APIs remain on one UI thread; they never call
LVGL display functions. A semaphore wakes the worker, with a 10 ms bounded wait
so notification failure does not orphan accepted work.

Stop closes admission first, then publishes the exit request. The worker checks
for a final queued frame after observing stop, finishes it and publishes resource
quiescence as its final context access before returning. Close is nonblocking:
it refuses until quiescence and consumption of outstanding completion. This is
an application-resource lifetime handshake, not an OS thread-reclamation join;
RT-Thread reclaims the returned dynamic thread. No forced deletion is used.
Session/tx/command contexts are borrowed and are not freed by worker close.
On transport fault they retain their separate reboot-only lifetime.

Validation: **64/64 host PASS**. Pthread-backed OSAL contract covers semaphore
and thread allocation failures, priority/stack rejection, lost wake recovery,
stop while DMA completion is blocked, refusal to close active/unconsumed work,
fault delivery and balanced semaphore cleanup. D13x real OSAL headers compile
and component partial link PASS. Combined object SHA256:
`145af3afa3b084934722abb90c207370935a5cea7e5e017176236a8a11a700c2`.
Logs: `output/spi-worker-tests.log`, `output/spi-worker-target.log`.
Actual target scheduling, final enabled-device linkage, LVGL display binding,
blit mode and board output remain unverified; hardware **NOT_RUN**.


## LVGL 9.6 display binding

`lv_aic_spi_display_create` owns one budgeted full-frame RGB565 LVGL draw buffer,
a display and an OSAL worker, borrowing the application's configured SPI session.
Source geometry/rotation and worker stack/priority are explicit. The session
retains output geometry, wire format, panel callback and DMA storage ownership.
FULL rendering hands the complete frame to the worker. LVGL's flush-wait callback
consumes completion on the UI thread and calls flush-ready exactly once, without
depending on a timer that cannot run while refresh is waiting. Large packing and
transport waits execute on the worker; refresh waits for source ownership as
required. This first binding uses a single draw buffer, not pipelined double
buffer throughput parity.

Only use the borrowed LVGL display for normal UI operations: do not change buffer,
color format, render mode, rotation, driver data or callbacks. Close outside LVGL
refresh/events, retrying until the worker relinquishes resources. It pauses
refresh, stops admission, consumes any pending result and then deletes display
and draw pixels. Direct LVGL display deletion is observed: it stops admission
but retains the handle/pixels until this close API completes, so a worker cannot
read freed source storage. Session/panel/CMA fault lifetime remains separate.

Validation: **65/65 host PASS**, including real LVGL white RGB565 rendering,
two consecutive refreshes with deferred worker completion, budget/angle/startup
rejection, failed completion during close, and direct display deletion while
source ownership is outstanding. Worker transport is mocked in this display
contract; separate worker/handoff tests cover real host threads. D13x real-header
compile/component partial link PASS. Combined object SHA256:
`7233eb88b9239fbbc85d85f4585b4df8a35c348d7cfd7c6346b9aead501490e9`.
Logs: `output/spi-display-tests.log`, `output/spi-display-target.log`.
Enabled-device final linkage, target scheduling/cache behavior, panel setup/TE,
direct-blit integration, statistics and multi-display board acceptance remain
open. Hardware **NOT_RUN**.


## Double-buffered LVGL drawing

`lv_aic_spi_display_create_buffered(..., buffer_count, ...)` accepts one or two
full RGB565 draw buffers; the existing create API remains a one-buffer wrapper.
The pixel budget is checked against the sum of both stride*height spans before
allocation. Both buffers are released on startup failure or successful close.
Flush now reads LVGL's actual active draw buffer and verifies it is one of the
owned buffers, rather than always submitting the first allocation.

With two buffers LVGL can render the next frame while the worker reads/transmits
the previous one. Flush completion still gates reuse and handoff remains bounded
to one submitted job; the session's DMA buffer is separate and serialized. This
adds overlapping LVGL rendering, not a claim of the SDK's two-tx-buffer GE/DMA
pipeline throughput. No additional in-flight DMA or unbounded frame queue exists.

Validation: **65/65 host PASS**, including six real LVGL black/white alternating
frames, alternating source addresses, unchanged previous-frame pixels until
completion, aggregate budget boundary and invalid buffer-count rejection.
D13x compile/component partial link PASS; combined object SHA256:
`3ff5a960dc55cb8111a260742bde973633f7525c0dead18b031a1d87d7705773`.
Logs: `output/spi-double-tests.log`, `output/spi-double-target.log`.
Target throughput, physical panel output and enabled-device final linkage remain
unverified. Hardware **NOT_RUN**.


## Enabled SPI final firmware linkage

`tools/sdk/build.ps1 -Phase ge2d -WithFonts -WithGif -WithWidgets -WithAicp
-WithPlayer -WithApng -WithBarcode -WithSpi -Jobs 8` now enables the SPI component
and retains public display/panel/session roots in the smoke profile. The static
checker verifies live final-map symbols through display, worker, handoff, panel,
session and SDK QSPI submit/completion functions. Normal application builds do
not force these roots. The option uses existing board driver configuration and
does not initialize a screen, assign new pins or send display traffic on QSPI0
(the board's SPI NAND bus).

Bootloader/application build, enabled SPI symbol gates, general static checks,
image checks and provenance generation **PASS**. Clean component
`355c7ec3582ba2928d6b20bf8e680166cddf2d21`, SDK
`241ce7c3afe14d617133c706fb7a5ef5c75b2495`.
Image SHA256: `01538f83ceb4a70256abce9de9c1ce500f36fc39ba9253e97669fa66fe0f7acf`.
Evidence: SDK `output/lvgl-evidence/ge2d-fonts-gif-widgets-aicp-player-apng-barcode-spi/manifest.json`.
Final ELF contains live SPI display/worker/panel/SDK implementations. An extra
`nm -u` audit finds the same `__data_end__` entry as the preceding non-SPI ELF;
there are no newly unresolved SPI or atomic runtime dependencies. This evidence
supersedes earlier partial-link-only limitations, not physical runtime limits.

Hardware **NOT_RUN**. No panel object is instantiated and no flashing performed.
Target scheduler/cache behavior, concrete panel/power/TE setup, direct-blit mode,
statistics, GE/DMA pipelining and multi-display board acceptance remain open.


## Exclusive direct-blit producer mode

`lv_aic_spi_display_claim_blit` implements the SDK's permanent LVGL-to-blit
ownership transition. Call on the UI owner outside refresh/events. It pauses
refresh and disables invalidation immediately, then returns BUSY until the prior
LVGL frame completion has been consumed. Only after that boundary may direct
frames be submitted. Repeated successful claims are idempotent; no return-to-LVGL
mode is implied. Manual refresh also cannot submit pixels after claim begins.

`lv_aic_spi_display_blit` borrows an explicit immutable CPU-coherent RGB565 frame
through the same worker/session, with per-frame rotation. There is one outstanding
frame; `blit_take` returns its checked result exactly once and releases the CPU
source. Descriptor validation/packing failures are asynchronous results. Closing
waits for outstanding work before releasing source ownership, including external
frames. This interface uses the same producer/UI thread as display lifecycle;
a separate producer thread must not call these APIs concurrently. Standalone
worker usage remains available when no LVGL display needs to be managed.

Validation: **65/65 host PASS**, covering pre-claim rejection, BUSY claim while
LVGL owns a frame, idempotence, disabled manual/invalidation refresh, external
source handoff, backpressure, exactly-once completion and close with a pending
external source. D13x compile/component partial link PASS; combined SHA256:
`fe784c9ce44aeb4299d681bab7a1865ba68cb6d7ae7c2ebbc446805a17776d99`.
Logs: `output/spi-blit-tests.log`, `output/spi-blit-target.log`.
Enabled-firmware symbol gates now include the three blit APIs; the previously
recorded final image predates these additions and is not their link evidence.
Panel configuration/TE/power, statistics, pipeline performance and board output
remain open. Hardware **NOT_RUN**.


## Per-display observed transport statistics

`lv_aic_spi_display_stats` returns a UI-owner snapshot without polling or consuming
completion. Saturating counters distinguish accepted, checked-success completed,
asynchronously failed and immediately rejected flush/blit submissions. Claim,
status-poll and close calls do not count as submissions. Pending/closing/blit
ownership and last completion result are explicit, so an accepted frame is never
reported as already displayed. LVGL and direct-blit modes share the same counters.

Last/max observed latency spans accepted submission to UI completion collection.
It includes worker scheduling, packing, DMA and delayed collection; it is not a
GE or pure SPI measurement. Unsigned tick wrap is supported for a frame shorter
than the 32-bit tick wrap interval. Fine-grained GE/DMA stage timing and physical
FPS acceptance remain pending.

Validation: **65/65 host PASS**, including counts before completion, rejected
pre-claim/busy submissions, once-only completion, fault classification, distinct
17 ms/4 ms observed durations and a completion crossing tick wrap. D13x enabled
compile/partial link PASS; combined SHA256:
`566121c941162ca00b4655afe460d8cb2216a24ffaf2c7ac8181556588c284b0`.
Logs: `output/spi-stats-tests.log`, `output/spi-stats-target.log`.
Final firmware symbol gate includes the new API; the prior final image predates
this increment. Hardware **NOT_RUN**.


## Composed display-to-driver host regression

The new `spi_pipeline_contract` links the actual LVGL renderer, display binding,
OSAL worker, atomic handoff, panel sequence, session, transfer core, packer and
checked SDK bridge in one executable. Only OSAL thread/semaphore/memory/cache
and bottom-level SPI driver calls are modeled. Threads execute via pthreads;
CMA pixels use real mapped low-address storage. It does not replace any component
SPI layer with a success stub.

Six alternating black/white frames run through two LVGL draw buffers, then the
same display is claimed for direct red RGB565 blit. All seven frames execute
explicit RAMWR/D-C setup, 90-degree rotation/resize, wire byte swap, bounded cache
handoff and checked completion. The driver checks pixel bytes and verifies that
in-flight storage stays unchanged across its simulated wait. Normal shutdown
balances CMA and semaphore allocation, with guards outside pixel extent intact.
A second complete run injects pixel-DMA timeout: claim reports FAULT, statistics
record failure, display/worker cleanup finishes, but the session, DMA allocation,
bus claim and panel context remain alive without replay or early free.

**66/66 host PASS**. Logs `output/spi-pipeline-build.log` and
`output/spi-pipeline-tests.log`. This strengthens composed source/thread lifetime
evidence; it does not prove target cache behavior, pin sequencing, QSPI IRQ timing
or panel output. Hardware **NOT_RUN**. Production source is unchanged in this
regression increment; the blit/statistics enabled-firmware refresh remains pending.


## Latest combined firmware refresh

The final firmware has now been rebuilt with direct blit, per-display statistics
and the GE address-span correction. Clean source identities, image SHA256 and
all build/link/image gate results are recorded at the top of
[validation record](validation.md). This supersedes pending final-link notes in
the intervening sections. Hardware remains **NOT_RUN**.


## Explicit panel power/init/TE lifecycle

`lv_aic_spi_panel_set_lifecycle` snapshots optional synchronous checked callbacks
before worker startup/first prepare. First prepare performs power-on, one-time
initialization, TE wait and then frame commands. Later frames repeat TE/frame
commands only. The device bus/status is rechecked between phases. A configured
TE wait receives an explicit 1..60000 ms timeout; its implementation must honor
the bound. No callback means no invented GPIO, delay, screen command or TE source.
Applications may use the checked SPI command writer for initialization, retaining
persistent command buffers if completion becomes uncertain.

Normal close powers off after the referencing session has been successfully
closed, with unmanaged bus clients excluded until panel close finishes. Reentrant
close during callbacks is rejected. Callback failure is sticky: no command replay,
no force power-off after uncertain DMA, and contexts/storage remain alive. Lifecycle
cannot be replaced once prepare starts; geometry rejection before prepare does not
start or power the panel. The application must honor worker/UI callback execution
contexts (prepare on worker; normal close on lifecycle owner).

Validation: **67/67 host PASS**, including power/init before first frame, no repeat
initialization over two frames, per-frame TE, timeout argument validation, refusal
to reconfigure a started panel, reentrant close, and TE failure with no command
submission or retry. D13x compile/partial link PASS; combined SHA256:
`90d14649f5f81cea85669606db604ee2cc809b88fba236f64e451c57f7fc24c8`.
Logs: `output/spi-lifecycle-tests.log`, `output/spi-lifecycle-target.log`.
The final-link gate includes the lifecycle setter, but the prior image predates
this increment. Concrete panel callbacks and physical TE/power acceptance remain
application/board work; hardware **NOT_RUN**.
