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

Remaining: explicit decoder close-count instrumentation, transformed tiling,
board numeric probes and target build validation.
