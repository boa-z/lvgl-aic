# SDK player port / SDK 播放器移植

## Worker-side session foundation

compat/lv_aic_player_session.h and port/lv_aic_player_session.c provide a
serialized adapter to the actual SDK aic_player API. Enable
AIC_LVGL_USE_PLAYER_SESSION only with AIC_MPP_PLAYER_INTERFACE and
AIC_MPP_PLAYER_VIDEO_EXT_RENDER. The latter is also enforced at compile time:
an SDK-owned video renderer must not compete with the application for frames.
The application still owns audio/codec configuration. No SDK core is changed.

会话必须零初始化一次，随后只能由同一串行 worker 使用；prepare、start、取帧和
stop 都可能阻塞，不能直接从 LVGL UI 调用。下述 playback worker 已接入这些接口；
原生播放器 widget 已接入，接口与验证边界见下文。
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

The bridge does not implement a playback scheduler. The integrated worker
below consumes SDK external-render get_frame, which already performs PTS
wait/drop and audio/video synchronization. Do not add a second sleep/drop
scheduler. Seek, guarded repeat and source replacement are integrated below. SDK PLAY_END also represents some decoder errors, so it
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

进一步核对 SDK mm_vdec_component.c 后修正早期判断：external-render 的
mm_vdec_get_buffer 已执行解码、等待/丢帧和音视频同步，并发送视频 PTS。它可能
阻塞；不能把它误当无时序的原始解码队列，也不能在其后叠加第二套时钟延时。
当前 worker 直接采用 SDK 的同步结果；clock helper 保留为独立算法，并未作为
当前 SDK 的额外调度器。纯视频状态取已接收帧的 PTS；带音频取 PLAY_TIME 回调，
其值为音频 PTS 减设备缓存时长，可能为负。两个 32 位片段安全重组，不读取 SDK
被其他线程更新的未同步 64 位字段。真实音视频同步质量仍需上板验证。

SDK PLAY_END 也用于解码器错误和资源不足，邮箱只记录 terminal 通知，不声明
正常 EOS。worker 分别保留 video_eos 和 sdk_terminal，FRAME_FLAG_ERROR 则进入 FAULT。
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

## Background playback worker / 后台播放状态机

`include/lv_aic_player_playback.h` exposes UI-owner prepare/start/pause/volume/
poll/status/close/destroy APIs. `port/lv_aic_player_playback.c` uses one exclusive
OSAL worker (8 KiB stack, priority 20), owning the SDK session, event mailbox,
CMA allocator and frame return. LVGL publication/destruction remains on the UI
thread via `include/lv_aic_player_image.h`; no SDK types enter that public handle.

准备不开始播放；可以在 OPENING 时排队 start/pause/volume。状态依次报告
OPENING、READY、PLAYING/PAUSED、TERMINAL 或 FAULT，close 为 CLOSING/CLOSED。
选项要求显式 CMA 字节预算、2..8 个额外 decoder 帧和 YUV 色彩空间；同一数目
限制应用同时借出的帧，避免把 decoder 的整个帧池占满。最新未发布帧可替换，
已发布 RGB/YUV 读者仍独立持有租约。frames_received/frames_queued 仅记录
接收与邮箱提交，不能作为面板显示或播放流畅性的证明。

PLAY_END 或纯视频 EOS 标记进入 TERMINAL，并保留最后图像供 UI 取出；它不是
正常播完的认证。终止状态不接受直接 start/pause，当前重播/换源流程为
close → destroy → new prepare。状态中的 position_us 是最新视频帧或音频回调
样本，不是额外外推的播放时钟。prepare/start/get/put/pause/stop 的 SDK 调用
全部留在 worker，UI close 只发送请求，不能中断正在进行的 SDK 阻塞调用。

