# Camera capture lifecycle

The internal compat/lv_aic_vin_session.h API is the first device-side layer of
the SDK camera port. It calls the real mpp_vin2 API for camera discovery/input
format, output negotiation, buffer pool allocation, queue/start, pause/resume,
dequeue/return, stop and close. It accepts SDK NV12, NV16 and YUV400 capture
formats. The session itself is not a camera widget; the worker below publishes frames.

Enable AIC_LVGL_USE_VIN only with AIC_MPP_VIN, AIC_DVP_DRV and AIC_USING_CAMERA configured by the
application. The default test image leaves the camera disabled. The application
must serialize calls and exclusively own the DVP device; the adapter enforces
one local session because SDK DVP state is global, but cannot exclude unrelated
SDK users. Without AIC_DVP_SUPPORT_DEMUX only channel zero is accepted. No
camera model, bus, pinmux or channel mapping is guessed by this module.

Buffer sizes come from negotiated per-plane stride/sizeimage, including padding,
and are checked before the SDK's 32-bit allocation multiplication. Returned
pool counts, plane capacities and addresses are checked before queueing.
Every dequeue owns an index until explicit release. Stop/close refuse while
any index remains held; the caller must first finish all CPU/GE/display reads.
Failed queue-back preserves ownership and can be retried. Failed STREAM_ON is
treated as possibly active (the SDK starts the sensor before DVP success).
Failed STREAM_OFF retains the complete session and allocations for retry.
Do not free its storage on a false close result. After a transport fault,
release held frames and close/reopen; start/resume will not silently recover.

Acquisition can block for 60000 ms in the SDK's vin_vb_dq_buf path. The worker
below handles transport outside the LVGL UI thread. The session layer itself
does not attach a video plane, return held GE frames early or reset hardware.

## Evidence

- Host: 22/22 PASS; new contract uses the actual SDK VIN/DVP-v1 headers and
  ioctl constants with mocked device operations. Coverage includes three
  formats, padded planes, start/stop/restart, pause/resume, exclusive session,
  held frames, duplicate/out-of-range dequeue, malformed buffer metadata,
  setup failures, queue-back failures and STREAM_ON/OFF failures.
- Strict target compile: tools/sdk/check-vin-session.ps1 PASS with real SDK
  headers, Xuantie GCC, D13x E907 double-float ABI and -Wall -Wextra -Werror.
- Object: SDK output/lvgl-vin-session.o, SHA256
  730b5d6f15814007008d97d0da36d19dad024d3d35d97db47035760a1eb92f72.
- This is compile-only evidence. Camera-enabled image linking, device capture,
  DMA/cache, UI publication and physical board acceptance are NOT_RUN.

The borrowed-view adapter in compat/lv_aic_vin_frame.h now maps a held capture
index into the application YUV frame API. It uses the negotiated strides and
reported capacities, rejects released indices/malformed spans and requires an
explicit colorspace. It never touches caches, queues a buffer or infers sensor
colorimetry. NV16 uses a frame-only format tag because LVGL has no native enum.
CPU conversion, image publication and native GE rendering now accept this tag.
Host checks include rejected stale/unheld indices and unchanged output on error.
The target check script also compiles the frame adapter; object SHA256:
1940a78808ad2542a8df33825ae0264f93713ac4b250a30b6525a7d08cd4ed27.

The GE/fonts/GIF/widgets/AICP regression image also builds successfully with
NV16/NV61 conversion/rendering enabled (VIN hardware remains disabled): clean
SDK 62981ee9, component f6923c5, LVGL 80ca777e. Boot/app builds, static/image
checks and manifest PASS. Evidence is in SDK
output/lvgl-evidence/ge2d-fonts-gif-widgets-aicp; image SHA256:
b80a887afc58eb866823976379a9211ee74170900a6e7456d1faa304c7d2ec12.
This regression image is not evidence of a linked or running VIN device.

## Background capture and publication

