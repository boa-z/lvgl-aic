# YUV conversion and geometry into straight ARGB targets

Published YUV frames now use the same bounded straight-ARGB target staging as
RGB images. GE performs YUV-to-RGB conversion, bounded scale, orthogonal rotation
and clipped tiling into a zeroed ARGB surface. Native LVGL CPU blending applies
global opacity once and computes the target's color and alpha. This is hybrid
rendering, not a claim that GE's direct transparent-target blender is equivalent
to native software.

All twelve existing YUV layouts retain their own plane/stride/CSC configuration:
I420/I422/I444/I400, NV12/NV21, NV16/NV61, YUY2/UYVY and YVYU/VYUY. This adds no
new scaler interval, arbitrary YUV rotation, recolor or mask acceleration.
Premultiplied destination flags are explicitly declined before commands.

## Shared bounds and ownership

The private staging helper is shared by RGB and YUV without SDK or upstream
LVGL changes. `AIC_LVGL_GE2D_ALPHA_BYTES` still defaults to 2 MiB. Only the clipped
visible surface is allocated; dimensions, footprint, allocation and GE address
checks precede DMA. All YUV tile geometry, source/target overlap, plane addresses
and subsampling constraints are first checked against the original target, before
allocation/cache work. Failure returns the existing unsupported result so the
caller can use native fallback. The original task and opacity are unchanged.

The producer lease spans conversion, every strip/tile, intermediate cache
invalidation, final native blend and target-cache clean. If the producer retires
its published frame during the first sync, the planes remain pinned through the
CPU tail. Uncertain DMA retains both planes and surface, leaves the original
target unchanged and blocks further YUV submission until reboot.

Opaque YUV has no per-pixel alpha. Zero-alpha intermediate pixels therefore mark
unwritten tile/rotation gaps, and the CPU tail skips those spans. This preserves
all target bytes in a gap, including hidden RGB in a zero-alpha destination.
Ordinary RGB source alpha semantics keep the prior full native-blend path.

## Development and board evidence

The real YUV executor runs against a descriptor-driven CPU model using actual
low-address staging and target memory. The model uses analytic colored-plane
ramps and BT.709 full-range conversion; it does not decode real codec data or
establish hardware CSC/filter behavior. Existing layout and CSC contracts remain.

576 scenes cover twelve layouts, four orthogonal directions, native/near-unity/
1.5x/2x scale and opacity 64/128/255. All 294,912 visible pixels are compared in
RGBA against independent inverse coordinates and the source-over equation, with
max error 3. Outside-clip bytes remain exact. Source-Y split refresh is
byte-identical; source-X split has maximum observed difference 1 (contract limit
2 for Q16/chroma rounding). The 2x band joined the matrix after Board run 10:
it is the geometry whose rotated inverse source window approaches the engine's
8x8 YUV minimum.
Rotated tile gaps preserve untouched bytes and split refresh matches exactly.

Allocation/address rejection, a late invalid seven-pixel tile, premultiplied
target rejection, producer retirement, six two-strip DMA failures and a later
tile failure all preserve their documented ownership. Cache checks require no
target change before final sync, intermediate invalidation before CPU blending,
and target clean before releasing the frame. The 1,680-scene RGB matrix runs
unchanged against the extracted shared helper.

Forty-eight additional board probes cover all twelve layouts, four orthogonal
placements, native and 2x scale, three global opacities and target alpha
0/64/128/255. Uniform neutral chroma isolates alpha composition from CSC/filter
precision. Native software fill is the oracle; all RGBA channels and exact clip
guards are checked. An immediate target-cache invalidation tests that final CPU
writes were cleaned. These probes are compiled with the existing YUV suite.
Physical pixels, cache/CMA behavior, memory peaks and timing remain **NOT_RUN**.
Combined host **86/86 PASS** (23.07 s) and FreeType/vector/SVG/Lottie-disabled
baseline **79/79 PASS** (20.80 s). Full D13x boot/app, final-link/static, image
and clean-source manifest gates **PASS**. All 29 file hashes and three source
pins were independently verified. Exact identities are in [validation](validation.md).
No SDK/core source changes or physical board execution occurred.

## Board run 9: the v1.1 bitblt alpha lane (2026-10-07)

The `yuv-tile-clean` candidate (image SHA-256
`7ec543b9be84ef07f2e47d2e1f24c50c6f8642faa05f3dd00702a427c00460b5`) was
flashed; the pasted serial slice (from 4.048 s, banner not included) is
archived at `output/lvgl-evidence/board-2026-10-07-yuv-argb/serial.log`,
SHA-256 `3D7B810BC351B1069E7D7B18D07C1A764B7C8F396944B789609825105140CD04`.

Measured in this boot:

- The open item from Board run 8 is closed: `PASS I420 tile rot=90
  pixels=1920 max_error=3 guards=OK`, with no `DIAG` and no `aa=0`
  re-run. Every GE I420 rotation/scale/tile probe stayed green
  (`max_error` 0/1/3).
- All thirty-two colored stripe cells passed (`error<=4`, whole and split
  refresh, opacity 255/128) and the eight packed-format probes at
  `error=0`.