关闭会停止新发布、归还未发布帧，等全部 decoder/GE 读者释放，重试失败的归还，
再销毁 SDK 和事件邮箱。只有 finished 后 destroy 才尝试释放桥和 CMA allocator；
仍有 DMA 隔离租约时永久保留到重启，不能强杀 worker。启动、事件注册、预算分配、
帧错误和控制失败进入可观察的 FAULT，清理成功后仍保留 FAULT 状态供 UI 读取。
调用方必须先完成排队 draw task 再销毁图像，随后定期重试 destroy。

## Evidence

- Host **31/31 PASS**, actual SDK player/mpp_frame declarations, mocked player
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
- Playback integration contract runs the actual worker/session/allocator/
  events/RGB-YUV bridge with real pthread synchronization and mocked SDK. It
  covers thread creation failure, prepare without start, early/idempotent
  pause, volume, exclusive instance, blocked-get close, image-reader delayed
  shutdown, failed-put retry, terminal-with/without EOS, pause racing final
  frame, frame errors, callback/start/CMA-budget failure and audio-only signed
  position. All blocking SDK calls assert worker identity. SDK decode timing,
  codec output pixels, real audio and target concurrency remain unverified.
- `tools/sdk/check-player-session.ps1`: D13x E907 double-float ABI, real SDK
  player/allocator/frame bridge/events/clock and RT-Thread pthread headers, -Wall -Wextra -Werror: PASS.
  The script uses the SDK's Newlib/POSIX defines from compiler/pthread
  SConscript, including _POSIX_C_SOURCE=1 and _SYS__PTHREADTYPES_H_. It does not
  change SDK configuration or substitute host pthread declarations for target.
- SDK output/lvgl-player-playback.o SHA256:
  5e7013dc45439f43ca0b25808c0fc3c2514c4c86b08e8d1b48222e064915dba3.
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
- Media-enabled image linking: **PASS**, see the media link evidence below.
  Real demux/codec/audio playback, physical DMA lifetime and board execution:
  **NOT_RUN**. The earlier GE-only image has no player.

## GE regression image after RGB integration

`build.ps1 -Phase ge2d -WithFonts -WithGif -WithWidgets -WithAicp -Jobs 8`:
boot/app builds, static check, image check and provenance manifest **PASS**.
Clean sources: SDK `17277cfb`, component `5045856`, LVGL `80ca777e`.
The rebuild explicitly checks YUV span-validation return values before overlap
arithmetic, eliminating the component's optimizer uninitialized-span warning.
SDK tooling still reports its existing short-version/pywin32 environment warnings.

- Evidence directory: SDK `output/lvgl-evidence/ge2d-fonts-gif-widgets-aicp`.
- Image: `images/d13x_D50T-2-Lite_page_2k_block_128k_v1.0.0.img`.
- SHA256: `6b0cb4feed18ffe4c2832a0661625e93b40c1109d46feac7b4cd3e112196c279`.
- This replaces that profile's previous image/evidence. It verifies target GE
  integration/linking, not player execution: player/VIN remain disabled and
  physical board validation is **NOT_RUN**. No flashing was performed.

## Remaining SDK parity

Implement backend-specific rate behavior and multi-player group
lifetimes, APNG backend, and explicit video-plane composition/ownership.
Do not report the current widget as complete SDK player parity or as tested
hardware decoding. All physical verification remains deferred.

## Native player widget (2026-10-03)

`AIC_LVGL_USE_PLAYER` adds `lv_aic_player.h`, an LVGL image subclass using the
asynchronous playback worker. Configure an explicit CMA budget, extra-frame
limit and colorimetry, initialize both image decoders, then set the SDK URI.
Preparation does not auto-start. Supports start/pause/resume, volume,
stop/close, manual terminal replay and deferred source replacement (latest
queued URI wins). Volume persists when reopening. Use native image transforms;
do not replace its image source with `lv_image_set_src`.

