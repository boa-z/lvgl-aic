# YUV frame and conversion contracts

The application-owned frame API in include/lv_aic_yuv.h now describes borrowed
CPU-addressable planes with independent strides and capacities. It supports
I420, I422, I444, I400, NV12, NV21, YUY2, UYVY, NV16, NV61, YVYU and VYUY, with explicit BT.601/BT.709
limited/full range. The producer owns lifetime and cache synchronization;
these descriptors must not be passed to LVGL as if they were decoded pixels.

LVGL 9.6 has no NV16/NV61/YVYU/VYUY color enums. The corresponding
LV_AIC_YUV_* constants are frame-only
32-bit tags accepted by this API, its CPU converter and native GE path. They
must never be stored in lv_image_header_t.cf. Publication still reports RAW
source/RGB888 decoded metadata; ordinary LVGL/MPP image format mappings remain
unchanged. Twelve-format host coverage includes row-varying 4:2:2 chroma, all four
matrices, GE transforms/tiling and 20 actual widget frame replacements per
NV16/NV61 format without stale pixels or leaked producer references.

CPU conversion to native RGB888 validates every span and output overlap before
writing. Odd dimensions use rounded-up chroma storage. Conversion performs no
allocation/cache insertion, does not mutate source planes and preserves output
padding. It supplies the renderer's fallback/reference path.

## Immutable LVGL image publication

include/lv_aic_yuv_image.h provides an optional decoder independent of MPP/GE.
Call lv_aic_yuv_image_decoder_init after lv_init, create a frame image with
mandatory retain/release callbacks, and pass lv_aic_yuv_image_source(image)
to lv_image_set_src. Creation copies metadata and retains the producer once.
Producer pixels must remain immutable and CPU-coherent until release.

Each decoder open owns a separate RGB888 conversion. No generic image-cache
entry retains the producer or reuses a previous video frame. Detach images from
widgets and complete queued draws before destroying the owner reference.
Already-open readers retain their snapshot and the producer; release occurs
only after the final reader closes. Retired descriptors cannot be reopened,
and decoder deinit refuses while any image or reader remains. All calls run
on the LVGL owner thread; producer callbacks must not re-enter LVGL. Destroy
images, close readers and deinit this decoder before lv_deinit.

Host integration: 20/20 PASS. New coverage includes allocation/retain failures,
two simultaneous readers, owner retirement, deferred release, rejected decoder
teardown, exact conversion pixels, real lv_image software rendering, 20 frame
replacement cycles without stale pixels and repeated LVGL init/deinit.
The board CPU probe also exercises decoder pixels and deferred producer release.
The decoder's normal open path produces RGB. A GE task can now acquire a
native frame lease before opening the decoder and submit its original planes.

## Direct GE path

Native-size IMAGE tasks support the eight frame layouts and 0/90/180/270
rotation with aligned clipping and global alpha. The path preserves explicit
CSC flags and independent plane addresses, cleans source planes, prepares the
destination cache and waits for GE completion before releasing its frame lease.
Source/destination aliasing, inaccessible planes, geometry below the SDK's
8-pixel minimum, odd subsampled crops and unsupported effects decline before
any cache operation or submission. Bounded scaling with orthogonal rotation configures
one luma channel and, except I400, one chroma channel using SDK Q16 phase rules.
Subsampled axes round the luma step/phase down to even before halving for UV.
Filter footprints can expand to complete chroma samples within the source;
odd crop origins, out-of-source filter samples and the known GE split-risk
interval decline before submission. Transformed tiles and
arbitrary rotations retain the RGB conversion/rendering fallback.

Host scaling coverage includes all eight layouts at 0.5x, 1.5x and 2x,
fractional-phase clipping, channel dimensions, source bounds and nonzero-pivot
anisotropic scaling with 90/180/270 rotation. These are descriptor/cache/lifetime
mocks, not GE filter pixel validation. Board probes now cover 0.5x/1.5x/2x,
fractional clipping and anisotropic scaling at all three nonzero orthogonal
rotations. Neutral chroma and an analytic BT.601 luma ramp form an independent
pixel oracle (tolerance 3); every pixel outside the clip must retain its sentinel.
All scaled board probes remain NOT_RUN.

Native-size YUV tiling holds one source lease across two passes: every visible
tile is checked before the first cache operation or write, then the same tile
geometry is submitted synchronously. An unsupported final edge declines the
whole task. Failure after any submitted tile retains the lease and stops
rendering without software replay. The repeat anchor follows image_area, with
offscreen rows/columns skipped arithmetically. Host tests cover all eight
formats, aligned partial edges, invalid final edges, a distant anchor, owner
retirement during the first tile and a second-tile hardware failure. A new
2x2 I420 board probe checks 1680 clipped pixels and every outside byte against
an analytic neutral-chroma ramp; board execution is NOT_RUN.

On a submission/emit/sync failure, software replay is forbidden. One source
lease is quarantined and further YUV submissions fail. The dispatcher keeps
the task/destination layer in flight and stops rendering, requiring reboot;
an error return cannot prove DMA quiescence. Direct callers must likewise
retain destination storage. There is deliberately no production reset/unpin
API based merely on a failed wait or GE close.

