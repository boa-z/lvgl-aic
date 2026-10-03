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

Acquisition can block in the SDK. A future camera worker must handle transport
outside the LVGL UI thread, marshal publication back to the LVGL owner and
perform source-cache invalidation before CPU access. This adapter does not
provide that worker, convert colorspace, attach a video plane, return held GE
frames early, or reset uncertain hardware.

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

Remaining work includes NV16 frame publication, camera widget/worker integration,
video-plane ownership and SDK player backends. The camera gap remains open.
