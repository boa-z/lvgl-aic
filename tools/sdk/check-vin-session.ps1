# SPDX-License-Identifier: Apache-2.0
# Compile-only D13x E907 double-float ABI check. Does not enable/open a camera.
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
$lvgl=(Resolve-Path (Join-Path $component '../lvgl')).Path
$includes=@('.', 'bsp/common/include', 'bsp/artinchip/sys/d13x/include',
    'bsp/artinchip/include/uapi', 'bsp/artinchip/hal/dvp/v1',
    'packages/artinchip/mpp/include', 'kernel/rt-thread/include', 'kernel/common/include/osal',
    'kernel/rt-thread/components/finsh', 'kernel/rt-thread/components/drivers/include',
    'kernel/rt-thread/components/utilities/ulog', 'bsp/artinchip/include',
    'kernel/rt-thread/components/libc/compilers/common/include')
$arguments=@('-std=gnu99','-Wall','-Wextra','-Werror',
    '-march=rv32imafdcpzpsfoperand_xtheade','-mabi=ilp32d','-DAIC_LVGL_USE_VIN=1','-DAIC_LVGL_USE_CAMERA=1')
foreach ($path in $includes) { $arguments+=@('-isystem',(Join-Path $sdk $path)) }
$arguments+=('-I'+(Join-Path $component 'compat'))
foreach ($path in @($component,(Join-Path $component 'include'),$lvgl,(Join-Path $lvgl 'include'),(Join-Path $lvgl 'include/lvgl'))) {
    $arguments+=('-I'+$path)
}
$arguments+=@('-include',(Join-Path $component 'compat/lvgl_aic_build_config.h'))
foreach ($module in @('vin_session','vin_frame','camera_capture')) {
    $output=Join-Path $sdk ("output/lvgl-"+$module.Replace('_','-')+'.o')
    New-Item -ItemType Directory -Force (Split-Path $output) | Out-Null
    $compileArgs=$arguments+@('-c',(Join-Path $component "port/lv_aic_$module.c"),'-o',$output)
    & (Join-Path $sdk 'toolchain/bin/riscv64-unknown-elf-gcc.exe') @compileArgs
    if ($LASTEXITCODE -ne 0) { throw "VIN $module target compilation failed" }
    Get-FileHash $output -Algorithm SHA256
}
$output=Join-Path $sdk 'output/lvgl-camera-widget.o'
$compileArgs=$arguments+@('-c',(Join-Path $component 'widgets/lv_aic_camera.c'),'-o',$output)
& (Join-Path $sdk 'toolchain/bin/riscv64-unknown-elf-gcc.exe') @compileArgs
if ($LASTEXITCODE -ne 0) { throw 'Camera widget target compilation failed' }
Get-FileHash $output -Algorithm SHA256
Write-Output 'PASS compile-only VIN session/frame/capture/widget; no camera link or hardware execution'
