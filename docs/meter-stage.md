# Meter cluster demo stage (G1)

Application-owned port of `packages/artinchip/lvgl-ui/aic_demo/meter_demo/`
for LVGL 9.6. Source: `demos/meter_demo/meter_ui.{h,c}` (+ vendored
`demos/meter_demo/assets/`, 169 files), Kconfig
`AIC_LVGL_BUILD_DEMO_METER`, build flag `-WithMeter`
(`--with-meter` in `check_integration.py`). File layout, widget tree,
timer callbacks/periods, sweep table and asset subpaths follow the
reference `meter_ui.c`; shell gate `lv_aic_meter_test` mirrors the APNG
mailbox pattern.

## 9.1 -> 9.6 deltas (all in the demo, never in LVGL core)

- `lv_img_*` -> `lv_image_*`; `lv_coord_t` -> `int32_t`;
  `lv_scr_act()` -> top-layer overlay (below); `lv_obj_add/clear/has_flag`
  -> dedicated `lv_obj_set_hidden/scrollable` (v9 deprecations are
  `-Werror` on host).
- No `aic_ui.h` (`LVGL_PATH`/`ui_snprintf`), no `mpp_fb.h`
  (`fbdev_draw_fps`), no `lv_port_disp.h`: asset root and FPS source are
  injected through `meter_ui_configure()`; `ui_font_regular` (vendor
  generated) replaced by built-in `lv_font_montserrat_14`; the FreeType
  branches and CPU label are dropped.
- Bitmap widgets share one uniform scale (`lv_image_set_scale` about the
  top-left + `meter_fit()` positions) so the 1024x600 design letterboxes
  into any panel (800x480 -> zoom 200); background centers about its own
  pivot. Needle strip frames stay vendored under `assets/point` for
  phase 2 (scaled-rotation board validation); until then the reference
  `LV_METER_SIMPLE_POINT` rotation path runs on all targets.
- `ui_init()` is omitted so several demos can link together;
  `meter_ui_init()` is the entry (`meter_ui_configure()` first when
  non-defaults are wanted). New test-only APIs use the file prefix:
  `meter_ui_destroy/timer_fires/get_speed_step/get_needle_angle`.
- Overlay on `lv_layer_top` (same pattern as the APNG acceptance
  panel): the smoke/manual nav header lives on the top layer and
  survives screen loads; all roots use `LV_OPA_COVER` (v9 default
  objects are transparent). `close` deletes the overlay, smoke page
  untouched underneath.
- Timer periods unchanged (point 10 ms, speed 100 ms, signal 500 ms,
  other 600 ms, fps 1 s, gear 3 s, trip 5 s, time 60 s).

## Host evidence

- `tests/host/meter_demo_contract.c` (`-DAIC_BUILD_METER_DEMO_TESTS=ON`,
  sibling `LVGL_ROOT` 9.6.0): create on a 1024x600 ARGB8888 display,
  2.5 s virtual time, asserts timer firings increase, needle angle and
  speed digit move, flushes occur and pixels vary; re-entry returns the
  live root and destroy clears state. **PASS** (MSYS2 UCRT64 GCC 16.1.0,
  2026-10-07). Asset files, GE acceleration,
  FPS accuracy and board timing are NOT covered here.

## Assets packaging (test image)

- 169 vendor files (~0.95 MB) vendored at `demos/meter_demo/assets/`
  ride the normal pipeline: component `SConscript` adds
  `INSTALL=[('demos/meter_demo/assets/', 'rodata/lvgl_data')]` under
  `AIC_LVGL_BUILD_DEMO_METER`; `fsinstall.py` stages to
  `output/<prj>/images/rodata/lvgl_data/`, `makefatfs.py` packs from
  there into `rodata.fatfs` (14 MB partition, mounted at `/rodata`;
  `L:` maps to `/`). `packages/` sources are not referenced at build.
- Binding: background `bg/bg_red.jpg`, three `warning/*.png`, all digit/
  level/gear images via `lv_image` (MPP FILE decode); needle stays on the
  simple rotation path, strip frames vendored for phase 2.

## Board evidence (`demo-meter-toplayer` image, 2026-10-07)

- Serial: `output/lvgl-evidence/board-2026-10-07-meter-toplayer/serial.log`
  (31k lines; boots of older GE2D candidates plus three
  `e4cb5194+dirty` meter sessions).
- `lv_aic_meter_test show` -> `meter demo running fires=0 speed=0 angle=0`;
  `lv_aic_capture dump` -> `AICCAP BEGIN 1 800 480 RGB888 frame=1503`,
  full dump `AICCAP END lines=14567 crc32=eb931281`.
