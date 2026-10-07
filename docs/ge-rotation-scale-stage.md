# GE orthogonal rotation and scale bounds

This stage closes two gaps in the SDK-shaped GE IMAGE path. Inverse mapping
for 0/90/180/270 degree rotation now floors every signed Q16 source-axis term
before adding the pivot. C integer division truncates negative values toward
zero; using that result advances a crop phase by one Q16 unit for clipped
images whose destination lies before the pivot.

The same preflight is now applied to every GE bitblt and rotated scale crop:
source and destination rectangles must be between 4 and 4096 pixels on each
axis. Requests outside that range return before cache maintenance, allocation
or GE submission and continue through LVGL software rendering. This covers
unscaled small images and narrow clips as well as scaled and rotated tasks.

The host GE scale contract uses an independent signed-floor oracle across all
four orthogonal angles, non-integral 384/512 scales, negative and positive
axis offsets, and source/destination boundary cases. It also drives a real
3-pixel source through the public executor and verifies a software outcome with
no submission. Physical scaler phase, cache and panel acceptance remain
**NOT_RUN**.