include/lv_aic_camera_capture.h now provides a UI-owner API backed by an SDK
OSAL worker (8 KiB stack, priority 20). Open starts capture asynchronously;
state reports OPENING/READY/RUNNING/PAUSED/CLOSING/CLOSED/FAULT. Prepare
configures the pool without queueing buffers or starting the sensor. Start is
an idempotent asynchronous request, accepted during opening or after READY;
the existing open convenience API still requests immediate start. Close while
prepared releases the pool without STREAM_ON or dequeue. Pause requests made
before start are applied before the first dequeue. This is a transport API,
not yet the SDK camera widget binding.

The worker exclusively performs VIN operations and invalidates completed DMA
planes before publishing their metadata. Poll creates immutable YUV images
on the LVGL owner thread. Only the latest unpublished frame is retained;
published images retain their buffer index until the image owner AND all
decoder/GE readers release it. The release callback only marks a pending
return under the mailbox mutex; the worker performs Q_BUF. No LVGL allocation
or registry API runs in the worker. A single active capture prevents competing
workers from entering SDK global DVP state.

Close is a nonblocking request. Destroy returns false until every published
image is released and the worker successfully closes VIN. Failure to return a
frame or stop capture retains the entire context and retries on the worker.
The application must retain the handle and retry destroy; killing the worker
or freeing the context during a 60-second dequeue wait is unsupported. A GE
quarantine can intentionally keep this context alive until reboot.

Host: 23/23 PASS. The capture test runs a real pthread worker against SDK-ABI
mocks, with controlled blocking dequeue, pause/resume, latest-frame dropping,
delayed image readers, thread/device startup failures, failed publication,
queue-back failure and repeated STREAM_OFF failures. It asserts all VIN/cache
calls occur on the worker and buffers are never freed while held. Cache calls
and physical frame addresses are mocked; this is not real DMA evidence.

The compile-only script covers session, frame adapter and capture worker with
real SDK headers/OSAL and D13x double-float ABI. Component code uses
-Wall -Wextra -Werror; vendor headers are system includes because SDK OSAL
contains existing signedness warnings. Worker object SHA256:
d4fd9b7c62832763ef1866310ca813703eae48d4eba884917fa9b6b3cd861b68.
Camera-enabled image linking and hardware execution remain NOT_RUN.

The widget binding added below supersedes the transport-only status above.
Video-plane ownership and SDK player backends remain open.

## Prepared capture stage

Host 23/23 PASS and real SDK compile-only checks PASS after splitting prepare
and start. Added concurrent tests verify no queue/STREAM_ON/dequeue in READY,
close-before-start, rejected start after close, early repeated start requests,
and pause-before-first-dequeue. The capture object SHA256 for this stage is
72d8c6ae77c142f83b526b46c37ccdd9e10f07ba9bc2d3b17e674864cb1aebed.
No newer camera-enabled image or board evidence is implied.

Channel terminology matters: the transport parameter is a VIN queue index.
The SDK widget's set_channel calls camera_set_channel(camera_dev, ch), a sensor
input selector; its VIN1=0 and VIN2=2 constants are not queue indices. Sensor
selection needs a separate worker-side operation and must not index vin_buf
with those constants. The separate selection operation is implemented below.

## Camera image widget / 摄像头图像控件

启用 AIC_LVGL_USE_CAMERA（依赖 VIN）后，include/lv_aic_camera.h 提供创建、格式、
打开、开始、暂停、恢复、停止及关闭接口。它是 LVGL image 子类，借助现有 YUV
发布器走 GE 或软件合成；传感器输入切换见下节，尚未实现 SDK 独立视频层或 barcode。

应用必须先初始化 YUV decoder，并通过 configure 明确设备名、VIN 队列索引和
传感器色彩空间。不会默认猜测 BT.601/BT.709。格式默认 NV16，支持 NV12/YUV400。
控件独占自己的 image source，请勿通过 lv_image_set_src 外部替换或共享该 source。
所有操作需在 LVGL owner 上、绘制回调之外执行。

