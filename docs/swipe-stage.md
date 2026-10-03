# SDK swipe_v1 adaptation

Enable `AIC_LVGL_USE_SWIPE_V1` and include `lv_swipe_v1.h`.
This component-owned adaptation retains ArtInChip's Apache-2.0 attribution
and public API. It does not depend on SDK `aic_ui.h` or alter upstream LVGL.

应用先添加四个子项的 active/deactive 资源对，通过
`lv_swipe_v1_get_child` 设置每个位置的坐标及缩放。尾项为当前活动项；
next/prev 循环交换四个位置，保留逻辑 ID。点击首项向前切换，
点击第三项向后切换，其余项循环选择资源组。

资源本体由应用持有，必须活到对应子项销毁。组件只拥有资源对数组，
不释放图片内容。支持非连续 ID（0..32767），每项最多 32768 对资源；
拒绝第五项、空资源以及越界资源下标。子项必须保留在原父对象下，
可以单独删除；删除会取消正在进行的切换，不发送完成事件。

## Changes from the SDK implementation

- Resource pairs share one allocation; failed growth preserves existing data.
- A single widget-bound animation drives positions and both scale axes.
- Switching is locked before callbacks, preventing overlapping transitions.
- ANIM_OFF and zero duration complete synchronously.
- Completion uses the completed callback, never the deletion callback.
- SCROLL_BEGIN/SCROLL_END/VALUE_CHANGED callbacks may delete the widget.
- The header's set_child_active_src and the SDK implementation's spelling
  set_child_activate are both available.
- NULL animation path selects the linear default.

## Evidence and remaining work

Host contract runs real LVGL timers, four-slot geometry and stable IDs,
resource bounds/cycling, immediate repeated commands, deletion during
animation, child deletion, and deletion from all three transition events.
The complete host configuration passed 11/11 tests.

`cmake -S tests/host -B output/lvgl-host-ge -DAIC_BUILD_SWIPE_TESTS=ON`
enables the contract in an already configured external-LVGL host build.

The shared manual UI has an optional SDK widgets page for both widgets;
host pointer tests navigate to it and activate its Next icon button.
Build it with `tools/sdk/build.ps1 -Phase ge2d -WithFonts -WithGif -WithWidgets`.
The profile checks both feature settings and live linked widget symbols.
Target cross-build and physical display/input acceptance remain pending. Host PASS
does not establish hardware rendering, DMA/cache behavior or performance.
