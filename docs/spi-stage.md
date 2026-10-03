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
`output/lvgl-spi-frame.o` defines `lv_aic_spi_pack_rgb565`.

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
