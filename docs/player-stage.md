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

## Evidence

- Host **25/25 PASS**, actual SDK player/mpp_frame declarations, mocked player
  operations. New checks cover setup/metadata/start/pause/resume/seek failures,
  idempotent pause, frame saturation, stale tickets across reopen, transactional
  getters, held-frame stop/seek/close refusal, return retry, stop/destroy retry,
  restart preparation and duplicate-ID quarantine.
- `tools/sdk/check-player-session.ps1`: D13x E907 double-float ABI, real SDK
  player and RT-Thread pthread headers, -Wall -Wextra -Werror: PASS.
  The script uses the SDK's Newlib/POSIX defines from compiler/pthread
  SConscript, including _POSIX_C_SOURCE=1 and _SYS__PTHREADTYPES_H_. It does not
  change SDK configuration or substitute host pthread declarations for target.
- SDK output/lvgl-player-session.o SHA256:
  b960e2e327fa6733588a25ab4d5a062a0c05f2b0e84727b2cdc531bc33b7ba7d.
- Media-enabled image linking, real demux/codec/audio playback, physical DMA
  lifetime and board execution: **NOT_RUN**. The current GE image has no player.

## Remaining SDK parity

Implement background command/event handling (including EOS/error/seek), frame
publication with verified allocation bounds and deferred put_frame, player
widget controls, source replacement, repeat/rate behavior, slave and group
lifetimes, APNG backend, and explicit video-plane composition/ownership.
Do not report this internal session as a complete player widget or as tested
hardware decoding. All physical verification remains deferred.
