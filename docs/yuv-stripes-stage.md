# YUV stripe scaling and chroma-safe clipping

The direct YUV IMAGE path now shares the RGB/SPI near-unity stripe planner.
All twelve supported layouts are included: I400, I444, I420, NV12/NV21, I422,
NV16/NV61 and YUYV/YVYU/UYVY/VYUY. Orthogonal transforms and tiled publication
retain the existing frame lease, CSC flags, plane addresses and strides.

## Sampling and clipping

Luma and chroma use the same output grid. Chroma's input coordinates and steps
are divided on the source's subsampled axes, even when rotation maps source X
to destination Y. Each strip advances its source crop to an aligned luma/chroma
origin and carries the remaining absolute Q16 coordinate into each channel's
initial phase. Subsampled luma phase can retain one whole pixel; chroma retains
the corresponding half pixel. Source width and chroma channel width are updated
together; vertical phase and channel height remain unchanged by strip planning.

Scaled odd crop origins now back up to a full chroma sample and compensate in
the phase rather than rejecting the task. Native-size unscaled odd crops keep
their previous conservative fallback. Before strip planning, both source axes
retain adjacent filter taps for luma AND chroma, bounded by actual frame storage.
This also corrects the old orthogonal partial-refresh path, where the cropped
chroma input could clamp at an intermediate clip boundary. Interior strips have
the required taps; the final strip preserves the original image's edge clamp.

The planner verifies channel count, subsampling relationships, aligned crop
sizes, phase and step consistency before commands. Balanced 16..31-pixel strips
preserve the existing 8-pixel YUV minimum and <=4096 scaler-X output bound.
All tiles are checked before any cache operation or target write. No new buffer
allocation or plane conversion is introduced. On any submit/emit/sync failure,
the existing source lease remains quarantined; no software replay or later GE
work is allowed. Other effects/geometry retain the existing fallback policy.

## Evidence

- Combined host **83/83 PASS**; vector/SVG/Lottie-disabled baseline **78/78 PASS**.
- 7,616 independent YUV descriptor plans span twelve layouts, all orthogonal
  rotations, widths 32..4096, near-unity Q16 steps and fractional/whole-pixel
  phases. Both channels retain their original inverse coordinate; all output
  samples are covered once. CSC/plane storage is unchanged and inconsistent
  channel phase is rejected before submission.
- Real YUV executor with a virtual colored-plane CPU model: 48 scenes / 24,576
  independent inverse-mapped pixels. Split refresh on source Y is byte-identical;
  source-X splits and odd Y crop origins differ by at most one channel value
  from the full render, accounting for existing per-clip Q16 rounding.
- Twelve-layout tiled stripe dispatch, late invalid-cell preflight, owner
  retirement during the first strip and all six two-strip DMA failure points
  preserve the existing cache/lease/fault contracts.
- Thirty-two new I420 board cases cover two near-unity ratios, four rotations,
  two opacities and whole/split refresh. Colored luma/chroma ramps and an
  independent BT.601 oracle check 512 pixels each plus untouched clip guards,
  with four RGB values of tolerance. No physical execution is claimed.

Host modeling verifies descriptor geometry and lifetime, not real GE filtering,
CSC precision or performance. Target identities are recorded in
[validation](validation.md); board execution remains **NOT_RUN**.
