# Near-unity RGB GE scaler stripes

The vendor LVGL adapter (`lv_draw_ge2d_img_scale.c`, `calculate_split_params`)
uses a special split when `59392 < dx_16 < 65536` and source-X output width is
at least 32. Previously our RGB executor declined that whole request. The
application-owned executor now emits balanced commands of 16..31 output pixels
along the scaler-X axis. This is explicit component planning; `mpp_ge_bitblt`
does not perform the vendor LVGL adapter's split itself.

## Geometry and command ownership

The number of commands is `ceil(width / 31)`; balancing avoids a tiny final
command. Each strip retains the original Q16 step and absolute sampling phase,
advances the source crop by the integer part, and carries the fractional
remainder. Two filter taps of padding are retained where source storage allows.
Destination placement follows 0/90/180/270-degree orientation. Source-X maps
to destination Y for 90/270 degrees, and reverses for 180/270 degrees. Allocation
addresses, stride, alpha and vertical phase stay unchanged.

All descriptors are checked before the first cache operation/submission in
ordinary RGB tasks. Tiling preflights all cells before destination writes.
Multipass transforms check the copy and every scale strip before even the
preparation copy; they share their existing bounded scratch allocations.
Unsupported geometry uses the existing whole-task fallback. Each command is
followed by emit/sync. Uncertain completion stops the sequence, retains source,
decoder and any transform scratch, and prevents software replay and later GE
requests until reboot. This adds no new scratch allocation to ordinary RGB.

The special path is bounded to source-X output <=4096, one scaler channel,
RGB565/RGB888/XRGB8888/ARGB8888, a fractional initial phase in [0,65535] and
orthogonal rotation flags. Other intervals retain their original descriptor.
Existing source sampling, dimensions, physical address and task guards still
apply. Fractional outer samples outside the source can still require software.
YUV/chroma and the separate SPI conversion planner retain their split-risk
fallback; this stage does not relax their alignment or phase contracts.

## Verification

- 864 independent descriptor plans span output widths 32..4096, six Q16 steps,
  four phases and all four rotations. Every output sample has exactly one
  command with the original inverse coordinate; the input descriptor is intact.
- All 399 submit/emit/sync failure positions in a 133-strip plan stop immediately.
  A late source shortage declines before any submission.
- The real image executor runs 36 ordinary stripe scenes against a CPU GE model:
  90,528 independent ramp pixels, straight/premultiplied alpha, clipped pivots,
  exact full/partial refresh equality, and failures after the first target strip.
- The multipass matrix now covers 432 IMAGE/LAYER scenes and 138,576 independent
  interior pixels. Both regular and near-unity tiled transforms share one
  preparation. All 12 copy/two-scale/rotate failure points retain both scratch
  allocations and the decoder.
- Combined host **82/82 PASS**; vector/SVG/Lottie-disabled baseline **77/77 PASS**.

The CPU model verifies descriptor geometry, composition and ownership, not
hardware filtering or timing. Forty additional offscreen board probes cover
32 ordinary stripe cases and eight striped multipass cases, including both
premultiplied encodings. They require GE outcomes, independent gradient pixels
and untouched clip guards. Physical execution remains **NOT_RUN**. Target build
boot/app compilation, live preflight/runner linkage, image and clean-source
manifest gates all **PASS**. Exact identities are in [validation](validation.md).