Host validation: 21/21 PASS. Native submission tests use real SDK descriptors
with mocked GE/cache operations: eight layouts, all orthogonal rotations,
nonzero layer origin, aligned/odd clipping, address/alias rejection, producer
retirement during sync and all three hardware failure stages. The mock alone
can prove it has no pending DMA and explicitly releases quarantine in tests.
The actual dispatcher fault-stop branch and physical DMA/cache behavior still
need target/board validation.

The internal MPP adapter preserves format, plane addresses, strides and color
space. It rejects pointers outside the configured physical window/32-bit range,
16-bit stride truncation, insufficient padded DMA rows, unequal planar U/V
strides and subsampled dimensions that the HAL would silently round.
Ordinary RGB GE source/destination acceptance and JPEG/PNG decoder policy
remain RGB-only; widening shared format translation does not enable YUV draws.

SDK evidence:

- lv_draw_ge2d_utils.c advertises four planar formats, I420/I422/I444/I400.
  NV/packed layouts are supported by GE HAL but are not equivalent to the
  legacy LVGL mapper's accepted set.
- hal_ge_normal.c masks chroma-subsampled source crop coordinates/dimensions
  down to even values. Future clipped GE submission must preserve boundary
  pixels, with CPU fallback where exact geometry is not representable.
- GE registers carry two 16-bit strides: planar U and V share the UV stride.
- hal_ge_hw.c selects BT.601/709 limited/full conversion from buffer flags.

Host: 19/19 contracts PASS. YUV tests cover 8 layouts, 4 matrices and 256
deterministic luma/chroma patterns against independent floating-point Kr/Kb
equations (maximum accepted difference 1), odd edges, independent padding,
short spans, pointer overflow, output aliasing, MPP metadata and unchanged
outputs on invalid input. MPP mapping uses synthetic addresses without DMA.

A board startup CPU probe checks four color matrices, odd I420 output/guards
and metadata rejection/acceptance. A separate new CMA I420 probe calls the
public image executor at all four orthogonal rotations, compares 512 pixels
per rotation against CPU conversion (tolerance 3), and checks every outside
pixel. Its DMA allocations remain pinned after a hardware failure.

Target validation: PASS, full ge2d-fonts-gif-widgets-aicp profile.

- Command: tools/sdk/build.ps1 -Phase ge2d -WithFonts -WithGif -WithWidgets -WithAicp -Jobs 8.
- SDK: cbaf5779; component: 8132f95; LVGL: 80ca777e.
- Boot/app builds, static checks, image checks and manifest: PASS.
- Allocated target text contains conversion/metadata/probe code, source leases,
  lv_draw_aic_ge2d_yuv and lv_draw_aic_ge2d_prepare_yuv_cache; these are not
  merely discarded source objects.
- Evidence: SDK output/lvgl-evidence/ge2d-fonts-gif-widgets-aicp.
- Image: images/d13x_D50T-2-Lite_page_2k_block_128k_v1.0.0.img.
- SHA256: 5f68739c4430a9ee2eea5226e19b616456b737f65c30d4a2e820b92dbbfef60a.
- Manifest records clean source states; later documentation commits are
  excluded from this build, and the profile directory is reused by later builds.
- Physical board execution: NOT_RUN.

This image adds native-size YUV tiling and its clipped 2x2 board probe to
the combined-rotation 0d3859b candidate
(SHA256 2aed74d90d49c776484d85dc8c349a07ae67c93162ae55706c7f2b259cdd1793).
That image added corrected combined rotation and seven scaled board probes to
the unrotated-scaling 1b3e5b2 candidate
(SHA256 bff582369c94d5d8dd0182c8a99058c0eacaeb44a34b261df2825494bda6b608).
Earlier stages include the native-rotation f709dd6 candidate
(SHA256 4c77cf3929c2efcb13a3d09e6b50213f10382978417c6c0cfaab3f6d5a151518).
It supersedes the CPU-decoder-only 467509b candidate
(SHA256 467e6682531d0aebe82d8d855ef30948a04240bf40eabce498d7d084f01d7d40)
and the earlier frame-contract-only a32613c candidate.

Dispatcher host validation: PASS (21/21 full host suite). The production
dispatch callback is tested with real LVGL task allocation and selection.
Ordinary image failure becomes FAILED and clears the active task; a YUV DMA
fault retains IN_PROGRESS, the active task and destination buffer. Repeated
dispatch leaves a second queued task WAITING, performs no additional image
execution and increments the error counter only once. This synchronous mock
does not establish hardware quiescence or authorize a production reset path.

Bounded transformed tiling is now implemented; see ge-tiling-stage.md.
Remaining scope: transformed-tile board probes, broader board CSC
and clipped-rotation numeric probes,
and camera/player/video-window ownership. This stage does not close those gaps.

## Packed YVYU/VYUY firmware evidence (2026-10-04)

Host **54/54 PASS**. Full GE/fonts/GIF/widgets/AICP/player/APNG profile
boot/app/static/image/manifest gates **PASS**. Both new formats also have
four-color-space cropped pixel probes in the board runner.
Clean component `7f04e4db77383213efea4dc7cf1d52b1617e7e93`,
SDK `ccd4100d84a5ee52f55a550a807a417d2f3ee8c6`.
Image `images/d13x_D50T-2-Lite_page_2k_block_128k_v1.0.0.img`,
SHA256 `96883af79cd3eb411fe201ff83a4d4424d4dfeb2db0207a4ceced10b951a5f75`. Board **NOT_RUN**.
