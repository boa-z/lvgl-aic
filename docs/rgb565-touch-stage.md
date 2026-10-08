# RGB565 framebuffer and gt911 touch (board, 2026-10-08)

Question: does the port work on the D50T-2-Lite with an RGB565 framebuffer and
the gt911 touch controller, the combination the product was assumed to use?

## What the product actually uses

Not RGB565. `d13x_d50t-2-lite_rt-thread_forklift-meter-platform*_defconfig`
select `CONFIG_AICFB_RGB888=y`, and the product's `sim/lv_conf.h` defaults to
`LV_COLOR_FORMAT_XRGB8888`. Its `CONFIG_LV_COLOR_DEPTH=16` is a leftover
Kconfig symbol of the SDK's LVGL 9.1 package that the 9.6 port ignores: the
display format follows the framebuffer (`AICFB_*`). The product differs from
the smoke profiles in that it builds with GE2D and MPP decoding disabled and
sets `AIC_TOUCH_X/Y_COORDINATE_RANGE=800/480` (smoke: the SDK default
1024x600). RGB565 was tested anyway because the port supports it
(`lv_aic_format_to_lvgl`) and it had never run on the board.

## Build switches added

`build.ps1 -FbFormat rgb565` selects `CONFIG_AICFB_RGB565` (evidence suffix
`-fb565`); `-TouchRange 800x480` sets the touch range (`-tr800x480`). The
defconfig overlay now removes the original line of a `# X is not set` setting;
before, an override such as `# CONFIG_AICFB_RGB888 is not set` left both
symbols in the effective defconfig.

## RGB565 against RGB888 (GE2D profile, official demos, CAN capture/OTA)

Two images from identical flags, one per framebuffer format
(RGB888 `0A9041D2...C4946F`, RGB565 `5C3443F7...8835BC`), each booted and its
full serial log archived:

| | RGB888 | RGB565 |
|---|---|---|
| PASS / FAIL / SKIP / `E/lvgl` | 380 / 0 / 11 / 0 | 380 / 0 / 11 / 0 |
| GE2D counters | `fill=23 image=19 layer=1`, `errors=0` | same |
| Probe lines after removing timestamps | 599 | 599; one line differs: the MPP heap counter (`before=119160` vs `119152`, `peak=174740` vs `179692`) |
| CAN capture | `RGB888` | `RGB565`; colors correct on the smoke page, meter and dashboard |
| meter / dashboard FPS (virtual 1024x600) | 24 / 48 | 24 / 48 |

Most probes use private off-screen buffers, so identical results are expected
for them. The framebuffer-dependent paths are the display present, the GE2D
scale into the panel and the capture; those were judged from the pixels.
The `/sdcard` mount error in both logs is the absent SD card.

## gt911 touch

New tools, both in `tests/manual`: shell `lv_aic_touch [watch <s>]` (indev
counters; during a watch, the mean coordinate of each touch and the extent),
and the orange **Touch** button in the smoke navigation bar, which opens a
full-screen page with five target rings (four corners inset 48 px, and the
center), a live crosshair, the error of each touch against its ring, and a fit
`delivered = scale * target + offset` per axis with the range to configure
(`range_now * scale`). Corner touches that land within 100 px of a ring count.

RGB565, official demos (virtual 1024x600 display, so the page ran in 1024x600
and also exercised the inverse touch mapping), touch range 800x480 as in the
product:

| ring | error (x, y) |
|---|---|
| top-left | -2, -4 |
| top-right | -10, +12 |
| bottom-right | +2, -11 |
| bottom-left | -7, 0 |
| center | -11, -16 |

Fit: scale x=1.001, y=0.980, offset -5/+5; the page reports "OK, keep 800x480".
The image (`DC287F2A...BE2FF7`) is RGB565 with `-TouchRange 800x480`. An
earlier run with the default range 1024x600 and an 800x480 logical display
(corner touches at x 19..766, y 13..465 of 800x480; no axis swap or mirror)
also mapped correctly, so on this board the product's 800x480 range is right;
both ranges give correct coordinates, i.e. the controller's output follows the
configured range (the gt911 driver has a set-range path; it was not traced
further). Tapping "Test touch" changes its label to "button event received":
touch to LVGL hit test to click works end to end.

Not covered: touch accuracy beyond about 12 panel px (finger placement
dominates), multi-touch, and the product application itself (see below).

## Not done

- The forklift product was not built with GE2D/MPP enabled: it lives in a
  separate checkout and building it there modifies that checkout.
- GE2D 565 color-key remains an open policy decision
  ([GE color key](ge-colorkey-stage.md)).

## Bench note

The board's CAN transmit failed once (`hal_can_send_frame ... hardware write
refuse`, `rx=0`) after the person handled the board; the CAN cable was
reseated and everything worked again. An endpoint reporting `rx=0 tx=0`
after boot means the link is down, not the firmware.
