# Arbitrary IMAGE and LAYER rotation

The application-owned backend now shares the unscaled GE ROTATE path between
IMAGE and LAYER. Right-angle scaling continues to use BITBLT. Arbitrary angles
combined with scaling use the subsequent [bounded multipass path](ge-multipass-stage.md);
skew remains software work.

Combined orthogonal transforms keep the destination pivot fixed and inverse
rotate before dividing by source-axis scale. GE scales before rotating, so
Q16 steps and phases stay in source X/Y order even at 90/270 degrees; the
[RGB stripe planner](ge-stripes-stage.md) uses the pre-rotation output width. This fixes nonzero-pivot
drift and swapped anisotropic scales. Host tests use upstream LVGL forward
transforms and a floating inverse matrix across all four rotations, three
anisotropic scale pairs and fractional clips, plus a real blit descriptor
assertion. Hardware pixel acceptance remains pending.

The native LVGL degree table is interpolated at 0.1-degree resolution before
conversion from Q15 to Q12. This corrects the previous whole-degree rounding.
The SDK normal-mode HAL accepts source and destination rectangles only within
4..4096 pixels on each axis. The executor checks those limits before cache or
hardware submission, preserving software fallback for narrow clips.

Host evidence covers the real decoder/executor, all four RGB source formats,
nonzero layer origins, pivot and clip coordinates, alpha, source-address
rejection, empty child layers and rotate/emit/sync failures. A floating-point
oracle checks every tenth of a degree with error below 2 Q12 units.

Hardware calls are mocked. Rotated edge pixels, antialiasing, DMA/cache behavior
and panel output remain pending consolidated board verification. No global
allocator, SDK core or upstream LVGL source is changed.

## Wide-coordinate crop arithmetic (2026-10-04)

The orthogonal rotation-plus-scale crop helper now widens each destination
coordinate before multiplying by 256. Previously the expression overflowed a
32-bit signed integer before assignment to int64_t, corrupting inverse mapping
for large image-relative translations/pivots. Both rotated crop helpers also
reject reversed destination rectangles instead of silently reordering corners.
Normal source/buffer bounds, minimum crop sizes and scaler limits are retained.
This is arithmetic correction, not expanded end-to-end LVGL geometry support.

A regression failed before the fix on valid inverse mapping with remote pivots.
An independent double forward matrix now checks all four right-angle directions,
both pivot signs and per-axis 2x/3x scaling. Separate cases reject an out-of-image
rectangle that previously wrapped into the source and a reversed rectangle.
Host regression **70/70 PASS**; strict D13x helper compilation **PASS**.
SDK object `output/ge-wide-crop-rotate.o` SHA256:
`91928d8df04725fdfa40af6be6414f3c8f0dc522bb94aa37381b6eaaef12c8a0`.
Logs: `output/ge-wide-crop-before.log`, `output/ge-wide-crop-build.log`,
`output/ge-wide-crop-tests.log`, `output/ge-wide-crop-target.log`.
The current packaged demo firmware predates this correction. Full-firmware
refresh for this increment and physical hardware execution remain **NOT_RUN**.

Full-firmware follow-up: the clean independent music profile now includes the
wide-coordinate crop correction and passes boot/app, final-link/static, image
and manifest gates. See [validation.md](validation.md) for source identities
and hashes. This supersedes the pending packaging note above; board NOT_RUN.
