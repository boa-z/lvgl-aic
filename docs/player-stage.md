# SDK player port / SDK 播放器移植

## Worker-side session foundation

compat/lv_aic_player_session.h and port/lv_aic_player_session.c provide a
serialized adapter to the actual SDK aic_player API. Enable
AIC_LVGL_USE_PLAYER_SESSION only with AIC_MPP_PLAYER_INTERFACE and
AIC_MPP_PLAYER_VIDEO_EXT_RENDER. The latter is also enforced at compile time:
an SDK-owned video renderer must not compete with the application for frames.
The application still owns audio/codec configuration. No SDK core is changed.

会话必须零初始化一次，随后只能由同一串行 worker 使用；prepare、start、取帧和
stop 都可能阻塞，不能直接从 LVGL UI 调用。本阶段尚未创建播放 worker 或控件。
URI 是 SDK 原生路径，限制为 SDK 的 128 字节（含终止符），不自动解释 LVGL 盘符。

基础能力：打开并同步准备、媒体信息、开始、幂等暂停/恢复、停止后重新准备并启动、
seek、音量与播放时间读写，以及借出/归还解码帧。SDK start 本身开始播放；SDK 的
重复 pause 会恢复播放，因此会话层必须去重，不能直接转发每次 pause 请求。
SDK stop 销毁解码器，重新 start 会重新 set_uri/prepare，而不是使用旧帧池。

最多记录八个并发借出帧，这是适配层上限而非声明硬件有八个帧缓冲。借出帧元数据
不可修改，仅在租约归还前有效。64 位单调租约编号跨槽位复用、stop 和 reopen 保留，
防止旧回调归还新帧；编号耗尽时拒绝继续取帧。归还失败保留租约，允许重试。
有任何借出帧时拒绝 stop、close 和 seek；调用者须先完成所有 UI/GE/decoder 读取。
检测到仍持有的 decoder frame ID 再次借出时，所有权已经不可信，整个会话保留到
重启，既不猜测应归还几次，也不销毁可能仍被读取的帧池。

控制失败后禁止继续采集或恢复播放，仅允许归还帧、stop/close。open 失败时尝试
清理，但清理失败可能留下资源；必须以 close 返回 true 为最终释放依据。getter
失败不修改输出值。SDK 当前 stop/destroy 返回成功，但主机测试也覆盖错误返回时
保留资源的适配层行为；这不证明所有 SDK 内部错误都会传播出来。

## Bounded MPP frame import / 有界帧转换

`common/lv_aic_yuv_mpp.h` now provides `lv_aic_yuv_from_mpp` for ten YUV
formats, with explicit colorimetry and independently verified per-plane
capacities. It applies aligned planar, semiplanar and packed crops, preserving
stride and reducing capacity by the actual plane offset. Invalid metadata,
FD-backed frames, physical-address wrap and unaligned chroma origins fail
without modifying the output. It performs no retain/release or cache operation.

SDK mpp_buf 没有分配容量字段，不能用 stride × height 猜测实际分配容量。
调用方必须从分配器取得容量，保证物理地址可由 CPU 直接访问，并保持帧租约有效。
转换先验证原图可见跨度，再验证裁剪；颜色空间由调用方明确传入，不猜测 SDK flags。
靠近右下边界的裁剪可能满足 CPU 可读范围，却缺少 GE 要求的完整 padding 行；
此时现有 to_mpp 拒绝 GE，不能扩大容量绕过检查。下述分配器提供真实容量来源。

Host contract covers all ten formats and four color spaces, crop offsets and
remaining capacities, chroma alignment, invalid bounds/FD/format/colorimetry,
transactional rejection and CPU-valid/GE-invalid bottom-right cropping.

## Application-owned CMA allocator / 应用侧帧分配器

