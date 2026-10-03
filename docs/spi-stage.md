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

Still missing: exclusive SPI device/session ownership, bounded DMA transmit
allocation, completion/error handling, panel initialization adapters, LVGL 9.6
flush/claim integration, per-panel statistics and multi-display acceptance.
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
