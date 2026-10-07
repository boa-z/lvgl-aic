# aic_demo 补齐目标（无 SPI/多屏验证环境约束，2026-10-07）

上游：`packages/artinchip/lvgl-ui/aic_demo/` 17 套；现状：只有 upstream widgets/benchmark/music + smoke/manual 页。
结论：最大功能面缺口属实，但 `demo_hub` 全量（~15–20k LOC）不可一次性搬；`spi_screen/double/five` 无验证环境直接挂起。

约束（实际审查得出）：
- 9.1->9.6 必改：`lv_img_* -> lv_image_*`、`lv_img_dsc_t/LV_IMG_CF_RAW/lv_img_cache_*`、`lv_coord_t` 移除、`_lv_ll_*` 私有链表（`multi_lang.c`）、`lv_ft_*` 分支；`LVGL_PATH/LVGL_DIR/aic_ui.h/mpp_fb.h/fbdev_draw_fps` 不能直链
- Kconfig 必须用 `AIC_LVGL_BUILD_DEMO_*`（如已有 `BUILD_DEMO_WIDGETS/BENCHMARK/MUSIC`），不得选中 legacy `AIC_LVGL_*_DEMO`；只动 target/app 配置，不动 kernel/packages/LVGL core
- 每个 goal 独立镜像 + `output/lvgl-evidence/<tag>` + 三行 `build:` banner；主机契约先行，板级以串口零 FAIL + `lv_aic_capture` + 视觉确认为准

## G1 meter_demo（首选，产品价值最高）
- 范围：`meter_demo/meter_ui.c:438-627` 逻辑 + 必需 assets（bg/warning/point/speed_num/time/mileage 子集先行，全量 170 资源后补）；`font/ui_font_regular.c` 用原生 FT/内置字体替代验证
- 验收：单屏点亮 + 指针/数字 timer 跑帧 + `capture` 导出；同屏 9.1/9.6 fps/RAM 差 ≤10% 或解释；GE `img+rotate/scale` 走硬件路径可测
- 验收：单屏点亮 + 指针/数字 timer 跑帧 + `capture` 导出；同屏 9.1/9.6 fps/RAM 差 ≤10% 或解释；GE `img+rotate/scale` 走硬件路径可测

## G2 multi_lang_demo（次选，与 G1 并行无冲突）
- 范围：`multi_lang_demo/multi_lang.c:19-199` 的 ini+style 表（`_lv_ll_*` 改走 `compat/lvgl_aic_private.h` 或自有小表）+ `screen_white/dark` 两屏 + `assets/lang/*.ini`
- 验收：中/英切换 + CJK fallback 渲染 + 浅/深两屏 capture；不引入全局字体字节预算外的新分配器

## G3 image_demo + slide_demo（小而高复用）
- `image_demo/image_ui.c:106-139`：FILE/MEM 双路径改用 MPP JPEG/PNG + 有界解码缓存语义（`lv_img_cache_*` 不得直搬）；`slide_demo/slide_ui.c + hor_slide_screen.c` 验证 swipe/滚屏手感
- 验收：cook_0..3 切换无泄漏（100-hit 无新增 CMA 思路复用 resource-stage），滑动页 capture 正常

## G4 demo_hub 最小 launcher（骨架，不搬 15k 业务屏）
- 仅 `demo_hub.c + app_entrance/navigation` 入口框架 + 已移植 G1–G3 入口注册；86box/coffee/elevator/photo/video/audio/camera/dashboard/steamer 等业务屏明确为后续子 goal，不在此 goal 内
- 验收：launcher 可切 G1–G3 + 返回，切换无野指针/泄漏（复用 widget-lifecycle 思路）

## 暂缓（写明理由，不算缺口隐瞒）
- `dashboard_demo`：与 meter 重叠，G1 后复用其 GE/asset 管线再做
- `aic_widget_demo`：依赖 player/APNG/camera（video-plane/scanout NOT_RUN），等 R6/R7 收口后再对接
- `ai_eyes(screen_ctl/ota/serial/usb_osd/dm_daemon)`：需 wifi/audio/OTA/串口/屏控外设环境，当前无验证条件
- `double/five_disp + spi_screen`：需多屏/SPI 面板环境，已与 SPI 一起挂起（R4）
- `ui_builder`：17 行 stub，最后顺手接即可

顺序：G1 -> G2 -> G3 -> G4；每个 goal 独立 `build.ps1 -Phase ge2d -WithDemos...` 证据目录（如 `-demo-meter`），`docs/validation.md` + `VALIDATION.md` 追加记录。
