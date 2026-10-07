# Tiled IMAGE bitmap masks

The bounded native IMAGE mask extension now covers repeated `tile` draws. Each
decoded RGB/ARGB source is converted to one private ARGB staging image, the
A8/L8 mask is applied once, and LVGL's tile executor reuses that copy for every
source-sized cell. The original image and mask remain unchanged. This extends
the SDK GE path, which rejects bitmap masks; it does not claim a hardware mask
operator.

When `image_area` is omitted, the first tile origin is derived from the task
area and reduced to one source-sized cell for mask alignment. Every cell keeps
the existing clipping, source address, destination address and GE stripe
preflight rules. The decoder remains open through validation and submission,
and an uncertain GE failure retains the staged copy and decoder lifetime under
the existing quarantine policy.

The stage is bounded by `AIC_LVGL_GE2D_MASK_BYTES` (1 MiB by default), the
existing 4096 pixel source limits and the normal GE destination constraints.
Allocation, malformed mask, unsupported YUV source, narrow/unsafe geometry and
address failures fall back to native software before any GE write. YUV masks
remain software because they use the separate immutable frame path.

## Host contract

`lvgl_aic_ge2d_scale_contract` now exercises a source-sized A8 mask over a
24x16 tiled region (three columns by two rows). It checks the default tile
origin, 128-level mask coverage, six GE submissions, source immutability and
staging release after the two-pass preflight. The complete GE contract passes;
physical mask pixels, cache coherency and panel output remain **NOT_RUN**.

The existing 46 manual mask probes remain unchanged and continue to cover
RGB565/RGB888/XRGB8888/ARGB8888/premultiplied sources, opaque and ARGB targets,
and the original LAYER mask matrix. Tiled IMAGE board probes are queued for the
next unified board session.