- **New failure: the GE v1.1 bitblt writes the source Y sample into the
  destination alpha lane of the private ARGB surface.** Three `SKIP YUV
  ARGB fmt=32/33/34 ...` lines (I420/I422/I444 decline) preceded `FAIL
  YUV ARGB fmt=3 rot=0 error=39` - probe index 3 is I400, the only layout
  the v1.1 bitblt admits. The uniform Y=100 frame left alpha 100 in the
  staging surface; the CPU tail mixed it with the probe's opa 64 to 25,
  where the native fill oracle keeps 64: 64 - 25 = 39. YUV has no source
  alpha, and `ge_bitblt()` builds its blend command with
  `en_alpha_out_oxff = 0` (`OUTPUT_ALPHA_CTRL(0)` in
  `update_blend_cmd()`), so no engine control passes an opaque alpha
  through on this path.
- The runner stopped at `FAIL YUV ARGB target probes` / `FAIL YUV
  frame/CPU conversion contract`; the eight remaining `SKIP` lines and
  the later mask/color-key, counter/refresh and video-window blocks are
  NOT_EVALUATED in this slice. Panel/touch/capture remain NOT_RUN.

Corrective revision (component `9892cea`): `lv_aic_ge2d_alpha_mark_opaque()`
normalizes every engine-written crop of the staging surface to alpha 0xff.
After each successful submission the executor invalidates that crop's DMA,
marks it opaque and cleans the lines back for the CPU tail, so no CPU read
depends on the engine's alpha lane. Pixels no command wrote (tile/rotation
gaps) keep their zeroed alpha and stay excluded from the blend; the mask
tail now multiplies an opaque lane and the color-key tail clears one. This
is a component-owned normalization; SDK behavior is untouched.

Host `ge2d_yuv_contract` now models the board lane (`board_color()`
hands the Y byte through in the mock's ARGB output) and counts the staging
invalidate/clean pairs: GE profile **73/73 PASS**, the no-GE2D baseline
**35/35 PASS**. Disabling the normalization turns the model red
(`ARGB YUV f=0 angle=0 zoom=0 opa=64 xy=54,44 c=3 error=54`), so the
contract covers this defect.

Expected next log: four `PASS YUV ARGB fmt=3 rot=0/90/180/270 ...` lines,
eight `SKIP YUV ARGB fmt=...` lines for the probe list's tail, then the
later YUV and GE2D blocks. A remaining mismatch now prints `xy`, `got`
and `want` on the FAIL line.

## Board run 10 (2026-10-07): the rotated 2x strip and the 8x8 YUV minimum

The `yuv-argb-alpha` candidate (image SHA-256
`7c2cd6be03596704ef481cfd14942bfb23db7584723ca9975e2b20eac62255bc`) was
flashed; the pasted serial slice (10.071-10.870 s) is archived at
`output/lvgl-evidence/board-2026-10-07-yuv-argb-alpha/serial.log`, SHA-256
`3D4F16686F3EF0D860904B203AE6C05669A7FE85663F1E89920024BA7DD9841C`.

- Run 9's alpha item is closed: `PASS YUV ARGB fmt=3 rot=0 opa=64 error=1`
  (was `error=39` before the `9892cea` normalization). Every earlier block
  of the slice stayed green (four rotations, seven scales, both tiles,
  thirty-two stripes, eight packed probes, three SKIPs).
- The block then stopped without another line. The probe's 90/270 clips
  were 8 wide; at 2x the inverse-mapped source window is half the visible
  width plus filter taps, so the crop was 10x6 and `submit()` declined
  (return 0) at the `w < 8 || h < 8` guard - the same minimum the SDK
  enforces in `check_blit()` ("invalid src size, the min size of yuv is
  8x8"). The executor is correct: the decline precedes every cache
  handoff, allocation and write, so nothing was corrupted; the probe
  treated `!= 1` as a silent failure.
- Probe revision: the 90/270 clips are 16 wide (`{36,24,51,39}`, window
  10x10) so the rotated strips reach the engine, and every silent exit
  now prints its own diagnostic (executor/decline `rc=`, clip-outside
  `xy`/`got`/`want`, setup and lease checks).

Host contracts pin both halves of the geometry decision:
`ge2d_yuv_v11_contract` asserts the 8-wide strip declines with no
allocation, submission or cache activity (ARGB target, mirroring the
probe) and the 16-wide strip submits with source crop `{12,2,10,10}`,
phase `{0,32768}` and destination crop `{36,24,16,16}`;
`ge2d_yuv_contract` joined ratio 512 to the staged-ARGB scene matrix
(**576 scenes, 294912 analytic RGBA pixels, max error 3**) so the 2x
rotated strip runs through the full pixel oracle.

Expected next log: four `PASS YUV ARGB fmt=3 rot=0/90/180/270 ...` lines,
eight `SKIP YUV ARGB fmt=...` lines, then the later blocks; any failure
now names its reason (executor `rc=`, outside write `xy`/`got`/`want`).

## Board run 11 (2026-10-07): rotation accepted

The `yuv-argb-rot90` candidate (image SHA-256
`2DC19002DB18380A6477A6608F48444A4DFE96CAB581E355F39326666577F220`) was
flashed; the pasted serial slice (9.378-12.908 s) is archived at
`output/lvgl-evidence/board-2026-10-07-yuv-argb-rot90/serial.log`, SHA-256
`BAB4316555D580F0E3878597816C17BB60DA104A5B65D072740112CA8B039121`.

All four rotated ARGB probes pass - `PASS YUV ARGB fmt=3 rot=0/90/180/270
opa=64/128/255/128 error=1/1/0/1` - followed by the eleven v1.1 `SKIP`
lines. The sliced GE2D scale test then exposed a stale probe expectation,
recorded in [validation](validation.md); the YUV ARGB block itself is
closed.
