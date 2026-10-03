# Camera capture lifecycle

The internal compat/lv_aic_vin_session.h API is the first device-side layer of
the SDK camera port. It calls the real mpp_vin2 API for camera discovery/input
format, output negotiation, buffer pool allocation, queue/start, pause/resume,
dequeue/return, stop and close. It accepts SDK NV12, NV16 and YUV400 capture
formats. It is not yet a camera widget or frame-to-LVGL publisher.

Enable AIC_LVGL_USE_VIN only with AIC_MPP_VIN and AIC_DVP_DRV configured by the
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
state reports OPENING/RUNNING/PAUSED/CLOSING/CLOSED/FAULT. This is an internal
transport API, not yet the SDK camera widget's open/start contract.

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

Remaining work includes SDK camera widget binding, video-plane ownership and
SDK player backends. The camera gap remains open.
