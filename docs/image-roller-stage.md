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
SDK swipe_v1 now has a separate [adaptation](swipe-stage.md).
Camera/player/video-window have separate application-owned adapters; see
[capabilities.md](capabilities.md) for their current validation boundaries.

## Pending-child deletion and reparenting (2026-10-04)

The inherited SDK scroll-end path held `obj_switch` without checking whether the
selected image was still a child. Deleting only that image during an animated
selection could later dereference freed storage. Moving it outside the roller
also left a stale selection and its click callback cast the new parent to the
roller's private type.

The port now clears a removed pending target on child changes and rechecks
membership at scroll-end using pointer comparison without dereferencing the
candidate. Click callbacks act only when the image still belongs to the original
roller. Remaining child IDs stay stable; adding another image does not reuse the
removed ID. Sources/user data and uniform-size layout requirements are unchanged.
Moving an image to another parent does not register it as that parent's roller item.

Regression evidence: new removal/reparent tests failed before the fix with Windows
heap corruption `0xc0000374`. After the fix, **70/70 host PASS**, including continued
selection and addition after removal, click on a reparented image and teardown.
Strict D13x compilation PASS; SDK `output/roller-child-lv_img_roller.o` SHA256:
`71250b24d08d17022e44107b320195ab91e8315a9a9001489e636a1780e806f1`.
Logs: component `output/roller-child-before.log`, `output/roller-child-build.log`,
`output/roller-child-tests.log`, `output/roller-child-target.log`.
Full-firmware refresh PASS at `eab1b40`; see
[validation.md](validation.md). Physical drag/rendering acceptance remains pending.