`compat/lv_aic_player_allocator.h` and `port/lv_aic_player_allocator.c` implement
SDK external allocation for all 14 formats accepted by its frame manager
(six planar/semiplanar/monochrome YUV and eight RGB byte orders). NV16/NV61
and packed YUV are deliberately not allocator outputs: SDK add_dmabuf does
not register them, even though the separate frame conversion supports them.

分配器管理最多 32 帧及显式总字节预算，记录每个 plane 实际 CMA 容量（含 32 字节
cache-line 尾部）。SDK callback 的 width 是字节 stride，不能作为像素宽度。
每帧分配失败回滚已分配 plane；超预算、无空槽、非法尺寸/stride 或物理地址拒绝。
H.264 max-size 模式可以缩小输出尺寸并改变 pitch；获取容量时核实所有 plane
地址、格式及新布局边界，不要求仍等于初次分配时的 pitch。

`lv_aic_player_session_allocator` 在 start 前安装外部分配器与 1..8 个额外 decoder
缓冲；数量不是总帧数。控制部分失败会 fault 会话，调用方仍须保留分配器直到
close 成功。SDK 的 close_allocator 在 decoder stop 时发生，但播放器可能重启，
故 callback 不释放上下文，由应用在 SDK 完全脱离后显式 destroy。

Usage ordering (worker side, use the bridge below for UI/GE publication):

1. Create allocator with an application-selected CMA budget; open session;
   install `lv_aic_player_allocator_sdk(allocator)` before session start.
2. Acquire an SDK session frame lease, then allocator pin/capacities. Import
   the YUV view using explicit colorimetry and those capacities.
3. Keep both leases until all readers finish. Release allocator pin **before**
   SDK session release/put_frame; never allow decoder reuse while readers run.
4. Close session before destroying allocator. Destroy refuses live/retired
   allocations. A quarantined GE frame must retain both leases until reboot.

Allocator callbacks and pins share an OSAL mutex. Allocation cleans/invalidates
cache before VE ownership; acquiring a decoded frame invalidates without
cleaning. SDK free retires a pinned frame, retaining CMA and budget until final
pin release. Monotonic tickets prevent stale release across slot reuse. Host
mutex stubs check locking discipline; they do not prove target concurrency or
DMA coherency. The allocator does not itself publish frames or call LVGL.

## Immutable frame publication bridge / 不可变帧发布桥

`compat/lv_aic_player_frames.h` and `port/lv_aic_player_frames.c` connect real
SDK session leases, allocator pins and the existing YUV image registry.
Create/poll/close/destroy run on the LVGL owner; submit/drain run on one
serialized playback worker. SDK operations never run in image release callbacks.

worker 先按播放时序获取到期帧，再 submit 租约；成功后桥接层负责 pin 和 SDK
租约，失败则原 worker 仍须归还。重复提交已持有的租约幂等。新帧替换尚未发布帧，
已发布图像保持不可变。poll 返回新图像及 PTS，失败不修改 PTS；未初始化 decoder
或发布失败的帧也统一交给 drain 归还。

UI 销毁图像只标记归还，所有原生 GE/decoder 读者完成后，worker drain 先释放
allocator pin，再调用 SDK put_frame。put 失败保留 SDK 租约，可重试且不会重复
释放 pin；不可信的 pin 释放失败则隔离邮箱，保留资源。close 拒绝新发布并取消
未发布帧，已存在的读者仍阻止 teardown。销毁桥前必须让 worker 停止使用它；
仅看到 idle 不能授权与 worker 并发释放。GE 隔离读者可能持续到重启，不能强拆。

This bridge does not implement a playback scheduler: feeding raw decoded
frames as fast as possible would skip through a movie. PTS pacing, audio/video
clock selection, pause/seek/repeat and source replacement belong to the pending
background player. SDK PLAY_END also represents some decoder errors, so it
must not be treated as proof of clean EOS. Native RGB publication is now supported as described below; error-marked frames
still fail without consuming their lease.

## Media clock and event mailbox / 媒体时钟与事件邮箱