- Terminal column-truncation broke 626 DATA lines (split RLE token at the
  wrap column); whitespace-fused repair decodes with matching CRC32, so the
  frame is intact: `meter-capture-frame1503.png` in the same directory.
- Frame proves: top bar fully covered (top-layer overlay), `bg_red.jpg`
  + 3 warning PNGs decoded via MPP, needle/timers live (trip `0102`).
- Known issues visible in this frame, both fixed in the current tree:
  background unscaled (dial right-shifted, right side cropped) ->
  uniform letterbox `fit()`; fallback text labels overlapping PNG
  digits -> labels only without assets.

## Target evidence (`-WithMeter`, 2026-10-07)

- `build.ps1 -Phase ge2d -Jobs 8 -WithMeter -AllowComponentDirty
  -EvidenceTag demo-meter-assets`: boot/app/static/image/manifest 全 PASS
  （首轮曾因 `lv_aic_meter_test.h` 未带入空宏归一化链致链接失败，
  按 APNG 门惯例 include `lv_aic_manual_test.h` 后解决）。
- Pins: `sdk@e4cb5194` / `lvgl-aic@a4db7ff`+工作区 diff（随证据归档）
  / `lvgl@80ca777e`；`board_validation: NOT_RUN`。
- Image SHA256 `31fbe30a4cad616311ca18caf4bd13d60a37b95b30d323b9f38a7a21c1203679`，
  ELF SHA256 `b0c087c78cd121574fb291eb1e03dbd00d4b5a1322ba22de30ad1da63cd682e6`。
- `images/rodata/lvgl_data` 169 文件（`rodata.fatfs` 打包输入一致）。
  中间曾踩 `fsinstall.py` 尾部分隔符坑致 0 文件 + `C:\` 散射，
  组件侧补 `os.sep` 解决，SDK 零修改（见 SConscript 注释）。
- 独占 screen 修订（`demo-meter-screen`）：Image SHA256
  `410c2d47efe52b3cf427f9ba3d09f01684fbe7fea5050bdfaae4483260cf75a1`，
  assets 仍 169 文件；`close` 回 smoke 页。
- top-layer overlay 修订（`demo-meter-toplayer`）：板级照片证明
  screen 方案盖不住 `lv_layer_top` 导航栏，改与 APNG 门相同的
  top-layer 全屏 overlay；Image SHA256
  `c0bcc878c7b4c311bf01f6dd68abe7054c1c0f5e9ef4513c1722889fbd4c2156`，
  assets 169 文件；全门 PASS，板级 capture 已验证（见上节）。
- 官方命名 + 分辨率自适应修订（当前树，未出镜像）：`demos/meter_demo/`、
  回调/入口/素材路径与官方同名；1024x600 设计经 `fit()` 等比进
  800x480（zoom 200）；`ui_init()` 缺省以便多 demo 共链。
  镜像（`demo-meter-official`）：全门 PASS，Image SHA256
  `5f98a3a20669a1fb1bf666df560973a94982ade04404e50e55d4298ef6ebda4f`，
  assets 169（组件内路径），`meter_ui_init/destroy` 活符号确认；
  待刷机验证 fit 后的完整表盘。
- 背景 pivot 锚定修正（`demo-meter-bgfix`，当前树未提交）：LVGL 缩放
  保持 pivot 不动，背景按中心定位会右偏 112px；改
  `widget_pos = 居中目标 - pivot*(256-zoom)/256`。Image SHA256
  `ae5e2251ae7f602a11ba85e5637c86dbf4f91260683187d35643068ecac1ae9d`，
  全门 PASS，待刷机目检。
- 待板级：刷机 → `lv_aic_meter_test show` → `lv_aic_capture dump`，
  确认背景 jpg + 3 警告 png 解码显示与 timer 跑帧。

## Open (target/board)

- Asset binding (`<root>/bg/bg_red.jpg` etc. via `lv_image` + MPP FILE
  JPEG/PNG) needs `rodata/lvgl_data` packaging and a `-meter` image
  (`build.ps1 -Phase ge2d -WithMeter -EvidenceTag ...`).
- Board: single-display点亮 + `lv_aic_capture` 导出 + 9.1/9.6 fps/RAM
  对比（目标差值 ≤10% 或解释）；触摸无交互需求，属纯展示页。
- `dashboard_demo` 与本页管线重叠，G1 板级收口后再复用接入。
