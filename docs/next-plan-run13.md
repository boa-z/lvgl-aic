# 接下来的开发规划（基于 sdk-parity-roadmap.md + 实际代码审查，2026-10-07）

基线（worktree 实测）：
- SDK `luban-lite-jc-d50t-rev`：`codex/port-lvgl-9.6@e4cb5194`，submodule `application/rt-thread/lvgl-aic-smoke@eea657a(main)`
- `third_party/lvgl-aic@a4db7ff`（squashed 22 提交），`third_party/lvgl@80ca777e`（9.6.0）
- `docs/sdk-parity-roadmap.md` 为 untracked working draft；`tests/host` 实际 117 文件（roadmap 写 107，已过时）
- 板级锚点：run 12 `scale-264-engine`（`sdk@c290bbd1 / lvgl-aic@a31c613a`）零 FAIL，`fill=23 image=19 layer=1 declined=8 errors=0`；当前 pin 已前移到 `e4cb5194/eea657a/a4db7ff`，需重做回归基线

实际移植结论：
- 已对齐/超集：display（fb+PAN/VSync+软旋转 `port/lv_aic_display.c`）、input（RT device/OSAL+encoder+mouse）、GE2D（fill渐变/img/tile/layer/mask/色键/recolor/multipass/YUV 12格式/stripes/ARGB target/premult/preflight）、MPP JPEG/PNG RAW(+ALPHA)/BMP/AICP/fake/fs+有界缓存、字体（原生FT动态字号+`print_stats/drop_all/drop_specific/get_stats` 兼容 API）、自研 canvas（无闭源 .a）、video-window/roller/swipe/APNG-lottie-resource/barcode-mailbox
- 最大代码缺口：SPI 只有管线抽象（`port/lv_aic_spi_{pipeline,session,worker,handoff,ge2d,panel,sdk,display}.c` 8 文件），零具体面板驱动（无 gc9a01/gc9d01n/st7789/st77903/st77912/st77916 绑定，当前镜像不初始化面板）
- 最大功能面缺口：17 套 `aic_demo` vs 只有 upstream widgets/benchmark/music+smoke/manual 页
- 待物理验证：panel 视觉/触摸人工确认、`lv_aic_capture` 导出（`tests/manual/lv_aic_capture.c:26,100`）、APNG/plane/GIF shell gates、scanout/GE 时序；SPI stripe 仅 `-WithSpi` profile

## P0 — 先收口（1–2 个镜像迭代）
- R1 run 13 综合回归：GE2D/YUV/矢量/字体/MPP 全量探针 + `lv_aic_capture dump` + 面板视觉/触摸人工确认 + scanout/GE 时序；`-WithSpi` 单独一档。验收：零 FAIL 串口日志 + 镜像 SHA256 + 三行 `build:` banner，`docs/validation.md` + `VALIDATION.md` open 清零或写降级理由
- R2 三个修正项（组件侧修，随新 pin 带入）：
  1. `tests/manual/lv_aic_mpp_test.c:100,106` `rt_uint32_t -> rt_size_t*` 警告
  2. RGB565 色键默认策略：引擎为 replicate 扩展（9/9 PASS `common encoding=engine`），生产保持关闭或写理由开启
  3. `README.md:35-36` “vendor cache remains gap” 与 `docs/ft-cache-stage.md` 不一致 + `tests/host` 107->117 数量更新
- R3 归档 parity baseline：镜像 + 日志 + pin 三元组（sdk/smoke/lvgl-aic/lvgl），后续 fps/RAM 对比以此为准

## P1 — 按产品价值对齐
- R4 SPI 面板 1–2 个先行（建议 st7789/st77916）：bus/pin/时序只放 target/app；评估 SDK GE resize 滤波 vs 当前 nearest 差异。验收：点亮+视觉+吞吐；无绑定不得默认初始化
- R5 Demo 对等：先 `meter`（或 dashboard）+ `multi_lang`，`demo_hub` 作入口；复用 `lv_aic_player_set_cmd`/`lv_aic_spi_*`/video-window/barcode。验收：同板同屏 9.1/9.6 fps/RAM 差 ≤10% 或解释
- R6 播放器：APNG rate parity、真实 A/V 时序/seek、多组 + video-plane scanout；音频混音明确非目标或单独立项
- R7 相机/条码：`common/lv_aic_barcode.c:4` 的 yydecoder 对照结论（采用/不支持）、真机解码 + 吞吐/栈预算、camera 与 player plane 仲裁

## P2 — 平台工程
- R8 性能/内存基线（同 demo 9.1/9.6 fps/RAM/PSRAM，PERF_MONITOR/SYSMON 可选 profile，GE/SPI 实测）
- R9 多屏/OSD 取舍（double/five disp、usb_osd、screen_ctl、serial_com、ota 复用 SDK 包不重做）
- R10 发布工程：组件 tag + CHANGELOG + parity matrix 定稿 + 9.7 升级窗口（契约重跑清单）
- R11 其它 SoC 仅按需求评估（当前只认证 D13x/D50T）

非目标：不动 `kernel/`、不注册 `packages/`、不 patch LVGL core；不引闭源 canvas `.a`；D13x 不强开 H.264（V30 限制）；无需求不搬全 17 套 demo
风险：板级依赖人工刷机+串口，每个 R 独立镜像/日志，主机绿不代替板级；历史已压缩，旧 hash 见 `refs/lvgl-evidence/pre-squash-20261007`；LVGL pin 私有 API 集中 `compat/lvgl_aic_private.h`，升级重跑契约
命令：`build.ps1 -Phase ge2d -Jobs 8 -WithFonts -WithWidgets -WithAicp -WithPlayer -WithApng [-WithSpi]`；主机 `tests/host` CMake/Ninja；证据 `output/lvgl-evidence/<tag>` + `docs/validation.md`
