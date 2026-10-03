# GE image tiling

Native-size, unrotated IMAGE tiling now follows LVGL's image_area anchor and
positive tile stepping. Each tile intersects the task area, task clip and
layer buffer before using the existing RGB/ARGB blit.

同一次解码持有完整图片，先遍历所有 tile 检查支持范围，再提交绘制。
预检查不触发缓存维护或 GE 操作。任何 tile 不支持时整项软件回退；
提交阶段发生错误则停止，禁止整图软件重绘造成 alpha 重复叠加。
逐行解码器和 tiled LAYER 仍走软件；受限变换平铺见下节。

Full host contracts pass 16/16. Coverage checks four clipped ARGB tiles,
edge crop dimensions and global alpha, zero submissions for inaccessible
sources, real LVGL bin-decoder public execution and failures at every tile.
Empty intersections report NOTHING rather than ENGINE. Traversal is bounded
by the visible intersection, including a two-billion-pixel task with a tiny clip.
These mock GE submission; actual tiled pixels and DMA/cache remain NOT_RUN.

Target build including RGB/ARGB tile numeric probes: PASS.
The startup scale suite now checks clipped 2x2 native tiles with an independent
modulo-coordinate oracle, including seam pixels, mixed alpha and every pixel
outside the clip. Physical execution of these probes remains NOT_RUN.

- Command: tools/sdk/build.ps1 -Phase ge2d -WithFonts -WithGif -WithWidgets -WithAicp -Jobs 8
- SDK source: a7a7f1c3; component source: 51b44c7; LVGL: 80ca777e.
- Boot/app builds, static symbol checks, image checks and manifest: PASS.
- Evidence: SDK output/lvgl-evidence/ge2d-fonts-gif-widgets-aicp.
- Image: images/d13x_D50T-2-Lite_page_2k_block_128k_v1.0.0.img.
- SHA256: d57ad5cb4d7b182741f5677e78f62998759398994cc1c1a8576b65f27b2102d6.
- Manifest records clean source states; later documentation commits are not
  part of this image. This profile directory is replaced by subsequent builds.

Remaining: explicit decoder close-count instrumentation, expanded transformed
tile board probes and physical-board numeric acceptance.

## Bounded transformed tiles / 受限变换平铺

RGB/ARGB tiled IMAGE now accepts the same bounded transforms as the existing
single-image executor: scaling, orthogonal rotation with scaling, and unscaled
arbitrary rotation. YUV tiled publication uses its existing bounded scaler and
orthogonal rotation path for all ten frame formats. Unsupported crops, small
filter footprints, chroma alignment, inaccessible buffers and scaler split-risk
cases still decline the whole task before submission. This is not universal
transform acceleration, and arbitrary-angle scaling remains unsupported.

LVGL 9.6 的 lv_draw_image_tiled_helper 仍以原图宽高步进，而不是变换后的尺寸。
每个原始 cell 单独变换、裁剪，不能把缩放倍率再乘到平铺间距。RGB 复用两阶段
blit，YUV 保留统一帧 lease 并两次遍历。某个变换 cell 没有交集时跳过该 cell，
全部 cell 都为空才报告 NOTHING，避免首个空 cell 导致后续可见内容丢失。

Host 24/24 PASS, followed by focused GE tests after adding failure/empty-cell
coverage. RGB uses the actual upstream tiled helper as a grid/clip oracle and
an independent floating inverse matrix for nonzero pivots, anisotropic 1.5x/2x
scales and 0/90/180/270 degrees. Q16 sampling phase tolerance is 1/4096 pixel.
An unscaled 45-degree case checks the rotation submission path. Tests also
inject failures at every transformed tile and reject an unsupported final cell
with zero prior writes. YUV covers all ten formats at 2x and 2x+90 degrees,
plus empty-first/visible-later and entirely empty transformed clipping.

These are descriptor/ownership tests with mocked GE; they do not prove physical
pixels, interpolation, DMA or cache behavior. The older native-tile board probes
do not establish transformed-tile hardware acceptance. Current target-image
build evidence is recorded separately after a clean source build.

Clean target regression build for transformed tiling:
- Command: tools/sdk/build.ps1 -Phase ge2d -WithFonts -WithGif -WithWidgets -WithAicp -Jobs 8
- SDK 1f2e34a5, lvgl-aic 230a51f, LVGL 80ca777e; manifest source states clean.
- Boot/app builds, static symbol checks, image verification and manifest: PASS.
- Evidence: SDK output/lvgl-evidence/ge2d-fonts-gif-widgets-aicp.
- Image: images/d13x_D50T-2-Lite_page_2k_block_128k_v1.0.0.img.
- SHA256: 74a92da2c0df09e2f0ae6080d7330b0070c42e2cb8fc8c175258666b21d98ceb.
- VIN/camera disabled; this is not a camera-enabled image. Transformed-tile
  hardware pixels and all new camera hardware acceptance remain NOT_RUN.
- This directory replaces the historical evidence above; this documentation
  commit is newer than the source commits that built the image.
