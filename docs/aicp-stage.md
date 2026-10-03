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
Remaining: genuine AICP fixtures, FILE decode/cache lifecycle tests,
V31 four-component tests, enabled-codec target link and board pixel parity.
Do not enable the codec in a release solely on the basis of these host tests.
