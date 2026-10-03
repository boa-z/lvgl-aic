# Whole-display GE rotation

The display flush path can now submit 90/180/270 degree whole-frame copies
through the component's existing synchronous GE device. This is independent
of IMAGE/LAYER rotation and requires the configured display rotation buffer.
No SDK display driver or upstream LVGL source is modified.

在 GE 可用且源/目标格式、几何、容量和物理地址符合要求时使用硬件。
显示旋转 90/270 对应 MPP 270/90，沿用 SDK 的方向约定。
支持 RGB565、RGB888、XRGB8888、ARGB8888；整帧复制不启用 alpha 混合。

Cache preparation cleans the source and cleans/invalidates the destination
before submission. After successful sync, the hardware-written destination
is presented without a CPU clean overwriting it. Unsupported inputs decline
before submission and use the existing software path. A bitblt/emit/sync
failure suppresses presentation and invalidates snapshot availability for
that flush; it does not retry into a potentially active DMA destination.

Buffer validation rejects overlapping ranges, address overflow, inaccessible
memory, insufficient capacity/stride, mismatched formats and dimensions.
The initial geometry bound is 4096 on each axis.

Host validation: 12/12 tests passed, including three rotations across four
formats, padded strides, cache/submission ordering, failures at each engine
step, unavailable engine, overlap and address/capacity rejection.
These use mocked hardware and do not prove actual pixels, cache coherency,
PAN/VSync interaction or board throughput.

Remaining: target build with rotation enabled, flush/presentation failure
contract coverage and physical portrait/landscape checks. SPI and multiple
displays are still outside this implementation.
