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

Usage ordering (worker side, UI/GE publication remains to be implemented):

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

## Evidence

- Host **26/26 PASS**, actual SDK player/mpp_frame declarations, mocked player
  operations. New checks cover setup/metadata/start/pause/resume/seek failures,
  idempotent pause, frame saturation, stale tickets across reopen, transactional
  getters, held-frame stop/seek/close refusal, return retry, stop/destroy retry,
  restart preparation and duplicate-ID quarantine.
- New allocator host contract: all 14 SDK formats, each plane allocation
  failure/rollback, 32-frame limit, total budget including retired pins,
  metadata rejection, H264 smaller layout, deferred free, stale tickets,
  32-bit address/alignment guards and actual bounded YUV crop import. Session
  tests cover installation order and partially applied control failures.
- `tools/sdk/check-player-session.ps1`: D13x E907 double-float ABI, real SDK
  player/allocator and RT-Thread pthread headers, -Wall -Wextra -Werror: PASS.
  The script uses the SDK's Newlib/POSIX defines from compiler/pthread
  SConscript, including _POSIX_C_SOURCE=1 and _SYS__PTHREADTYPES_H_. It does not
  change SDK configuration or substitute host pthread declarations for target.
- SDK output/lvgl-player-session.o SHA256:
  812cec2ad03ed380e9a8d5fa7a664481918c3f6adce9a34c3f8ee94e66272d49.
- SDK output/lvgl-player-allocator.o SHA256:
  a64904bcfccb70d3ebf2359637f903f4b8319e87be928011f42b4742dd1b4682.
- Media-enabled image linking, real demux/codec/audio playback, physical DMA
  lifetime and board execution: **NOT_RUN**. The current GE image has no player.

## Remaining SDK parity

Implement background command/event handling (including EOS/error/seek), frame
publication with verified allocation bounds and deferred put_frame, player
widget controls, source replacement, repeat/rate behavior, slave and group
lifetimes, APNG backend, and explicit video-plane composition/ownership.
Do not report this internal session as a complete player widget or as tested
hardware decoding. All physical verification remains deferred.
