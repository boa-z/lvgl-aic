# Bounded IMAGE recolor with color-key preparation

IMAGE and LAYER draws may now combine an A8/L8 bitmap mask, an LVGL inclusive
color key and the application-owned CPU recolor extension before GE copy,
scale, rotation and tile work. The order is deliberate: the mask first
multiplies source alpha, the key then clears matching pixels to transparent,
and recolor changes only the surviving source colors. This matches LVGL's
source semantics and keeps the decoder and mask buffers immutable.

The scheduler and direct executor accept the combination. A key that could
otherwise use the GE single-value comparator is still normalized when recolor
is present, because the comparator must observe the original source color
before recolor. The two private CMA buffers remain bounded by the existing
`AIC_LVGL_GE2D_COLORKEY_BYTES` and `AIC_LVGL_GE2D_RECOLOR_BYTES` limits. Any
format, footprint, address or allocation rejection returns to native software;
an uncertain GE submission retains both buffers under the existing quarantine
rule.

This is an application-owned composition extension. The ArtInChip SDK GE
reference rejects recolor, and its single-value key comparator does not define
the full LVGL range semantics. Hardware key/recolor arithmetic is therefore
not claimed by this stage.

## Host contract

The multipass contract checks the mask → key → recolor order, that a keyed
pixel remains transparent, that a nonmatching pixel retains mask alpha and
receives the requested recolor, and that source/mask bytes remain unchanged.
The executor layer contract also confirms that the combined descriptor is
accepted and reaches GE while rounded clips and non-normal blend modes still
fall back before allocation. Full profile counts and exact build pins are
recorded in `validation.md`.
