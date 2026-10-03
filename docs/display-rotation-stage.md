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

Host validation: 13/13 tests passed, including three rotations across four
formats, padded strides, cache/submission ordering, failures at each engine
step, unavailable engine, overlap and address/capacity rejection.
These use mocked hardware and do not prove actual pixels, cache coherency,
PAN/VSync interaction or board throughput.

## Target build evidence

Application-owned Kconfig `AIC_LVGL_DISPLAY_ROTATION` selects 0/1/2/3
quarter turns, independently of the SDK legacy LVGL menu.
Build with `tools/sdk/build.ps1 -Phase ge2d -WithFonts -WithGif -WithWidgets -Rotation 90`.
The script also accepts 180 and 270, stores separate evidence directories
and restores the smoke defconfig after success or failure.

90-degree cross-build passed at component `2a80f72`, SDK `305cff9a`:
boot/app build, static configuration checks, image checks and manifest PASS.
The live map contains `lv_draw_aic_ge2d_display_rotate`.
Evidence: SDK `output/lvgl-evidence/ge2d-fonts-gif-widgets-rotate90`.
Image SHA256:
`0882c5c520600c13b13ea9560e086e84d56aba3bd60bfdcbf8e869a85e45f292`.
All three source repositories were clean in that manifest. Board NOT_RUN.

The real display init/flush/deinit path is also exercised with mocked BSP and
GE calls: hardware failure suppresses PAN/VSync and index advancement,
software fallback presents, snapshot availability follows GE/PAN/VSync
failures and recovery, and CMA allocation/free balances at teardown.

Remaining: 180/270 target configuration coverage and physical portrait/landscape checks. SPI and multiple
displays are still outside this implementation.


## GE rotation DMA fault quarantine

The whole-screen rotation path previously returned hardware errors without
latching shared GE quarantine, released LVGL flush ownership and allowed a later
software retry/deinit. An emit/sync failure does not prove DMA has stopped, so
that could reuse or free a source still being read by GE. Rotation now quarantines
the shared client on any bitblt/emit/sync failure and reports existing quarantine
as failure rather than a software-fallback decline.

Framebuffer flush now marks the display quarantined, invalidates snapshot state
and deliberately withholds flush-ready. It does not present, advance buffers,
retry in software or release the LVGL source. Deinit retains the display, rotation
allocation and framebuffer handle; new display initialization is rejected while
shared GE remains faulted. Reboot is the recovery boundary. A refresh waiting on
the failed flush can remain blocked; this is deliberate resource retention, not a
successful presentation or recoverable timeout. Pre-submission geometry decline
still permits software fallback, and ordinary framebuffer ioctl error handling
is unchanged.

Validation: **68/68 host PASS**, injecting all three hardware failure stages,
checking shared quarantine/no subsequent GE calls, and checking persistent flush,
no software replay/snapshot, retained CMA and rejected reinitialization. Existing
healthy deinit, software fallback and framebuffer failure tests still pass.
Strict real-header D13x compilation PASS for both modified modules:
- `output/quarantine-lv_draw_aic_ge2d_display.o`: SHA256
  `0c7d7d15b6d6a4a0655385c6add3357697db4e3d41b8a3f743de0ad59af01ecf`.
- `output/quarantine-lv_aic_display.o`: SHA256
  `c5e7ef9178f8e24bdf2e45b0c6174d37d06a9b158579fb0f37ad0a965c767778`.

Objects are under SDK output. Host logs are component
`output/ge-rotate-quarantine-build.log` and `output/ge-rotate-quarantine-tests.log`.
Full rotated firmware refresh and hardware fault acceptance remain pending.
Hardware **NOT_RUN**. This fix is also a prerequisite for any future SPI GE
conversion, which must additionally preserve its source lifetime on GE failure.