与 SDK 同步 open 的差异：返回 LV_RESULT_OK 仅表示请求已接受；实际完成情况由
get_state 和 LV_EVENT_VALUE_CHANGED 通知。准备完成为 READY，start 启动采集；
stop 异步释放采集设备和帧，在全部读者退出后进入 STOPPED，再 start 将重新打开
设备并启动。STOPPING 期间拒绝 start；close 完成进入 CLOSED，需重新 open。
FAULT 保留为可观察状态，须显式 close/stop 清理后再打开。中间快速转换的状态可能
在 20 ms UI 轮询之间被合并，回调应读取当前状态而非假设收到每个中间状态。

删除控件只请求后台退出，独立 binding 与预分配 timer 继续管理帧引用。待全部
显示的排队绘制任务清空后才释放 image owner，再等待 worker 退出。已打开的 GE/
decoder 读者继续持有原始 VIN 缓冲区；GE quarantine 可使清理延后至重启。不会在
UI 内等待 VIN 取帧或强杀 worker。关闭 LVGL 前须先删除所有 camera 控件，继续
执行 timer handler，直到 lv_aic_camera_pending_cleanup() 为零，再释放 decoder。

English contract: application-owned image widget; asynchronous request/status
semantics, explicit colorimetry, complete device reopen on restart, and deferred
orphan cleanup. The state event may delete the widget. Keep UI timers alive
until deleted bindings drain. This is not binary/source compatibility for the
SDK's public struct, synchronous return semantics, video-plane,
or optional barcode APIs.

Evidence for this stage:
- Host **24/24 PASS**. Real LVGL widget/decoder/software renderer checks 20
  alternating black/white NV16 frames, pause/resume, stop/restart, a held reader
  across stop, pending-task deferral, delayed worker completion, deletion inside
  state notification, fault detachment, close-before-start and unopened deletion.
- The concurrent capture test also binds the real widget to the real pthread
  capture worker and mocked SDK VIN. Deletion returns while dequeue is blocked;
  the orphan timer releases storage only after worker completion.
- SDK E907 compile-only checks include the widget with -Wall -Wextra -Werror.
  Widget object SHA256:
  1b7ca2d18e78b25cbd07a5311d79ec9133c0f9085d876f0ac639c1347e23895d.
- Camera-enabled image link, real sensor/DMA/cache, panel rendering and physical
  camera acceptance are **NOT_RUN**. The older VIN-disabled regression image
  above does not include or validate this widget.

## Sensor input selection / 传感器输入选择

新增 SDK 风格 lv_aic_camera_set_channel/get_channel 及原有 VIN1=0、VIN2=2
常量；这些值原样传给 camera_set_channel(camera_dev, input)，绝不修改 VIN 队列
索引。0..3 是 SDK 控件允许的原始选择范围，不代表每个传感器都有四路输入，更不
推断板级接线。Kconfig 现在明确要求 AIC_USING_CAMERA，防止漏编 camera 服务。

set_channel 返回 0 仅表示异步请求接受，-1 表示未打开、关闭中、越界或有待处理
请求。get_channel_status 返回请求序号、请求值、驱动最后确认值及状态：NONE、
PENDING、APPLIED、FAILED、CANCELLED。没有驱动确认时 get_channel 返回 UINT32_MAX，
不会凭空报告通道 0。输入状态与采集状态变化共用 LV_EVENT_VALUE_CHANGED。

worker 串行执行传感器切换和 VIN 操作，不持 mailbox 锁调用驱动。READY、RUNNING
及 PAUSED 均可提交请求；SDK 取帧阻塞时请求需等待其返回（可能 60 秒）。关闭时
取消尚未执行的请求，但已进入驱动的操作必须执行完才能回收；其结果仍可查询。
失败可能意味着硬件被部分修改，因此原子发布 FAILED/FAULT、清除确认值并关闭
采集，不继续发布带有不确定输入状态的帧。stop/start 重新打开设备后重放最后
接受的输入选择；configure 更改设备配置时清除此选择。

