# AICP decoder integration

组件在 SDK 启用 `AIC_MPP_AICP_DEC_ENABLE` 时识别大小写无关的
`.aicp` 文件及带 AICP 前缀的 RAW/RAW_ALPHA 内存资源。
信息解析验证前缀、JPEG SOI、SOF 长度和分量数；完整编码数据
（包括四字节前缀）送入 `MPP_CODEC_VIDEO_DECODER_AICP`。

RGB 输出复用现有 MPP 的外部分配器、CMA 限额和 LRU 生命周期。
四分量仅在 `AIC_VE_DRV_V31` 时声明 ARGB8888；普通四分量
JPEG 仍被拒绝，不能将 CMYK 当成 AICP 透明图片。

Host option `AIC_BUILD_AICP_TESTS=ON` enables source/header contracts.
Enabled and disabled MPP configurations both pass 2/2 tests. Tests cover
prefix recognition, codec selection, packet cursor/size preservation,
RGB geometry, truncated headers and inconsistent component lengths.

This is integration code, not verified AICP pixel decoding. The SDK codec
implementation is supplied outside the visible decoder source tree.
SDK bird.aicp (three components) and flower.aicp (four components) now
exercise FILE and RAW paths, decoded-buffer sharing, invalidation while
readers remain open, final CMA release and engine-error cleanup.
The engine is mocked: actual compressed pixels are not decoded on host.
Non-V31 rejects flower; AIC_BUILD_AICP_V31_TESTS=ON accepts its ARGB header.
Both platform contracts pass; the V31 host configuration passes 14/14 tests.
Fixtures are read from the external SDK without copying vendor assets.

Remaining: board execution and independent pixel reference comparison.
Target build entry: `tools/sdk/build.ps1 -Phase ge2d -WithAicp`.
It enables the SDK codec, stages bird/flower with SHA256 inventory, and checks
the live create_aicp_decoder symbol.
The four-component fixture remains unsupported on non-V31 targets.

Cross-build PASS at component 9ad728d / SDK 1d01d0d6 with
`-Phase ge2d -WithFonts -WithGif -WithWidgets -WithAicp`:
boot/app, codec live-symbol gate, image integrity and provenance checks pass.
Evidence: SDK `output/lvgl-evidence/ge2d-fonts-gif-widgets-aicp`.
Image SHA256:
`cdc829726af4ea0876630e487ddb41479f92f1ee8f4fe6c2dea6a6505de241f6`.
Board NOT_RUN. This image links the codec and contains assets; it does not
yet invoke the AICP fixtures in the startup resource probe.

The subsequent startup resource probe now runs bird.aicp when the codec
is enabled, and flower.aicp only on V31. It checks FILE/RAW pixel hashes,
shared readers, invalidation with active readers, CMA balance and 100 cache
hits without another allocation. Expect BEGIN AICP resource probes followed
by PASS AICP file-memory parity and cache lifecycle. Non-V31 explicitly logs
SKIP for the alpha fixture. FILE/RAW agreement is not an independent pixel
oracle; visual/reference-image acceptance remains necessary.
Do not enable the codec in a release solely on the basis of these host tests.