Deletion keeps an orphan cleanup timer until queued draws, immutable frame
readers and the worker finish. Run LVGL timers until pending_cleanup is zero
before lv_deinit, and close/delete live widgets too. GE fault quarantine must
never be force-released. VALUE_CHANGED reports state/applied volume; callbacks
can delete the widget or replace its source. TERMINAL remains an SDK terminal
notification, not proof of clean EOF. Seek and guarded auto-restart are implemented in the following stages; backend-specific rate and video-plane output
remain open.

Host regression: **32/32 PASS**. The widget contract uses real LVGL/decoders and
mock playback, checks displayed RGB/YUV pixels, reader-delayed replacement,
latest-source selection, volume persistence, terminal replay, stop/restart,
queued-draw deletion, blocked worker exit, callback deletion/replacement and
fault cleanup. The separate worker contract exercises real worker code with
mock SDK. Strict E907 widget compilation: **PASS**; output/lvgl-player.o SHA256
`8e14df2e234d1babe7f24ea0a386532be7927118d4168c5f40caa28e63942d9d`.
Media-enabled firmware linking subsequently passed; physical playback: **NOT_RUN**.

## Media link profile

`tools/sdk/build.ps1 -Phase ge2d -WithFonts -WithGif -WithWidgets -WithAicp -WithPlayer -Jobs 8`
enables the SDK external-render player, H264 and application player widget in
the isolated smoke build. The profile retains widget API roots so link-time
GC cannot silently discard the worker/allocator/bridge/SDK dependency closure.
It validates player Kconfig/header values and live symbols in the final map.
It does not auto-play or provide runtime media acceptance. Evidence goes to a
separate `ge2d-fonts-gif-widgets-aicp-player` directory.

LV_USE_IMAGE is an application C configuration, not an SDK Kconfig symbol.
The widget enforces that dependency during C compilation; referencing it in
Kconfig would silently disable this application-owned widget.

## Media link evidence (2026-10-03)

Standard `-WithPlayer` build above: boot/app build, static map/config check,
image check and manifest **PASS**. Host **32/32 PASS**. Strict E907 compilation
also passes with the audio driver enabled. No flashing or media execution.

- Clean source manifest: SDK `99ba57b7`, lvgl-aic `022ab0b`, LVGL `80ca777e`.
- Evidence: SDK `output/lvgl-evidence/ge2d-fonts-gif-widgets-aicp-player`.
- Image: `images/d13x_D50T-2-Lite_page_2k_block_128k_v1.0.0.img`.
- SHA256: `09364d00b078517b83544f5545af170272cb6df8e9624960e8cdb15b0c15a74b`.
- ELF SHA256: `8d78b326a4865ef2d4352100f225b45e878fdeaff28df57ecba0c3b46a97e45d`.
- H264/player external render enabled; no codec file is opened automatically.
  This profile retains widget APIs to validate their real SDK link closure.

The link gate exposed adapter feature guards evaluated before application
configuration was loaded. Player and VIN adapters now explicitly include the
feature configuration before their guards; SCons also tracks this dependency.
No SDK core modification was needed. This repairs VIN build selection but does
not certify camera configuration or physical capture.

Worker seek and guarded auto-restart are implemented below. Backend-specific rate, multi-player group lifetime and APNG/video-plane integration need
separate implementation and evidence. The new native API does not claim binary
or full source compatibility with SDK `lv_aic_player_set_cmd`.

## Asynchronous seek (2026-10-03)

`lv_aic_player_seek(obj, position_us)` and `lv_aic_player_playback_seek` accept
one request at a time after media metadata is ready. Unknown/unseekable media,
out-of-range timestamps and concurrent seeks are rejected without changing the
current stream. READY remains prepared without automatic playback; PLAYING,
PAUSED and TERMINAL can seek while preserving requested start/pause/volume.
The widget retires its old image only once queued draw tasks have finished.
Native/decoder/GE readers can delay seek indefinitely; never force-release them.

