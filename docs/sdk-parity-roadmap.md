# SDK parity roadmap — SDK lvgl-ui (LVGL 9.1.0) 与 lvgl-aic (LVGL 9.6.0) 对齐规划

Working draft（未提交，2026-10-07）。供后续 agent 接手执行；提交/推送策略由接手人按流程决定。

- 基线分支：`codex/sdk-basic-capabilities` @ `a4db7ff`（历史已压缩为 22 个功能提交）
- 旧历史保底：本地 ref `refs/lvgl-evidence/pre-squash-20261007`（= 旧 tip `4b7a356`，整条旧历史可达；远端不可解析）
- 适用平台：D13x / D50T-2-Lite / RT-Thread（其它 SoC 未认证）
- Pin 链：SDK `e4cb5194`（`codex/port-lvgl-9.6`）→ smoke `eea657a`（`main`）→ lvgl-aic `a4db7ff`

## 0. 对照对象与证据路径

SDK 原版（下称 9.1 栈，路径以 SDK 根为基准）：

- 版本：`packages/artinchip/lvgl-ui/lvgl_v9/lvgl/lvgl.h` -> `LVGL_VERSION_MAJOR/MINOR/PATCH = 9/1/0`；core 无 AIC 补丁，适配均在周边目录。
- 驱动/加速：`packages/artinchip/lvgl-ui/lvgl_v9/lv_drivers/`
  - `lv_port_disp`、`lv_port_indev`、`lv_port_encoder`、`lv_port_mouse`
  - `lv_ge2d/`：fill、img（含 tiled）、img_scale、YUV（正交旋转）；源码显式拒绝任意角+缩放、recolor、mask
  - `lv_mpp_dec/`：stream、img_dsc、bmp
  - `aic_lv_ft_cache/`：vendor FreeType cache
- 控件：`packages/artinchip/lvgl-ui/aic_widgets/`
  - `aic_camera`（携带 `yydecoder`）、`aic_canvas`（v8/v9 预编译 `.a`，闭源）
  - `aic_player`（`player_backend/`：aic_backend_ops、png_backend_ops）
  - `aic_spi`（6 个面板驱动：gc9a01、gc9d01n、st7789、st77903、st77912、st77916）
  - `aic_video_window`、`img_roller`、`swipe_v1`
- 示例：`packages/artinchip/lvgl-ui/aic_demo/`（17 套）：ai_eyes、aic_widget_demo、dashboard、demo_hub、dm_daemon、double_disp、five_disp、image、meter、multi_lang、ota、screen_ctl、serial_com、slide、spi_screen、ui_builder、usb_osd；入口 `aic_ui.c`、`lv_demo.c`
- 配置：`lvgl_v9/lv_conf.h`（GIF/MONKEY/SYSMON=1，FREETYPE=0，PROFILER=0，`LV_COLOR_DEPTH 32`）

移植版（下称 9.6 栈，路径以组件根 `application/rt-thread/lvgl-aic-smoke/third_party/lvgl-aic` 为基准）：

- 上游 LVGL：`third_party/lvgl` pin `9.6.0@80ca777e37a2b176770726a02e07a6fb79ef0b39`；`LPKG_USING_LVGL` 必须保持关闭
- 组件文档：`docs/capabilities.md`（含 SDK parity audit 章节）、`docs/compatibility.md`、`docs/validation.md`、各阶段 `docs/*-stage.md`
- 主机契约：`tests/host`（107 文件）；CI：`.github/workflows/host.yml`
- 已知集成约定：LV_OS_CUSTOM + RT event；不 patch kernel、不注册 packages/、只动 target/app 配置
## 1. 上游 9.1 -> 9.6 增量（目录级比对）

- 仅 9.6 存在的控件：3dtexture、arclabel、barcode、ffmpeg、gif、gstreamer、ime、lottie、property、qrcode
- 仅 9.6 存在的库：frogfs、FT800-FT813、gltf、gstreamer、libwebp、nanovg、vg_lite_driver
- 无 9.1 独有项（9.6 为超集）
- 移植版已启用的相关项：GIF 原生解码、barcode、SVG/Lottie（ThorVG）、matrix/float

