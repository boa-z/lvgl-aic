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
$includes=@('.', 'bsp/common/include', 'bsp/artinchip/sys/d13x/include',
    'bsp/artinchip/include/uapi', 'bsp/artinchip/include',
    'packages/artinchip/mpp/include', 'packages/artinchip/mpp/middle_media/player/include',
    'kernel/rt-thread/include', 'kernel/common/include/osal',
    'kernel/rt-thread/components/finsh', 'kernel/rt-thread/components/drivers/include',
    'kernel/rt-thread/components/utilities/ulog',
    'kernel/rt-thread/components/libc/posix/pthreads',
    'kernel/rt-thread/components/libc/compilers/common/include')
$arguments=@('-std=gnu99','-Wall','-Wextra','-Werror',
    '-march=rv32imafdcpzpsfoperand_xtheade','-mabi=ilp32d',
    '-DAIC_LVGL_USE_PLAYER_SESSION=1','-DAIC_MPP_PLAYER_VIDEO_EXT_RENDER=1',
    '-DRT_USING_NEWLIB','-DRT_USING_LIBC','-D_POSIX_C_SOURCE=1','-D_SYS__PTHREADTYPES_H_')
foreach ($path in $includes) { $arguments+=@('-isystem',(Join-Path $sdk $path)) }
$arguments+=('-I'+(Join-Path $component 'compat'))
$output=Join-Path $sdk 'output/lvgl-player-session.o'
New-Item -ItemType Directory -Force (Split-Path $output) | Out-Null
$arguments+=@('-c',(Join-Path $component 'port/lv_aic_player_session.c'),'-o',$output)
& (Join-Path $sdk 'toolchain/bin/riscv64-unknown-elf-gcc.exe') @arguments
if ($LASTEXITCODE -ne 0) { throw 'Player session target compilation failed' }
Get-FileHash $output -Algorithm SHA256
Write-Output 'PASS compile-only player session; no media link or hardware execution'
