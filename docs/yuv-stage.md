# YUV frame and conversion contracts

The application-owned frame API in include/lv_aic_yuv.h now describes borrowed
CPU-addressable planes with independent strides and capacities. It supports
I420, I422, I444, I400, NV12, NV21, YUY2 and UYVY, with explicit BT.601/BT.709
limited/full range. The producer owns lifetime and cache synchronization;
these descriptors must not be passed to LVGL as if they were decoded pixels.

CPU conversion to native RGB888 validates every span and output overlap before
writing. Odd dimensions use rounded-up chroma storage. Conversion performs no
allocation/cache insertion, does not mutate source planes and preserves output
padding. It supplies the future renderer's fallback/reference path; it is not
yet wired into LVGL image decoding or direct GE YUV submission.

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
and metadata rejection/acceptance. It deliberately logs that GE DMA is not
tested.

Target validation: PASS, full ge2d-fonts-gif-widgets-aicp profile.

- Command: tools/sdk/build.ps1 -Phase ge2d -WithFonts -WithGif -WithWidgets -WithAicp -Jobs 8.
- SDK: a45530e3; component: a32613c; LVGL: 80ca777e.
- Boot/app builds, static checks, image checks and manifest: PASS.
- Allocated target text contains lv_aic_yuv_to_rgb888, lv_aic_yuv_to_mpp
  and lv_aic_yuv_test_run; these are not merely discarded source objects.
- Evidence: SDK output/lvgl-evidence/ge2d-fonts-gif-widgets-aicp.
- Image: images/d13x_D50T-2-Lite_page_2k_block_128k_v1.0.0.img.
- SHA256: 49f9d124ef4d3c344fa1467b3ad7e02eebd2b058fd675625e718a6faf67d96a7.
- Manifest records clean source states; later documentation commits are
  excluded from this build, and the profile directory is reused by later builds.
- Physical board execution: NOT_RUN.

Remaining scope: application frame retain/release publication, LVGL decoder
integration, real GE YUV descriptors/cache maintenance, orthogonal rotation
and scaling with chroma phase, exact clip fallback, board CSC numeric probes,
and camera/player/video-window ownership. This stage does not close those gaps.
