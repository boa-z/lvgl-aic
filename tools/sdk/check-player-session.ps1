# SPDX-License-Identifier: Apache-2.0
# Compile only: real SDK player ABI, no playback/image/hardware execution.
param([string]$SdkRoot=$env:LVGL_AIC_SDK_ROOT)
$ErrorActionPreference='Stop'
if (-not $SdkRoot) {
    $candidate=Get-Item $PSScriptRoot
    while ($candidate -and -not (Test-Path (Join-Path $candidate.FullName 'SConstruct'))) { $candidate=$candidate.Parent }
    if (-not $candidate) { throw 'Set LVGL_AIC_SDK_ROOT' }
    $SdkRoot=$candidate.FullName
}
$sdk=(Resolve-Path $SdkRoot).Path
$component=(Resolve-Path (Join-Path $PSScriptRoot '../..')).Path
$includes=@('.', 'packages/third-party/freetype/include', 'bsp/common/include', 'bsp/artinchip/sys/d13x/include',
    'bsp/artinchip/include/uapi', 'bsp/artinchip/include', 'bsp/artinchip/include/drv',
    'packages/artinchip/mpp/ve/include', 'packages/artinchip/mpp/include', 'packages/artinchip/mpp/middle_media/player/include',
    'kernel/rt-thread/include', 'kernel/common/include/osal',
    'kernel/rt-thread/components/finsh', 'kernel/rt-thread/components/drivers/include',
    'kernel/rt-thread/components/drivers/audio',
    'kernel/rt-thread/components/utilities/ulog',
    'kernel/rt-thread/components/libc/posix/pthreads',
    'kernel/rt-thread/components/libc/compilers/common/include')
$arguments=@('-std=gnu99','-Wall','-Wextra','-Werror','-DKERNEL_RTTHREAD','-DAIC_LVGL_BSP_RTTHREAD=1',
    '-march=rv32imafdcpzpsfoperand_xtheade','-mabi=ilp32d',
    '-DAIC_LVGL_USE_GE2D=1','-DAIC_LVGL_USE_VIDEO_PLANE=1','-DAIC_LVGL_USE_APNG_WIDGET=1','-DAIC_LVGL_USE_APNG=1','-DAIC_LVGL_USE_PLAYER=1','-DAIC_LVGL_USE_PLAYER_SESSION=1','-DAIC_MPP_PLAYER_VIDEO_EXT_RENDER=1',
    '-DRT_USING_NEWLIB','-DRT_USING_LIBC','-D_POSIX_C_SOURCE=1','-D_SYS__PTHREADTYPES_H_')
foreach ($path in $includes) { $arguments+=@('-isystem',(Join-Path $sdk $path)) }
$arguments+=('-I'+(Join-Path $component 'compat'))
$lvgl=(Resolve-Path (Join-Path $component '../lvgl')).Path
foreach ($path in @($component,(Join-Path $component 'include'),(Join-Path $component 'common'),
    $lvgl,(Join-Path $lvgl 'include'),(Join-Path $lvgl 'include/lvgl'))) {
    $arguments+=('-I'+$path)
}
$arguments+=@('-include',(Join-Path $component 'compat/lvgl_aic_build_config.h'),'-DAIC_LVGL_BSP_MPP=1')
foreach ($module in @('video_plane','ve_client','media_runtime','player_session','player_playback','player_allocator','player_frames','player_events','player_clock','rgb_image','rgb_mpp','player','apng','apng_compose','apng_timeline','apng_decoder','apng_stream','apng_frames','apng_playback','apng_widget','player_control','plane_test')) {
    $output=Join-Path $sdk ("output/lvgl-"+$module.Replace('_','-')+'.o')
    New-Item -ItemType Directory -Force (Split-Path $output) | Out-Null
    $directory=if ($module -eq 'plane_test') { 'tests/manual' } elseif ($module -in @('player_clock','rgb_mpp','apng','apng_compose','apng_timeline')) { 'common' } elseif ($module -in @('player','apng_widget','player_control','plane_test')) { 'widgets' } elseif ($module -eq 'rgb_image') { 'image/mpp' } else { 'port' }
    $compileArgs=$arguments+@('-c',(Join-Path $component "$directory/lv_aic_$module.c"),'-o',$output)
    & (Join-Path $sdk 'toolchain/bin/riscv64-unknown-elf-gcc.exe') @compileArgs
    if ($LASTEXITCODE -ne 0) { throw "$module target compilation failed" }
    Get-FileHash $output -Algorithm SHA256
}
Write-Output 'PASS compile-only player session/playback/allocator/frame bridge/events/clock/RGB/widget/APNG container/composition/timeline/MPP adapter/stream/frame publication/playback/widget/command adapter; no media link or hardware execution'