切换请求丢弃未发布的 READY 帧，并暂停新的 publication，直至驱动操作完成；已有
image/GE/decoder 引用保持有效。APPLIED 仅表示驱动返回成功，既不证明物理接线
也不保证下一个 dequeue 或当前显示帧已经来自新通道。SDK 没有提供逐帧输入标识。

Host 24/24 PASS after adding real-worker tests for selector 2 with queue 0,
READY selection, rejected invalid/busy requests, blocked control with responsive
UI mailbox, in-flight close, cancellation during blocked dequeue, retained image
readers across switching, and driver failure shutdown. Widget tests cover the
SDK aliases, unknown/confirmed values, restart reapplication and deletion from
an input-only status notification. SDK ioctl, sensor and DMA effects are mocked.
Real SDK header/OSAL/D13x strict compilation also passes; camera-enabled link
and hardware acceptance remain NOT_RUN. Final object SHA256 values:
- capture: 9e136350aa9e180fb0367dfd4ea82798080240d267cbd8b80d7ff1111893fd2a
- widget: eebb8cb1444e930ce5adea0397ba3288b6459daaef4a8a9d375fa4593bcfedec

## RT-Thread configuration-aware camera compile (2026-10-04)

The VIN checker now enters the real RT-Thread configuration path, matching the
SDK libc definitions and audio/FreeType include paths used by the combined
profile. All four adapter modules pass E907 -Wall -Wextra -Werror. Defined-symbol
checks confirm VIN open/close, capture open/close and camera create/channel APIs
exist in the objects, rather than accepting disabled/empty translation units.

This remains compile-only evidence. The inspected D50T board/smoke defconfigs
provide no explicit camera sensor, I2C channel or reset selection. Enabling the
SDK camera choice would silently select its OV5640 default; that is not evidence
of the user's hardware. A camera-enabled full-image profile and physical capture
remain NOT_RUN pending a documented board camera configuration. Other port work
can continue independently; no camera hardware is opened by this checker.

## Explicit camera video-plane binding (2026-10-04)

`lv_aic_camera_set_video_plane(obj, enabled, rotation_budget)` now opts a
closed camera into the shared native video-plane path. It requires an ARGB8888
UI and initialized component MPP/.fake decoder support. It uses the same checked rectangle, clipping, offsets, right-angle rotation
and bounded CMA rules as player. The internal plane-window implementation is
shared; unsupported geometry reports FAULT rather than silently showing a
different composition. Default camera rendering remains the native image path.

Frame replacement retains the old plane reader until the new scanout completes.
Hidden cameras hide scanout, paused cameras preserve their last frame, and
stop/close/deletion waits for plane shutdown before retiring the camera image
and capture worker. A plane failure closes capture and reports FAULT; failed
plane close retains the binding and VIN readers for retry/reboot as appropriate.

Host coverage uses actual immutable YUV publication with mocked capture/plane
operations: replacements, hide/show, pause, deferred deletion, failed present
and failed close. Shared player geometry regression remains active. This is
not sensor, DE, DMA or panel evidence. `check-vin-session.ps1 -WithVideoPlane`
compiles the enabled camera and shared window against D13x target headers and
checks actual camera references to window present/close. Full camera-enabled
image linking still requires application-supplied sensor/board configuration;
no bus or sensor selection is inferred. Barcode support remains absent.

Stage evidence: **56/56 host PASS**; strict enabled-camera/window D13x compile
**PASS**, including actual camera references to window presentation and close.
Camera widget object SHA256 `1c3a3d7c91ed943a81141f9a7563b740092cf5828083f096be2f8bdfdf2a51b7`.
The full GE/fonts/GIF/widgets/AICP/player/APNG regression boot/app/static/image/
manifest gates **PASS**, with camera disabled. Clean component
`10f146c239060f2d4b48e390ddf5f1c7a9a51619`, SDK
`527b5f006f9abd98437cc8a0f087ad539fb449e7`. Regression image SHA256
`27fc9175b47abb0ff9d01406fa7c0274c8d424beea195faaeaa8947dce5d7a11`.
Camera-enabled final linking and all physical capture/scanout remain **NOT_RUN**.