SDK aic_player_seek resumes a paused player and provides no generation-tagged
callback completion. The worker therefore drains old frames, closes the old SDK
session, destroys its event mailbox, reopens the same URI and seeks before
starting its new decoder. The application allocator/bridge remain bounded and
session lease tickets stay monotonic. This costs demux/decoder reprepare latency
and reopens the URI (applications must keep media content stable), but prevents
old audio/terminal notifications from being accepted in the new timeline.
No UI-thread SDK calls or changes to SDK core are needed.

Status SEEKING/seek_pending reports the transaction. seeks_completed counts SDK
seek acceptance after reopening, not exact frame presentation. position_valid
is cleared until a new frame/audio callback arrives. Close cancels a pending
request; failures enter FAULT and retain resources until normal cleanup can
finish. The lower session adapter also restores pause after SDK seek and faults
if that restoration fails. Native widget VALUE_CHANGED includes seek completion.

Host regression **32/32 PASS** includes real worker and mocked SDK tests for
reader-delayed seek, blocked-get exit, old terminal disposal, repeated paused
seek, terminal seek, no-auto-start prepared seek, close cancellation, rejection
of unseekable/out-of-range targets, seek failure and audio time reset. Widget
contracts exercise image retirement and pending-seek rejection. Strict target
compilation: **PASS**. Real demux seek accuracy, post-seek A/V sync, reprepare
latency and DMA behavior remain **NOT_RUN** pending board validation.

### Seek firmware evidence

`build.ps1 -Phase ge2d -WithFonts -WithGif -WithWidgets -WithAicp -WithPlayer -Jobs 8`:
boot/app/static/image/manifest **PASS**, including live widget and worker seek
symbols. Clean sources: SDK `0026b35e`, component `cd92461`, LVGL `80ca777e`.

- Evidence directory remains `output/lvgl-evidence/ge2d-fonts-gif-widgets-aicp-player`.
- Image `images/d13x_D50T-2-Lite_page_2k_block_128k_v1.0.0.img` SHA256:
  `a8985b6b88dc6dccce8adae4528e595f1362ac118f87315ff26bdd76096f30fe`.
- ELF SHA256: `7deae145681f4adc8f4c404e3ffb12398fb40f436242b900ecd94986f7652c02`.
- This profile rebuild replaces the previous media image in that directory;
  previous hashes above are historical. No flashing or hardware tests ran.

## Shared-frame slave player (2026-10-03)

`lv_aic_slave_player_create` creates a display-only LVGL image subclass.
`lv_aic_slave_player_set_master(slave, master)` binds it to a native player;
NULL detaches. Get the current binding with `lv_aic_slave_player_get_master`.
Attach/rebind/detach takes effect at a safe timer pass; master frame publication
updates attached slaves in the same timer callback. Slave widgets accept native
image scale/rotation/pivot/alignment independently. Do not call lv_image_set_src
on either master or slave. Playback controls remain on the master.

Master and slaves share one immutable image descriptor and decoded view cache.
A small reference count covers widget owners; no additional decoder or frame
pixel copy is introduced for slaves. A slow native/GE reader can still retain
old storage and apply normal bounded-frame backpressure. Last widget release
retires the image, with underlying decoder/GE leases preserving storage until
reads complete (or indefinitely for quarantined DMA).

Deleting a master immediately unlinks surviving slaves; their old image owners
remain until queued draws finish. Slave-first deletion removes the list entry.
Rebinding never leaves a pointer to the old master. Seek/stop/source replacement
clears all attached slave sources before waiting for native readers/worker exit.
Slave orphan timers are included in lv_aic_player_pending_cleanup; keep running
LVGL timers until zero before shutdown. This is LVGL multi-view composition, not
an additional hardware video plane or a cross-display scanout synchronization
contract. Multi-player decode/group support is still separate.

Host suite **32/32 PASS**: widget tests now render alternating RGB/YUV and
black/white frames into three views with independent scaling, verify one
producer allocation, deferred detach during queued drawing, rebind, master-
first/slave-first deletion, reader survival after master deletion, and seek
retirement. E907 strict compilation **PASS**. Physical multi-view GE rendering,
memory pressure and lifecycle stress remain **NOT_RUN**.