## 2. 模块级对比

| 域 | SDK 9.1 栈 | 移植版 9.6 | 结论 / 差距 |
| --- | --- | --- | --- |
| 显示 | lv_port_disp（fb） | fb + PAN/VSync + 软件旋转（port/lv_aic_display.c） | 功能对齐；物理 scanout/时序待验 |
| 输入 | lv_port_indev/encoder/mouse、lv_tpc_* | 触摸（RT device/OSAL）+ 编码器 + USB 鼠标 provider | 对齐 |
| GE2D | fill/img(tile)/img_scale/YUV 正交旋转；拒绝任意角+缩放、recolor、mask | 全套：fill 渐变、img/tile、layer、mask、色键、recolor、multipass 任意角、YUV 12 格式（旋转/缩放/平铺/掩码/键）、stripes、ARGB 目标、premult、preflight | 移植版为超集；剩余为板级像素/cache 验收 |
| 解码 | lv_mpp_dec（stream/img_dsc/bmp） | MPP JPEG/PNG RAW(+RAW_ALPHA)、BMP/AICP、fake image/fs、有界解码缓存 | 对齐；AICP alpha fixture 需 V31（板级 SKIP） |
| 字体 | LV_USE_FREETYPE=0 + vendor aic_lv_ft_cache | 原生 FreeType 动态字号 + 自有 ft cache（SDK 兼容 API：print_stats/drop_all/drop_specific/get_stats） | 基本对齐；L1 路径未验证、缺全局字节预算；README 与 ft-cache-stage 表述不一致（见 R2） |
| 播放器 | aic_player + png backend | 会话/分配器/时钟/事件/帧/worker/widget/control/slave/group；APNG backend | 缺：真实 A/V 时序、APNG rate parity、多组物理验证、video-plane 物理 scanout、音频混音未实现 |
| 相机 | aic_camera + yydecoder | VIN session + worker + widget + barcode mailbox | 缺：yydecoder 对照结论、真实条码解码上板、吞吐/栈预算 |
| SPI | aic_spi + 6 面板驱动 | 管线抽象（transfer/GE/handoff/worker/session/panel）+ stripes | 缺具体面板驱动（最大代码级缺口）；当前镜像不初始化任何面板绑定 |
| 控件 | camera/canvas(.a)/player/video_window/roller/swipe | 同名单控件（canvas 自研、无闭源归档）+ APNG/lottie resource/barcode | 对齐且开源化 |
| 多屏/OSD | double_disp/five_disp/usb_osd/screen_ctl | 无 | 按需求取舍（R9） |
| 示例 | 17 套业务 demo | upstream widgets/benchmark/music + smoke/manual 页 | 最大功能面差距（R5） |
| 工程化 | vendor 维护；无公开主机契约/板级证据 | 主机契约 107 文件 + CI + 分阶段文档 + 板级 run 证据与镜像 SHA256 | 移植版证据链更强 |

## 3. 验收状态锚点（2026-10-07）

- 最新板级：`scale-264-engine` 候选（banner `sdk@c290bbd1 / lvgl-aic@a31c613a / lvgl=9.6.0@80ca777e`）零 FAIL；GE2D counters `fill=23 image=19 layer=1 / declined=8 / errors=0`；YUV ARGB、掩码、色键、矢量块无失败；镜像 SHA256 与串口日志见 `docs/validation.md` 的 "full GE2D scale matrix closure (2026-10-07)" 节。
- 主机：GE profile 73/73 PASS、no-GE baseline 35/35 PASS；vector/SVG/Lottie 组合亦全绿。
- 仍未关闭：panel 边缘/触摸人工确认、`lv_aic_capture` 帧导出、APNG/plane/GIF shell gates、物理 scanout/GE 时序；SPI stripe 探针属 `-WithSpi` profile（未编入当前镜像）。
- 已知修正项：RGB565 色键生产默认关闭（引擎编码测量已完成、待决策）；MPP manual test 的 `rt_uint32_t -> rt_size_t*` 警告（应在组件侧修复、随新 pin 带入）；README 关于 vendor AIC FreeType cache 的 "remains a separate gap" 与 `docs/ft-cache-stage.md` 现状不一致。
## 4. 开发规划（R1–R11）

