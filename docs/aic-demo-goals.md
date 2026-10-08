# aic_demo 移植目标（2026-10-07 修订：官方源码原样 + 虚拟分辨率）

上游：`packages/artinchip/lvgl-ui/aic_demo/` 17 套，均按 1024x600（`demo_hub` 另有 480x272 素材）设计；D50T 面板为 800x480。

## 方式（替代原"逐个改写"方案）

- **源码原样**：官方 demo 目录原样复制到 `demos/official/<demo>/`（仅去掉 SDK `SConscript`），禁止修改；与 SDK 的差异只在 `demos/official/compat/` 与构建中处理。细则见 [demos/official/README.md](../demos/official/README.md)。
- **分辨率**：`AIC_LVGL_VIRTUAL_RES` 让 LVGL 以 1024x600 渲染，显示层每帧用 GE2D 整帧缩放一次到面板（800x469，上下黑边），触摸反向映射。见 [virtual-resolution.md](virtual-resolution.md)。产品 UI 仍按面板原生分辨率设计。
- **API**：LVGL 9.6 自带 v8 API 映射；缺失的旧名字逐条加到 `compat/lv_aic_v8_compat.h`。
- **多 demo 共存**：每个 demo 单独编译组，`ui_init`/`ui_font_regular` 按 demo 重命名，素材装到 `rodata/lvgl_data/<name>`。
- **运行**：`lv_aic_demo list|show <name>|close|status`；`close` 精确回收 demo 创建的定时器与屏幕。
- **验收**：主机 `official_demo_contract`（官方源码原样编译，show/run/close/重入/切换）+ 板级 CAN 截图目检 + `status` FPS。资源分区须用 `upgcmd` 整包烧录（`artinchip-flash` 不写 rodata，见 README）。

## 状态

| demo | 状态 | 板级 FPS（800x480，虚拟 1024x600） |
|------|------|------|
| meter_demo | ✅ 原样移植，板级 PASS | 12-24 |
| dashboard_demo | ✅ 原样移植，板级 PASS（需 `lv_style_set_arc_img_src` 兼容名） | 43-47 |
| slide_demo | ✅ 原样移植（源码已兼容 v9），板级渲染 PASS；滑动交互待触摸验证 | 33 |
| multi_lang_demo | ✅ 原样移植，板级 PASS（中文 ini 界面；切换需触摸验证）；compat：`lv_mem_*`、SimSun→Source Han Sans CJK | 静态页 |
| image_demo | ✅ 原样移植，板级 PASS（FreeType 40px 中文歌词在两个 canvas 上交替动画）。SDK 的 `libaic_canvas_v9_*.a` 按 LVGL 9.1 结构布局编译、调用 9.1 的 `_lv_log_add`，不能链接进 9.6；改用组件内源码实现的 `lv_aic_canvas`（行为逐函数对照反汇编，见 demos/official/README.md）。与 demo_hub 合计超出 rodata，需分开构建 | 33 |
| demo_hub | ✅ 原样移植，板级 PASS（启动器首页完整渲染；子应用需触摸验证）。只带 1024x600 素材（8.8 MB，五个 demo 合计占 rodata 12.4/14 MB）；依赖 player widget（`-OfficialDemos demo_hub` 隐含 `-WithPlayer`）；相机子应用无 `AIC_USING_CAMERA` 时为占位页 | 静态页（仅变化时重绘） |

先前按 800x480 改写的 meter（`8eb99ec`/`17f2e24`）已退役，由官方原样版本替代；其 MPP 缓存修复（GE/SW 步长共享、解码锁）保留在组件中。

## 顺序

1. demo_hub 子应用逐个验证（需触摸；或加 shell 入口直接打开子应用）

## 暂缓（写明理由）

- `aic_widget_demo`：依赖 player/APNG/camera（video-plane/scanout 未验证）
- `ai_eyes / screen_ctl / ota / serial_com / usb_osd / dm_daemon`：需 wifi/audio/串口/屏控等外设环境
- `double_disp / five_disp / spi_screen`：需多屏/SPI 面板
- `ui_builder`：17 行 stub
