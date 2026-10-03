# GE image tiling

Native-size, unrotated IMAGE tiling now follows LVGL's image_area anchor and
positive tile stepping. Each tile intersects the task area, task clip and
layer buffer before using the existing RGB/ARGB blit.

同一次解码持有完整图片，先遍历所有 tile 检查支持范围，再提交绘制。
预检查不触发缓存维护或 GE 操作。任何 tile 不支持时整项软件回退；
提交阶段发生错误则停止，禁止整图软件重绘造成 alpha 重复叠加。
逐行解码器、旋转/缩放平铺和 tiled LAYER 仍走软件。

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

Remaining: explicit decoder close-count instrumentation, transformed tiling
and physical-board numeric acceptance.
