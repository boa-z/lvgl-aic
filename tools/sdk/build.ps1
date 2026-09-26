# SPDX-License-Identifier: Apache-2.0
# Windows-native Gate 1 / MPP / GE2D board-test build. Run in a dedicated task checkout.
param([ValidateSet('gate1','mpp','ge2d')][string]$Phase='gate1', [ValidateRange(1,64)][int]$Jobs=8, [switch]$AllowComponentDirty, [string]$SdkRoot=$env:LVGL_AIC_SDK_ROOT)
$ErrorActionPreference='Stop'
if (-not $SdkRoot) { $SdkRoot=Join-Path $PSScriptRoot '../../../../..' }
$root=(Resolve-Path $SdkRoot).Path
if (-not (Test-Path "$root/SConstruct")) { throw "Invalid SDK root: $root" }
$env:LVGL_AIC_SDK_ROOT=$root
Set-Location $root
$evidence=Join-Path $root "build/lvgl-evidence/$Phase"
New-Item -ItemType Directory -Force $evidence | Out-Null
$env:SCONS_LIB_DIR=Join-Path $root 'tools/env/tools/Python27/Lib/site-packages/scons'
$env:PYTHONUTF8='1'
$env:PYTHONIOENCODING='UTF-8'
$env:PATH="$root/tools/env/tools/Python38;$root/tools/env/tools/bin;$root/toolchain/bin;$env:PATH"
$python=Join-Path $root 'tools/env/tools/Python38/python3.exe'
$scons=Join-Path $root 'tools/env/tools/Python27/Scripts/scons'
function Run-Step([string]$Name, [string[]]$Arguments) {
    # Toolchains report warnings on stderr. With Stop in effect PowerShell turns
    # that into a terminating NativeCommandError and kills the running build,
    # so only the exit code may decide failure here.
    $previous = $ErrorActionPreference
    try {
        $ErrorActionPreference = 'Continue'
        & $python @Arguments *> "$evidence/$Name.log"
        $code = $LASTEXITCODE
    } finally {
        $ErrorActionPreference = $previous
    }
    if ($code -ne 0) {
        Get-Content "$evidence/$Name.log" -Tail 25
        throw "$Name failed; see $evidence/$Name.log"
    }
    Write-Host "$Name passed"
}
if (Test-Path .config) { Copy-Item .config "$evidence/config-before" }
Run-Step 'boot-config' @($scons,'--apply-def=d13x_d50t-2-lite_baremetal_bootloader_defconfig')
Run-Step 'boot-build' @($scons,"-j$Jobs")
Copy-Item output/d13x_d50t-2-lite_baremetal_bootloader/images/d13x.bin target/d13x/d50t-2-lite/pack/bootloader.bin
$def='d13x_d50t-2-lite_rt-thread_lvgl-aic-smoke_defconfig'
if ($Phase -eq 'mpp') {
    Run-Step 'assets' @('packages/custom/lvgl-aic/tools/sdk/stage_assets.py')
    $def='d13x_d50t-2-lite_rt-thread_lvgl-aic-mpp_defconfig'
}
if ($Phase -eq 'ge2d') {
    # The GE2D profile is the MPP profile plus the draw unit, so the MPP
    # regression is exercised on the same image.
    Run-Step 'assets' @('packages/custom/lvgl-aic/tools/sdk/stage_assets.py')
    $def='d13x_d50t-2-lite_rt-thread_lvgl-aic-ge2d_defconfig'
}
Run-Step 'app-config' @($scons,"--apply-def=$def")
Run-Step 'app-build' @($scons,"-j$Jobs")
$app='output/'+($def -replace '_defconfig$','')+'/images'
$checkArgs=@('packages/custom/lvgl-aic/tools/sdk/check_integration.py','--root','.', '--map',"$app/d13x.map",'--phase',$Phase)
if ($AllowComponentDirty) { $checkArgs += '--allow-component-dirty' }
Run-Step 'static-check' $checkArgs
Run-Step 'image-check' @('packages/custom/lvgl-aic/tools/sdk/verify_image.py',$app,'output/d13x_d50t-2-lite_baremetal_bootloader/images',$Phase)
New-Item -ItemType Directory -Force "$evidence/images" | Out-Null
Get-ChildItem "$app/*.img" | Copy-Item -Destination "$evidence/images"
Copy-Item .config,rtconfig.h "$evidence/"
Copy-Item "$app/d13x.elf","$app/d13x.map" "$evidence/images/"
$env:PATH="$root/tools/env/tools/Python38;$env:PATH"
Run-Step 'manifest' @('packages/custom/lvgl-aic/tools/sdk/write_manifest.py',$evidence,$Phase)
Write-Host "Verified test image and provenance: $evidence"
