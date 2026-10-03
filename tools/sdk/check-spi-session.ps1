# SPDX-License-Identifier: Apache-2.0
# Compile checked SPI completion against real SDK headers; no device access.
param([string]$SdkRoot=$env:LVGL_AIC_SDK_ROOT,[switch]$WithGe)
$ErrorActionPreference='Stop'
if (-not $SdkRoot) {
    $candidate=Get-Item $PSScriptRoot
    while ($candidate -and -not (Test-Path (Join-Path $candidate.FullName 'SConstruct'))) { $candidate=$candidate.Parent }
    if (-not $candidate) { throw 'Set LVGL_AIC_SDK_ROOT' }
    $SdkRoot=$candidate.FullName
}
$sdk=(Resolve-Path $SdkRoot).Path
$component=(Resolve-Path (Join-Path $PSScriptRoot '../..')).Path
$lvgl=(Resolve-Path (Join-Path $component '../lvgl')).Path
$includes=@('.', 'packages/third-party/freetype/include', 'bsp/common/include', 'bsp/artinchip/sys/d13x/include',
    'bsp/artinchip/include/uapi', 'bsp/artinchip/include/drv', 'bsp/artinchip/hal/dvp/v1',
    'packages/artinchip/mpp/include', 'bsp/peripheral/camera', 'kernel/rt-thread/include', 'kernel/common/include/osal',
    'kernel/rt-thread/components/drivers/audio', 'kernel/rt-thread/components/finsh', 'kernel/rt-thread/components/drivers/include',
    'kernel/rt-thread/components/utilities/ulog', 'bsp/artinchip/include',
    'kernel/rt-thread/components/libc/posix/pthreads', 'kernel/rt-thread/components/libc/compilers/common/include')
$arguments=@('-std=gnu99','-Wall','-Wextra','-Werror','-DKERNEL_RTTHREAD','-DAIC_LVGL_BSP_RTTHREAD=1',
    '-march=rv32imafdcpzpsfoperand_xtheade','-mabi=ilp32d','-DAIC_LVGL_USE_SPI_SDK=1','-DRT_USING_NEWLIB','-DRT_USING_LIBC','-D_POSIX_C_SOURCE=1','-D_SYS__PTHREADTYPES_H_')
if($WithGe) { $arguments+=@('-DAIC_LVGL_USE_GE2D=1','-DAIC_LVGL_BSP_MPP=1') }
foreach ($path in $includes) { $arguments+=@('-isystem',(Join-Path $sdk $path)) }
$arguments+=('-I'+(Join-Path $component 'compat'))
foreach ($path in @($component,(Join-Path $component 'include'),$lvgl,(Join-Path $lvgl 'include'),(Join-Path $lvgl 'include/lvgl'))) {
    $arguments+=('-I'+$path)
}
$arguments+=@('-include',(Join-Path $component 'compat/lvgl_aic_build_config.h'))
$arguments+=@('-isystem',(Join-Path $sdk 'bsp/artinchip/include/hal'))
$output=Join-Path $sdk 'output/lvgl-spi-sdk.o'
New-Item -ItemType Directory -Force (Split-Path $output) | Out-Null
$compileArgs=$arguments+@('-c',(Join-Path $component 'port/lv_aic_spi_sdk.c'),'-o',$output)
& (Join-Path $sdk 'toolchain/bin/riscv64-unknown-elf-gcc.exe') @compileArgs
if($LASTEXITCODE -ne 0) { throw 'SPI completion target compilation failed' }
$symbols=& (Join-Path $sdk 'toolchain/bin/riscv64-unknown-elf-nm.exe') $output
if($LASTEXITCODE -ne 0) { throw 'SPI completion nm failed' }
foreach($symbol in @('rt_spi_wait_completion','rt_spi_get_transfer_status','rt_spi_nonblock_set','rt_qspi_transfer_message')) {
    if(-not ($symbols -cmatch ('\bU\s+'+$symbol+'$'))) { throw "Missing SDK reference: $symbol" }
}
if(-not ($symbols -cmatch '\bT\s+lv_aic_spi_sdk_wait_complete$')) { throw 'Missing completion implementation' }
if(-not ($symbols -cmatch '\bT\s+lv_aic_spi_sdk_submit_qspi$')) { throw 'Missing submit implementation' }
Get-FileHash $output -Algorithm SHA256
Write-Output 'PASS checked SPI submit/completion compile; no SDK transport link or hardware execution'

$objects=@($output)
foreach($source in @('port/lv_aic_spi_ge2d.c','port/lv_aic_spi_display.c','port/lv_aic_spi_worker.c','port/lv_aic_spi_handoff.c','port/lv_aic_spi_panel.c','port/lv_aic_spi_session.c','common/lv_aic_spi_transfer.c','common/lv_aic_spi_frame.c')) {
    $object=Join-Path $sdk ('output/'+[IO.Path]::GetFileNameWithoutExtension($source)+'.o')
    $compileArgs=$arguments+@('-c',(Join-Path $component $source),'-o',$object)
    & (Join-Path $sdk 'toolchain/bin/riscv64-unknown-elf-gcc.exe') @compileArgs
    if($LASTEXITCODE -ne 0) { throw "SPI target compile failed: $source" }
    $objects+=$object
}
$combined=Join-Path $sdk 'output/lvgl-spi-session-linked.o'
$linkArgs=@('-m','elf32lriscv','-r','-o',$combined)+$objects
& (Join-Path $sdk 'toolchain/bin/riscv64-unknown-elf-ld.exe') @linkArgs
if($LASTEXITCODE -ne 0) { throw 'SPI session partial link failed' }
$symbols=& (Join-Path $sdk 'toolchain/bin/riscv64-unknown-elf-nm.exe') $combined
if($LASTEXITCODE -ne 0) { throw 'SPI session nm failed' }
foreach($api in @('open','open_owned','enable_ge2d','submit','drain','close')) {
    if(-not ($symbols -cmatch ('\bT\s+lv_aic_spi_session_'+$api+'$'))) { throw "Missing session API: $api" }
}
if($WithGe) {
    foreach($api in @('create','convert','close')) {
        if(-not ($symbols -cmatch ('\bT\s+lv_aic_spi_ge2d_'+$api+'$'))) { throw "Missing GE implementation: $api" }
    }
    Write-Output 'PASS GE conversion/session partial link (no hardware execution)'
}
if($symbols -cmatch '\bU\s+lv_aic_spi_') { throw 'Unresolved component SPI dependency' }
Get-FileHash $combined -Algorithm SHA256
Write-Output 'PASS SPI session/frame/transfer/SDK bridge partial link; OS functions unresolved, no hardware execution'
