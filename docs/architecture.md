# Architecture

## Ownership and lifecycle

The application owns its UI loop and pages. Sibling third_party/lvgl-aic owns
platform adaptation; third_party/lvgl provides the unmodified pinned core.
RT-Thread, OSAL, framebuffer/touch and MPP remain SDK interfaces.

The caller calls lv_init(). lv_aic_init() installs the RT tick callback,
creates display, optionally creates touch and registers MPP, then opens GE2D.
Initialization errors unwind acquired resources. Teardown currently runs
touch -> decoder -> GE2D -> display; it is not an exact reverse of init.
The caller serializes LVGL access and stops UI work before deinitializing.
Outstanding MPP readers refuse the entire teardown; close them and retry.
GE opens again on each init cycle.

The display owns the mpp_fb handle, LVGL display and software-rotation buffer.
The physical framebuffer belongs to the BSP and is never freed by the port.
Whole-display rotation uses software even when GE IMAGE transforms are enabled.
Touch uses a worker and locked state; its IRQ callback wakes the worker without
manipulating LVGL objects. Optional polling recovery defaults off.

## Optional backends

MPP accepts JPEG/PNG files via LVGL FS and borrowed RAW/RAW_ALPHA memory.
Streams and MPP handles end after synchronous decode. A session owns CMA or
post-processed heap pixels until its last reader and cache retention end.
The component LRU defaults to 512 KiB / 16 entries, keys source plus decode
options, copies file keys and defers release of invalidated active entries.
Use lv_aic_mpp_cache_drop(source) before changing/releasing sources; it also
drops LVGL header metadata. Generic LVGL image-cache invalidation alone is
insufficient. No core or global allocator/cache hook is replaced. See
[resource ownership and limits](resource-stage.md).

GE2D has its own unit, separate from lv_draw_sw_unit_t. It evaluates
FILL/IMAGE/LAYER with task-local clip/destination data, then synchronously
submits, emits and waits. Failed engine operations mark FAILED, not FINISHED;
never blend in software over a destination the failed submission may have
partially changed. Unsupported work declines or falls back before submission.

FILL blends partial opacity on opaque RGB565/RGB888/XRGB8888 targets and leaves
partial ARGB8888 targets to software. Its executor rechecks address/geometry
and intersects the saved task clip with the layer buffer before cache/DMA work.

IMAGE handles RGB/ARGB/XRGB, alpha, bounded scaling and right-angle rotation
plus scale. LAYER has narrower transforms and needs accessible source memory;
ordinary D13x heap sources use software. No global layer allocator or LVGL
cache handler is replaced. Cache maintenance is region-local.

Native FreeType is owned by upstream lv_init/lv_deinit. Applications own font
instances, fallback chains and all objects/styles referring to them. The optional
SDK FreeType library supplies the rasterizer; no legacy lvgl-ui adapter is used.
Delete font users before deleting fonts, then deinitialize LVGL. The native
glyph cache is count-bounded per cache, not a global byte budget. See
[font ownership](font-stage.md).

See [capabilities](capabilities.md) and [validation](validation.md).

Native GIF is an optional upstream widget selected by AIC_LVGL_USE_GIF. The
application owns its objects, source strings/descriptors and borrowed RAW bytes.
Each live file-backed GIF retains its file until replacement/deletion. Delete
widgets before closing their filesystem/display. Its software-decoded canvas is
separate from MPP's compressed-image CMA/cache lifetime; no legacy SDK GIF hooks
are imported. The manual shell only queues requests for the UI timer to handle.
See [GIF ownership and board gate](gif-stage.md).
