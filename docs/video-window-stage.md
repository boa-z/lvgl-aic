# SDK video window

The SDK aic_video_window is an image subclass that publishes a replacement
`.fake` rectangle with alpha zero. It does not own a camera, player, video
buffer or hardware display layer. The application-owned port now provides
the SDK create, set_size and set_color entry points in
include/lv_aic_video_window.h, adapted to LVGL 9.6.

Enable AIC_LVGL_USE_VIDEO_WINDOW with GE2D and the MPP metadata bridge.
Initialize the port before setting window dimensions; keep the L filesystem
bridge active until widgets and queued draws have been destroyed. The widget
uses the existing GE or CPU replacement handler. A lower video plane is only
visible when the destination/display composition retains ARGB alpha. An
RGB565/RGB888 target cannot create a transparent hole. The application remains
responsible for configuring the lower layer and media device.

Dimensions are restricted to 1..4096 and at most 8M pixels, matching `.fake`
parsing. Invalid dimensions preserve the current source. Color can be set
before dimensions; repeated identical updates preserve the source allocation.
LVGL owns the copied file source and frees it when the widget is deleted.
The port intentionally exposes the widget API, not private instance fields.

Host coverage uses the real MPP metadata callback and L-drive wrapper: 20
create/change/delete cycles, image geometry, color-before-size, idempotent
updates, invalid sizes, style changes and zero MPP/CMA decode allocations.
The GE target WithWidgets profile includes the class and routes the existing
alpha-zero numeric fill probe through an actual video-window source. That
probe checks 90 ARGB pixels and outside sentinels; board execution is NOT_RUN.

Camera capture, player backends and lower-plane ownership remain separate
unimplemented SDK parity requirements. This widget does not close them.
