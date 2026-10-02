# Arbitrary IMAGE and LAYER rotation

The application-owned backend now shares the unscaled GE ROTATE path between
IMAGE and LAYER. Right-angle scaling continues to use BITBLT. Arbitrary angles
combined with scaling or skew remain software work.

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
