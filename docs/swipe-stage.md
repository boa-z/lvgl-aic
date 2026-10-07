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
拒绝第五项、空资源以及越界资源下标。删除或移出子项会立即注销逻辑 ID、
释放资源对数组并取消正在进行的切换，不发送完成事件。移出的图片保留当前
资源，应用仍须保证资源本体存活；移回原控件或移入另一 swipe 不会自动注册。
详见[子项生命周期与真实点击验证](widget-lifecycle-stage.md)。

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
Target cross-build passed at component `e5843b0`, SDK `faebbbc5`:
boot/app build, static/live-symbol checks, image check and manifest all PASS.
Evidence: SDK `output/lvgl-evidence/ge2d-fonts-gif-widgets`.
Image SHA256:
`cfab814c0a890f0ddb8d686248655dc69979ea863c495f0c7685e07650f4ebd9`.
Physical display/input acceptance remains NOT_RUN. Host PASS
does not establish hardware rendering, DMA/cache behavior or performance.

## Reentrant child replacement during transition events (2026-10-04)

SCROLL_BEGIN/SCROLL_END handlers can delete a child, add a replacement and start
another animation before the original callback returns. The old count/switching
checks could mistake the restored four-child/new-animation state for their own
transition and continue or emit a stale VALUE_CHANGED completion.

Each transition now carries a generation; child deletion invalidates it. After
begin/end notification the old call verifies that its generation still owns the
transition, so a replacement animation retains its guard and completion event.
No user-visible API changes are required. Normal nested requests without child
removal remain rejected while switching; deletion from events remains supported.

New tests reproduce the bug before the fix (completion count assertion failure)
and replace/restart from both event boundaries. After the fix **70/70 host PASS**;
only the replacement animation emits completion. Existing direct/animated,
forward/backward, object deletion and cancellation cases remain covered.
Strict D13x compilation PASS. SDK object
`output/swipe-generation-lv_swipe_v1.o` SHA256:
`3b99caa1f262f44101d771f6a97d4b196b3647f087f55e34e27642749d009919`.
Logs: component `output/swipe-generation-before.log`,
`output/swipe-generation-build.log`, `output/swipe-generation-tests.log`,
`output/swipe-generation-target.log`. The subsequent combined firmware refresh passed; see [validation](validation.md).
Physical input/rendering acceptance remains **NOT_RUN**.
