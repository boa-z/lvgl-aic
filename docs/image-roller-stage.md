# SDK image roller adaptation

Enable `AIC_LVGL_USE_IMG_ROLLER` in the application's Kconfig and include
`lv_img_roller.h`. The component compiles its own adapted widget; it does not
link the SDK's legacy `lvgl-ui` or `aic_ui.h`. The implementation derives from
ArtInChip's Apache-2.0 `aic_widgets/img_roller` with its attribution retained.

The SDK create/add-child, direction, loop mode, ready, active selection and
active-ID APIs are retained. Add images, size the container, update layout,
then call `lv_img_roller_ready`. Sources remain application-owned and must
outlive their image children. Horizontal and vertical layouts, center snapping,
distance-dependent zoom and animated selection use upstream LVGL scrolling.
Use uniform-size images for the SDK-compatible carousel layout. Child user data
is reserved for stable IDs; use `lv_img_roller_get_child_by_id` after loop
reordering rather than interpreting child position as its ID.

The ratio parameter is widened from uint8_t to uint16_t so 256 (no scaling)
is representable; values are clamped to 64..256. Internal structs are private,
with LVGL class internals imported through the component compatibility header.
Default direction, ID lookup, recursive scroll adjustment, empty/short content,
zoom bounds and event-handler deletion are corrected relative to the SDK code.

Host configuration: `-DAIC_BUILD_IMG_ROLLER_TESTS=ON`. The production widget
is compiled with warnings treated as errors and tested through real LVGL layout,
scrolling and timers across repeated create/delete cycles. Tests cover IDs,
invalid selection, horizontal/vertical selection, animated selection, loop
reordering and teardown during animation. Physical drag feel, panel rendering
and GE interaction remain pending consolidated board validation.

This is the SDK image carousel; native LVGL's text roller is a separate widget.
SDK swipe_v1 and camera/player/video-window remain separate pending ports.