`compat/lv_aic_player_clock.h` / `common/lv_aic_player_clock.c` provide a
worker-owned microsecond timeline: explicit source/seek reset, audio-reference
sync, idempotent pause/resume, position and wait/present/drop decisions.
Future frames never publish early. Late tolerance is supplied by the caller;
there is no guessed frame rate, discontinuity repair or playback-speed policy.
Negative audio preroll PTS is supported. Backward monotonic time and arithmetic
overflow fail transactionally, preserving clock state and output values.

`compat/lv_aic_player_events.h` / `port/lv_aic_player_events.c` register a
small SDK callback that records timestamped audio PTS, terminal and format
notifications under an independent OSAL mutex. It never calls back into the
SDK or LVGL. Snapshot readers see a coherent pair of 64-bit audio PTS and wall
time. Registration failure keeps the potentially installed callback alive;
mailbox destruction refuses until the associated SDK session has been closed.
Create a fresh mailbox for a new session; terminal/format failure flags are
sticky, not an implicitly reset seek epoch.

SDK 的纯视频 get_play_time 依赖内部 video renderer 的 PTS；外部渲染模式不能靠
它自动走时。带音频模式优先使用 PLAY_TIME 回调：其值是音频帧 PTS 减去音频设备
缓存时长，可能为负。回调的两个 32 位片段按无符号位模式重组，不从另一线程直接
读取 SDK 未同步的 64 位字段。后台 worker 仍需选择主时钟并把样本接入 timeline，
不能把单元测试的 clock helper 当成已完成音视频同步。

SDK PLAY_END 也用于解码器错误和资源不足，邮箱只记录 terminal 通知，不声明
正常 EOS。帧错误、EOS 帧标志与 terminal 事件必须由后续播放状态机分别处理。
另外 D13x 的 MJPEG external-render 路径按 framebuffer 格式请求 RGB 输出；
下述 RGB bridge 已覆盖原生 RGB565/RGB888/ARGB8888 发布；真实 MJPEG 播放仍待集成验证。
ARGB1555 与非原生 BGR/RGBA 字节顺序不在发布范围，不能冒充相近的 LVGL 格式。

## Native RGB publication and GE lifetime / RGB 发布与 GE 生命周期

`include/lv_aic_rgb_image.h` provides immutable native RGB565/RGB888/XRGB8888/
straight-ARGB8888 frames. `common/lv_aic_rgb_mpp.h` imports bounded physical
MPP frames with crop and verified allocator capacity, rejecting address wrap,
invalid source spans and channel orders without a native LVGL representation.

Decoder 完整 padding 行可直接借用；裁剪尾部缺 padding、像素/行未对齐、请求
stride 规范化或 alpha 预乘时创建只读副本，不修改 producer 像素。每图像最多
四种 lazy 解码视图，随图像和全部 reader 的最终释放统一回收，不进入通用 cache。
播放器 `poll_image` 返回 RGB/YUV 通用 owner handle（含 PTS），由 source/destroy
配套使用。原 YUV-only poll 不会丢弃待发布 RGB 帧；混合媒体须初始化两个 decoder。

GE RGB executor 在 decoder open/close 外再持有一个图像租约，覆盖源和所有副本。
bitblt/rotate/emit/sync 失败时保留租约直到重启；dispatcher 保留 IN_PROGRESS
任务和目标 layer，停止后续绘制，不做软件重放。这样 decoder close 不会导致
播放器归还仍可能被 DMA 使用的缓冲。普通和 tiled 路径采用相同保护。

## Evidence

- Host **30/30 PASS**, actual SDK player/mpp_frame declarations, mocked player
  operations. New checks cover setup/metadata/start/pause/resume/seek failures,
  idempotent pause, frame saturation, stale tickets across reopen, transactional
  getters, held-frame stop/seek/close refusal, return retry, stop/destroy retry,
  restart preparation and duplicate-ID quarantine.
