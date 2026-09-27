# D50T-2-Lite 帧缓冲导出

2026-09-27：已构建进新的 GE2D 测试镜像，串口实板抓取尚未验证。
这读取提交给显示控制器的 UI 帧缓冲，包含 GE 绘制结果；不是重新调用软件渲染器截图。
能用于分析图像缩放、裁剪和混合。LCD 物理颜色、撕裂、背光或额外硬件视频图层，
仍需实屏照片/录像交叉确认。

## 抓取

开启终端的“保存接收日志”，在测试页面运行后执行：

    lv_aic_capture

等待 capture ready，再执行：

    lv_aic_capture dump

等待 AICCAP END 后保存日志（单个 BEGIN/END 块；可包含其他启动日志）。
115200 串口导出较慢，画面复杂时可能需要数分钟；页面继续运行，导出的是同一张冻结帧。
UART 日志若穿插破坏一行，序号/CRC 检查会失败，不会悄悄生成错误截图。
放弃已请求或已抓取的帧可用 lv_aic_capture free，释放临时 CMA。

在 PC 的 SDK 根目录执行：

    python packages/custom/lvgl-aic/tools/sdk/capture_to_png.py capture.log capture.png

脚本只使用 Python 标准库，支持 UTF-8/带 BOM 的 UTF-16 日志；校验完整长度、
连续序号、RLE 边界及 CRC32 后才输出 PNG。可把日志或 PNG 交给 agent 做像素检查。

## 同步与范围

- 仅 manual-test 源码导出 Shell 命令；Shell 只发布请求，不调用 LVGL API。
- 30 ms 测试页定时器在 LVGL owner 线程中复制最后一次成功 PAN/VSYNC 的 framebuffer。
  保存的是物理方向、实际 byte stride；双缓冲使用最后提交的索引，不用“下一帧”索引。
- 复制前失效 CPU cache，获取已完成的 GE 输出；独立 CMA 副本让 UART 发送不阻塞 UI 线程。
  800x480 RGB565 临时占用 768000 字节，RGB888 为 1152000，ARGB 为 1536000。
- RGB565/RGB888/ARGB8888/XRGB8888 支持；导出 RGB 显示值，PNG 不带透明通道。
- 每行最多 6 个像素游程，日志行小于本配置 128 字节 console buffer。
- dump 完成自动释放，下一次请求重新抓取；没有测试页定时器时请求不会完成，可 free 取消。

验证：host 检查四种格式/通道顺序、PNG CRC、丢行、重复块、长度错误、CRC 错误及截断；
target 编译/map 确认命令和快照函数有效。实板命令、图像内容及面板对应关系仍待确认。