### P0 — 验收收口（把已实现能力推到板上 PASS）

- R1 综合回归镜像 + run 13 记录：GE2D/YUV/矢量/字体/MPP 全量探针 + `lv_aic_capture` 导出 + 面板视觉/触摸人工确认 + scanout/GE 时序；`-WithSpi` 单独一档跑 SPI stripe。验收：零 FAIL 串口日志存档（镜像 SHA256 + banner），`docs/validation.md` open 清单清零或写明降级理由。
- R2 修正项：MPP 指针类型警告修复；RGB565 色键默认策略决策（开/关 + 理由）；README/文档陈旧表述更新。
- R3 归档 "parity baseline"：镜像 + 日志 + pin 三元组，作为后续性能/功能对比基准。

### P1 — SDK 功能对齐（按产品价值）

- R4 SPI 面板驱动：按产品屏先做 1–2 个（建议 st7789 / st77916）；bus/pin/时序只放 target/app；评估 SDK 的 GE resize 滤波与当前 nearest 差异是否需要 GE 缩放路径。验收：面板点亮 + 视觉确认 + 吞吐记录（无面板绑定不得默认初始化）。
- R5 Demo 对等：先移 `meter`（或 dashboard）+ `multi_lang`，`demo_hub` 作统一入口；复用已有 SDK 形状兼容 API（`lv_aic_player_set_cmd`、`lv_aic_spi_*`、video window、barcode）。验收：同板同屏 9.1/9.6 帧率/内存对比（目标差值 ≤10%，否则给出解释）。
- R6 播放器收尾：APNG backend rate parity；真实 A/V 时序与 seek 精度；多播放器组与 video-plane 物理 scanout；音频混音明确为非目标或单独立项。
- R7 相机/条码：给出 yydecoder 对照结论（采用/不支持）；真实条码解码 + 吞吐/栈预算上板；相机与 player plane 的共享/仲裁路径验证。

### P2 — 平台与工程化

- R8 性能/内存基线：同 demo（meter/dashboard）对照 9.1/9.6 的 fps/RAM/PSRAM；PERF_MONITOR/SYSMON 作为可选 profile；GE/SPI 时序实测。
- R9 多屏/OSD/外设 demo 取舍：double/five disp、usb_osd、screen_ctl、serial_com、ota（能直接复用 SDK `ota`/`uds` 包的不重复实现）。
- R10 发布工程：组件 tag + CHANGELOG；parity matrix 固定为公开文档；9.7 升级窗口评估（契约重跑清单化）。
- R11 其它 SoC：仅在有明确需求时评估（当前只认证 D13x/D50T）。

## 5. 非目标

- 不修改 SDK `kernel/`、不把组件注册到 `packages/`、不 patch LVGL core。
- 不引入 vendor 闭源 canvas `.a`（已有自研实现）。
- 不为符号对齐强开 D13x 不适用的 H.264（`AIC_VE_DRV_V30` 限制，SDK 层结论）。
- 无产品需求时不全量搬运 17 套 demo。

## 6. 风险与依赖

- 板级验收依赖人工刷机 + 串口采集；每个 R 项需独立镜像与日志，不以主机结果代替。
- 历史已压缩：文档/记录中的旧 commit hash 在远端不可解析；映射见本地 `refs/lvgl-evidence/pre-squash-20261007`。
- 9.6 私有 API 依赖集中在 `compat/lvgl_aic_private.h`；升级 LVGL pin 必须重跑契约（见 `docs/compatibility.md`）。
- 构建命令与证据流程见 `application/rt-thread/lvgl-aic-smoke/README.md` 与组件 `tests/host`（CMake/Ninja）。
