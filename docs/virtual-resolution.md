# Virtual resolution display

`AIC_LVGL_VIRTUAL_RES` (+ `AIC_LVGL_VIRTUAL_HRES/VRES`, default 1024x600)
lets LVGL render a fixed design resolution on a panel of another size. It
exists so the SDK demos, all designed for 1024x600, run unmodified on the
800x480 D50T panel instead of each demo carrying its own scaling code.
`build.ps1 -VirtualRes` enables it; `-WithMeter/-WithDashboard` imply it.

## How it works (`port/lv_aic_display.c`)

- LVGL's display is created at the virtual size and renders, in DIRECT mode,
  into one CMA buffer of that size (same structure as display rotation; the
  two are mutually exclusive).
- On the last flush of a refresh, `lv_draw_aic_ge2d_display_scale()` scales
  the whole virtual frame once into the fit rectangle of the scanout plane,
  then pans. Scaling the full frame every present keeps the result seam-free
  (no partial filtered updates).
- Fit (`port/lv_aic_display_fit.c`, pure integer math): uniform, centered,
  rounded. 1024x600 on 800x480 = 800x469 at (0,5); the borders are cleared
  once on every scanout plane at init.
- GE2D declines (format, size, the 1/16..16 scaler range, or the near-unity
  step interval 59392 < dx < 65536 that needs the SDK adapter's stripe split)
  fall back to a CPU nearest-neighbour scale. A GE fault quarantines as for
  rotation.
- Touch: the controller range is scaled to the panel, then mapped back
  through the fit (`lv_aic_display_panel_to_logical()`); touches on the
  borders clamp to the nearest edge.
- `lv_aic_display_fps()` reports frames presented in the last full second
  (also what the official demos' `fbdev_draw_fps()` shows).
- Snapshots and the CAN/UART capture read the scanout plane, i.e. what the
  panel shows (800x480 with borders).

Cost: one virtual-size buffer (1024x600 RGB888 = 1.8 MB CMA) and one
full-frame GE scale per presented frame. Text is resampled (0.78x), so
product UIs should still be designed for the panel natively.

## Host evidence

- `display_fit_contract`: fit for 1024x600, 800x480 (identity), 480x272 (the
  SDK's other demo set), a pillarbox case and invalid input; inverse map at
  corners, the design center and both borders; monotonic over a full row.
- `ge2d_display_contract`: the scale descriptor (no blending, crop on the
  destination only, full source), each submit/emit/sync failure quarantines,
  and five decline paths submit nothing.
- Full host suite 75/75 with the official-demo option.

## Board evidence (D50T-2-Lite, 2026-10-07)

Evidence: SDK `output/lvgl-evidence/board-2026-10-07-vres/` and
`.../board-2026-10-07-official/` (CAN-captured PNGs, serial logs).

- Smoke page laid out for 1024x600, scaled into 800x469 with 5/6 px borders;
  boot self-tests unchanged (no FAIL lines).
- Frame rate, same meter workload, against the earlier per-demo scaling port:

| Variant | Left half (frame swap) | Right half (rotated needle) |
|---------|------------------------|-----------------------------|
| per-demo scaling (retired adapted meter) | 15-17 | ~14 (9-19) |
| virtual resolution, adapted meter at zoom 1.0 | 16-17 | 9-10 |
| virtual resolution, official meter (clean rodata) | 23-24 | 23-24 sampled |

- Official dashboard: 43-47 FPS.
