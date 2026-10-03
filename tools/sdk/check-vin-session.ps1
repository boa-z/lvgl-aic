# SPDX-License-Identifier: Apache-2.0
# D13x E907 compile and optional barcode partial-link check. Never opens a camera.
param([string]$SdkRoot=$env:LVGL_AIC_SDK_ROOT,[switch]$WithVideoPlane,[switch]$WithBarcode)
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
# Feature combinations must not silently inherit the last firmware profile.
# Load target config first: lv_conf.h normalizes defined Kconfig macros to 1.
$override=Join-Path $sdk 'output/lvgl-camera-check-config.h'
New-Item -ItemType Directory -Force (Split-Path $override) | Out-Null
$featureConfig="#include <lvgl_aic_target_config.h>`n#undef AIC_LVGL_USE_VIDEO_PLANE`n#define AIC_LVGL_USE_VIDEO_PLANE $([int]$WithVideoPlane.IsPresent)`n#undef AIC_LVGL_USE_BARCODE`n#define AIC_LVGL_USE_BARCODE $([int]$WithBarcode.IsPresent)`n"
[IO.File]::WriteAllText($override,$featureConfig,(New-Object Text.UTF8Encoding($false)))
$arguments+=@('-include',$override)
if ($WithVideoPlane) {
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
if ($WithBarcode) {
    $symbols=& $nm (Join-Path $sdk 'output/lvgl-camera-capture.o')
    if(-not ($symbols -match '\bU\s+lv_aic_barcode_decode$')) { throw 'Capture barcode decode path absent' }
    $symbols=& $nm (Join-Path $sdk 'output/lvgl-camera-widget.o')
    foreach($symbol in @('lv_aic_camera_capture_barcode_configure','lv_aic_camera_capture_barcode_poll')) {
        if(-not ($symbols -match ('\bU\s+'+$symbol+'$'))) { throw "Widget barcode path absent: $symbol" }
    }
}
if (-not $WithVideoPlane) {
    $symbols=& $nm (Join-Path $sdk 'output/lvgl-camera-widget.o')
    if($LASTEXITCODE -ne 0) { throw 'Widget nm failed' }
    if($symbols -match '\bU\s+lv_aic_plane_window_(present|close)$') { throw 'Disabled plane path still references window' }
}
if ($WithBarcode) {
    $barcodeObject=Join-Path $sdk 'output/lvgl-camera-barcode.o'
    $compileArgs=$arguments+@('-isystem',(Join-Path $sdk 'packages/artinchip/barcode/include'),
        '-c',(Join-Path $component 'common/lv_aic_barcode.c'),'-o',$barcodeObject)
    & (Join-Path $sdk 'toolchain/bin/riscv64-unknown-elf-gcc.exe') @compileArgs
    if($LASTEXITCODE -ne 0) { throw 'Barcode adapter target compilation failed' }
    $combined=Join-Path $sdk 'output/lvgl-camera-barcode-linked.o'
    $linkArgs=@('-m','elf32lriscv','-r','-o',$combined,(Join-Path $sdk 'output/lvgl-camera-widget.o'),
        (Join-Path $sdk 'output/lvgl-camera-capture.o'),$barcodeObject,
        (Join-Path $sdk 'packages/artinchip/barcode/lib/libdecoder.a'))
    & (Join-Path $sdk 'toolchain/bin/riscv64-unknown-elf-ld.exe') @linkArgs
    if($LASTEXITCODE -ne 0) { throw 'Camera barcode partial link failed' }
    $symbols=& $nm $combined
    if($LASTEXITCODE -ne 0) { throw 'Camera barcode linked-object nm failed' }
    foreach($symbol in @('lv_aic_camera_barcode_enable','lv_aic_camera_capture_barcode_poll',
        'lv_aic_camera_capture_barcode_configure','lv_aic_barcode_decode','Initial_Decoder',
        'Decoding_Image','GetResultLength','GetDecoderResult','Set_Donfig_Decoder')) {
        if(-not ($symbols -match ('\bT\s+'+$symbol+'$'))) { throw "Partial link unresolved implementation: $symbol" }
    }
    Get-FileHash $combined -Algorithm SHA256
    Write-Output 'PASS camera/widget/decoder archive partial link; OS/VIN/LVGL dependencies intentionally unresolved'
} else {
    $symbols=& $nm (Join-Path $sdk 'output/lvgl-camera-capture.o')
    if($LASTEXITCODE -ne 0) { throw 'Capture nm failed' }
    if($symbols -match '\bU\s+lv_aic_barcode_decode$') { throw 'Disabled barcode path still references decoder' }
}
Write-Output 'PASS VIN session/frame/capture/widget target checks; no final camera firmware link or hardware execution'
