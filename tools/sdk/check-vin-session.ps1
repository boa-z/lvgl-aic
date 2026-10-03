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
$output=Join-Path $sdk 'output/lvgl-vin-session.o'
$includes=@('.', 'bsp/common/include', 'bsp/artinchip/sys/d13x/include',
    'bsp/artinchip/include/uapi', 'bsp/artinchip/hal/dvp/v1',
    'packages/artinchip/mpp/include', 'kernel/rt-thread/include',
    'kernel/rt-thread/components/finsh', 'kernel/rt-thread/components/libc/compilers/common/include')
$arguments=@('-std=gnu99','-Wall','-Wextra','-Werror',
    '-march=rv32imafdcpzpsfoperand_xtheade','-mabi=ilp32d','-DAIC_LVGL_USE_VIN=1')
foreach ($path in $includes) { $arguments+=('-I'+(Join-Path $sdk $path)) }
$arguments+=('-I'+(Join-Path $component 'compat'))
$arguments+=@('-c',
    (Join-Path $component 'port/lv_aic_vin_session.c'),'-o',$output)
New-Item -ItemType Directory -Force (Split-Path $output) | Out-Null
& (Join-Path $sdk 'toolchain/bin/riscv64-unknown-elf-gcc.exe') @arguments
if ($LASTEXITCODE -ne 0) { throw 'VIN session target compilation failed' }
Get-FileHash $output -Algorithm SHA256
Write-Output 'PASS compile-only VIN session; no camera link or hardware execution'