- New allocator host contract: all 14 SDK formats, each plane allocation
  failure/rollback, 32-frame limit, total budget including retired pins,
  metadata rejection, H264 smaller layout, deferred free, stale tickets,
  32-bit address/alignment guards and actual bounded YUV crop import. Session
  tests cover installation order and partially applied control failures.
- New frame-bridge contract uses a real pthread worker, actual session and
  allocator code, real LVGL publication/native reader leases, and mocked SDK
  playback/CMA. Covers publication failure, duplicate submit, latest-frame
  replacement, error rejection, delayed native reader release, failed put retry,
  shutdown with readers, unpublished close and close during worker cache handoff.
  SDK calls/cache operations assert worker-thread ownership. No physical pixel
  memory is dereferenced by this test; CPU conversion has separate coverage.
- Clock contract: exact due/late boundaries, pause freeze/idempotence/resume,
  negative audio PTS, explicit reset, stale sample rejection and signed/unsigned
  64-bit limits. Event contract: real concurrent pthread callbacks/snapshots,
  10,000 alternating signed audio timestamps, sticky terminal/format flags,
  unknown events, partial registration failure and destruction refusal.
- RGB contracts: four native MPP format/crop mappings; truncated/FD/address
  rejection; shared borrowed decodes, immutable premultiply/stride snapshots,
  minimal last-row capacity and unaligned-row fallback; retired-open refusal;
  worker RGB publication and delayed put. GE tests cover normal/tiled paths,
  borrowed/copied storage and injected bitblt/emit/sync failures; dispatcher
  tests keep both RGB and YUV DMA-fault tasks in progress. These are mocked GE
  tests, not physical DMA validation.
- `tools/sdk/check-player-session.ps1`: D13x E907 double-float ABI, real SDK
  player/allocator/frame bridge/events/clock and RT-Thread pthread headers, -Wall -Wextra -Werror: PASS.
  The script uses the SDK's Newlib/POSIX defines from compiler/pthread
  SConscript, including _POSIX_C_SOURCE=1 and _SYS__PTHREADTYPES_H_. It does not
  change SDK configuration or substitute host pthread declarations for target.
- SDK output/lvgl-player-session.o SHA256:
  812cec2ad03ed380e9a8d5fa7a664481918c3f6adce9a34c3f8ee94e66272d49.
- SDK output/lvgl-player-allocator.o SHA256:
  a64904bcfccb70d3ebf2359637f903f4b8319e87be928011f42b4742dd1b4682.
- SDK output/lvgl-player-frames.o SHA256:
  0aa18367dc69a0dd8b92ca16b239ffdbf692bfc4380831da63ee0f4e6ab2b509.
- SDK output/lvgl-player-events.o SHA256:
  1de7123a1f3a67316156db124d15ca2a2dd314af42a47601ac0be2fb66b7cf1e.
- SDK output/lvgl-player-clock.o SHA256:
  d3ff0e11058f87bde95e23c925a09f5619321d0fa38686a62ec19866a5ab130b.
- SDK output/lvgl-rgb-image.o SHA256:
  d33d66747ff957f802494886b4273e5df6bcd95ba7ab42581831ea2b9c0a5767.
- SDK output/lvgl-rgb-mpp.o SHA256:
  6532233780565f09b09f7d1ecce6ddbef32fd744c176ae2c9fb5999a49f63d8b.
- Media-enabled image linking, real demux/codec/audio playback, physical DMA
  lifetime and board execution: **NOT_RUN**. The current GE image has no player.

## Remaining SDK parity

Integrate background command/event handling (including EOS/error/seek), PTS
pacing and audio/video synchronization using the clock and event primitives, player widget controls, source replacement, repeat/rate behavior, slave and group
lifetimes, APNG backend, and explicit video-plane composition/ownership.
Do not report this internal session as a complete player widget or as tested
hardware decoding. All physical verification remains deferred.
