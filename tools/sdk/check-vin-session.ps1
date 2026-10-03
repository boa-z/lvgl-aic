# SPDX-License-Identifier: Apache-2.0
# Compile-only D13x E907 double-float ABI check. Does not enable/open a camera.
param([string]$SdkRoot=$env:LVGL_AIC_SDK_ROOT,[switch]$WithVideoPlane)
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
    'bsp/artinchip/include/uapi', 'bsp/artinchip/hal/dvp/v1',
    'packages/artinchip/mpp/include', 'bsp/peripheral/camera', 'kernel/rt-thread/include', 'kernel/common/include/osal',
    'kernel/rt-thread/components/drivers/audio', 'kernel/rt-thread/components/finsh', 'kernel/rt-thread/components/drivers/include',
    'kernel/rt-thread/components/utilities/ulog', 'bsp/artinchip/include',
    'kernel/rt-thread/components/libc/posix/pthreads', 'kernel/rt-thread/components/libc/compilers/common/include')
$arguments=@('-std=gnu99','-Wall','-Wextra','-Werror','-DKERNEL_RTTHREAD','-DAIC_LVGL_BSP_RTTHREAD=1',
    '-march=rv32imafdcpzpsfoperand_xtheade','-mabi=ilp32d','-DAIC_LVGL_USE_VIN=1','-DAIC_LVGL_USE_CAMERA=1','-DRT_USING_NEWLIB','-DRT_USING_LIBC','-D_POSIX_C_SOURCE=1','-D_SYS__PTHREADTYPES_H_')
foreach ($path in $includes) { $arguments+=@('-isystem',(Join-Path $sdk $path)) }
$arguments+=('-I'+(Join-Path $component 'compat'))
foreach ($path in @($component,(Join-Path $component 'include'),$lvgl,(Join-Path $lvgl 'include'),(Join-Path $lvgl 'include/lvgl'))) {
    $arguments+=('-I'+$path)
}
$arguments+=@('-include',(Join-Path $component 'compat/lvgl_aic_build_config.h'))
if ($WithVideoPlane) {
    $override=Join-Path $sdk 'output/lvgl-camera-plane-config.h'
    New-Item -ItemType Directory -Force (Split-Path $override) | Out-Null
    [IO.File]::WriteAllText($override, "#include <rtconfig.h>`n#undef AIC_LVGL_USE_VIDEO_PLANE`n#define AIC_LVGL_USE_VIDEO_PLANE 1`n", (New-Object Text.UTF8Encoding($false)))
    $arguments+=@('-include',$override)
    $compileArgs=$arguments+@('-c',(Join-Path $component 'common/lv_aic_plane_window.c'),'-o',(Join-Path $sdk 'output/lvgl-plane-window.o'))
    & (Join-Path $sdk 'toolchain/bin/riscv64-unknown-elf-gcc.exe') @compileArgs
    if ($LASTEXITCODE -ne 0) { throw 'Shared plane window target compilation failed' }
}
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
# Confirm feature-enabled implementations, not empty conditional objects.
$nm=Join-Path $sdk 'toolchain/bin/riscv64-unknown-elf-nm.exe'
$expected=@{
    'lvgl-vin-session.o'=@('lv_aic_vin_open','lv_aic_vin_close')
    'lvgl-camera-capture.o'=@('lv_aic_camera_capture_open','lv_aic_camera_capture_close')
    'lvgl-camera-widget.o'=@('lv_aic_camera_create','lv_aic_camera_set_channel','lv_aic_camera_get_channel_status')
}
foreach($entry in $expected.GetEnumerator()) {
    $symbols=& $nm --defined-only (Join-Path $sdk ('output/'+$entry.Key))
    if($LASTEXITCODE -ne 0) { throw "nm failed: $($entry.Key)" }
    foreach($symbol in $entry.Value) {
        if(-not ($symbols -match ('\bT\s+'+[regex]::Escape($symbol)+'$'))) {
            throw "Enabled camera implementation absent: $symbol in $($entry.Key)"
        }
    }
}
if ($WithVideoPlane) {
    $symbols=& $nm (Join-Path $sdk 'output/lvgl-camera-widget.o')
    foreach($symbol in @('lv_aic_plane_window_present','lv_aic_plane_window_close')) {
        if(-not ($symbols -match ('\bU\s+'+$symbol+'$'))) { throw "Camera plane path absent: $symbol" }
    }
}
Write-Output 'PASS compile-only VIN session/frame/capture/widget; no camera link or hardware execution'
