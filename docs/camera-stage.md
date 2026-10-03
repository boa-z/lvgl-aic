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

Acquisition can block for 60000 ms in the SDK's vin_vb_dq_buf path. A future camera worker must handle transport
outside the LVGL UI thread, marshal publication back to the LVGL owner and
perform source-cache invalidation before CPU access. This adapter does not
provide that worker, convert colorspace, attach a video plane, return held GE
frames early, or reset uncertain hardware.
Widget deletion must request asynchronous shutdown and retain the independent
capture context until the worker returns and all published frame readers finish.

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

Remaining work includes capture-to-publication ownership callbacks, camera widget/worker integration,
video-plane ownership and SDK player backends. The camera gap remains open.
