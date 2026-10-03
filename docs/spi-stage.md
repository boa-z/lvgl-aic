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