### Slave firmware evidence and backend parity boundary

Standard `-WithPlayer` profile: boot/app/static/image/manifest **PASS**, including
live slave create/attach symbols. Clean sources: SDK `445a6b02`, component
`6adbd62`, LVGL `80ca777e`. No physical board execution.

- Evidence remains `output/lvgl-evidence/ge2d-fonts-gif-widgets-aicp-player`.
- Image `images/d13x_D50T-2-Lite_page_2k_block_128k_v1.0.0.img` SHA256:
  `3e6e1fd45a387f4371de88765ba5b29f7a9a24f807ac731f19a2767b4ec773e6`.
- ELF SHA256: `462d1ef7fbdd0cf4005ecff2238af23e6fba19c8ac712bb1dc8c71bd7e081136`.
- This replaces the previous seek-stage image in that evidence directory.

SDK parity clarification: packages/artinchip/lvgl-ui/aic_widgets/aic_player/
player_backend/aic_backend_ops.c explicitly rejects PLAYER_CMD_SET_PLAYBACK_RATE
with "AIC backend does not support playback rate control". Rate parity must be
assessed per backend (for example the pending APNG backend), not reported as a
working SDK video feature that this port alone lacks. Guarded auto-restart is implemented below. Backend
selection/APNG, groups and video-plane composition remain separate work.

## Guarded auto-restart (2026-10-03)

`lv_aic_player_set_auto_restart(obj, true)` opts into repeating the current URI;
default is off. It persists across source replacement/manual replay. The getter
reports this preference; `lv_aic_player_get_auto_restart_count` counts accepted
automatic seek requests over the widget lifetime, not successfully displayed
loops. The counter does not wrap. Controls remain on the master; slaves follow
its source retirement/new frames automatically.

A terminal VALUE_CHANGED event is delivered on its own timer pass. Applications
may disable repeat, stop, replace the URI or delete the widget in that handler.
On a later safe pass, repeat requires seekable media and either a fresh queued
video frame plus video EOS, or an audio-only valid timestamp from this epoch.
It uses asynchronous seek-to-zero, preserving pause and volume intent and
waiting for normal draw/native-reader release. Disabling repeat after that seek
was accepted prevents future repeats but does not cancel the in-flight seek.

This deliberately differs from SDK's unconditional PLAY_END -> seek(0): its
PLAY_END also represents decoder errors, and it provides no clean-audio-EOF
signal. Faults, unseekable media and terminal-without-progress do not retry.
An audio timestamp or video EOS still cannot certify clean completion of all
streams; the opt-in behavior is documented rather than labeled successful EOF.

Host **32/32 PASS** includes three repeat cycles, native readers surviving
retirement, no EOS/no fresh frames/unseekable suppression, disabling from the
terminal event, audio-only progress gating and callback deletion. Strict target
compilation **PASS**. Real media loop continuity, repeated decode resource
behavior and A/V synchronization remain **NOT_RUN** pending board validation.

### Auto-restart firmware evidence

Standard `-WithPlayer` profile: boot/app/static/image/manifest **PASS**, including
live auto-restart control/counter symbols. Clean sources: SDK `f258a3c3`,
component `8fd7708`, LVGL `80ca777e`. Physical board validation: **NOT_RUN**.

- Evidence directory: `output/lvgl-evidence/ge2d-fonts-gif-widgets-aicp-player`.
- Image `images/d13x_D50T-2-Lite_page_2k_block_128k_v1.0.0.img` SHA256:
  `7ed344b277de6a2f13a5591e6f0144b593426deefa40a7294b70b37fdd879e58`.
- ELF SHA256: `801604c625b66e8c45e73e67f15af6f374b7829a43888ac7799acaf8ad315556`.
- This replaces the slave-stage image in the same evidence directory.
