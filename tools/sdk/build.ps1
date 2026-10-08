# SPDX-License-Identifier: Apache-2.0
# Windows-native Gate 1 / MPP / GE2D board-test build. Run in a dedicated task checkout.
param([ValidateSet('gate1','mpp','ge2d')][string]$Phase='gate1', [ValidateRange(1,64)][int]$Jobs=8, [switch]$AllowComponentDirty, [switch]$WithFonts, [switch]$WithGif, [switch]$WithWidgets, [switch]$WithAicp, [switch]$WithPlayer, [switch]$WithApng, [switch]$WithBarcode, [switch]$WithSpi, [switch]$WithCamera, [switch]$WithDemos, [switch]$WithMusic, [switch]$WithMeter, [switch]$WithDashboard, [ValidateSet('meter','dashboard','slide','multi_lang','demo_hub','image')][string[]]$OfficialDemos=@(), [switch]$VirtualRes, [switch]$WithCanCapture, [switch]$WithCanOta, [ValidatePattern('^[A-Za-z0-9_.+-]{1,47}$')][string]$OtaVersion='1.0.0', [switch]$WithVector, [switch]$WithSvg, [switch]$WithLottie, [ValidateSet(0,90,180,270)][int]$Rotation=0, [ValidateSet('rgb888','rgb565')][string]$FbFormat='rgb888', [ValidatePattern('^(\d{3,4}x\d{3,4})?$')][string]$TouchRange='', [string]$SdkRoot=$env:LVGL_AIC_SDK_ROOT, [ValidatePattern('^[a-z0-9][a-z0-9-]{0,31}$')][string]$EvidenceTag)
$ErrorActionPreference='Stop'
if ($WithSvg -or $WithLottie) { $WithVector=$true }
if (-not $SdkRoot) {
    $candidate=Get-Item $PSScriptRoot
    while ($candidate -and -not (Test-Path (Join-Path $candidate.FullName 'SConstruct'))) { $candidate=$candidate.Parent }
    if (-not $candidate) { throw 'Set LVGL_AIC_SDK_ROOT to the containing SDK checkout' }
    $SdkRoot=$candidate.FullName
}
$root=(Resolve-Path $SdkRoot).Path
if (-not (Test-Path "$root/SConstruct")) { throw "Invalid SDK root: $root" }
$env:LVGL_AIC_SDK_ROOT=$root
Set-Location $root
if ($WithFonts -and $Phase -eq 'gate1') { throw '-WithFonts requires the mpp or ge2d resource profile' }
if ($WithAicp -and $Phase -eq 'gate1') { throw '-WithAicp requires the mpp or ge2d resource profile' }
if ($WithGif -and $Phase -eq 'gate1') { throw '-WithGif requires the mpp or ge2d resource profile' }
# demo_hub's audio/video apps are built on the media player widget.
if ($OfficialDemos -contains 'demo_hub') { $WithPlayer=[switch]$true }
# image_demo: FreeType lyrics on the component canvas widget.
if ($OfficialDemos -contains 'image') { $WithFonts=[switch]$true }
# 8.8 MB + 3.6 MB of assets plus the other demos exceed the 14 MB rodata.
if (($OfficialDemos -contains 'demo_hub') -and ($OfficialDemos -contains 'image')) { throw 'demo_hub and image do not fit in rodata together; build them separately' }
if ($WithPlayer -and $Phase -eq 'gate1') { throw '-WithPlayer requires mpp or ge2d' }
if ($WithApng -and $Phase -eq 'gate1') { throw '-WithApng requires mpp or ge2d' }
if ($WithCamera) {
    if ($Phase -eq 'gate1') { throw '-WithCamera requires mpp or ge2d' }
    $cameraProfile=if ($Phase -eq 'ge2d') { 'ge2d' } else { 'mpp' }
    $cameraDef=Join-Path $root "target/configs/d13x_d50t-2-lite_rt-thread_lvgl-aic-${cameraProfile}_defconfig"
    $cameraConfig=[IO.File]::ReadAllText($cameraDef)
    foreach ($symbol in @('AIC_USING_DVP','AIC_USING_CAMERA')) {
        if ($cameraConfig -notmatch ('(?m)^CONFIG_'+$symbol+'=y\r?$')) {
            throw "Camera profile requires reviewed $symbol=y in $cameraDef; no sensor/pin defaults are selected by this script"
        }
    }
    $sensors=[regex]::Matches($cameraConfig,'(?m)^CONFIG_AIC_USING_CAMERA_[A-Z0-9_]+=y\r?$')
    if ($sensors.Count -ne 1) { throw 'Camera profile must explicitly select exactly one sensor' }
    foreach ($symbol in @('AIC_CAMERA_I2C_CHAN','AIC_CAMERA_RST_PIN','AIC_CAMERA_PWDN_PIN')) {
        if ($cameraConfig -notmatch ('(?m)^CONFIG_'+$symbol+'=.+\r?$')) { throw "Camera profile must explicitly supply $symbol" }
    }
}
$variant=$Phase
if ($WithFonts) { $variant += '-fonts' }
if ($WithGif) { $variant += '-gif' }
if ($WithWidgets) { $variant += '-widgets' }
if ($WithAicp) { $variant += '-aicp' }
if ($WithPlayer) { $variant += '-player' }
if ($WithApng) { $variant += '-apng' }
if ($WithBarcode) { $variant += '-barcode' }
if ($WithSpi) { $variant += '-spi' }
if ($WithCamera) { $variant += '-camera' }
if ($WithDemos) { $variant += '-demos' }
if ($WithMusic) { $variant += '-music' }
# Official SDK demos (demos/official): -OfficialDemos meter,dashboard,slide;
# -WithMeter/-WithDashboard are shorthands.
$OfficialDemos=@(@($OfficialDemos) + $(if ($WithMeter) { 'meter' }) + $(if ($WithDashboard) { 'dashboard' }) | Where-Object { $_ } | Sort-Object -Unique)
if ($OfficialDemos.Count) { $variant += '-od-' + ($OfficialDemos -join '-') }
if ($VirtualRes) { $variant += '-vres' }
if ($WithCanCapture) { $variant += '-cancap' }
if ($WithCanOta) { $variant += '-canota' }
if ($FbFormat -ne 'rgb888') { $variant += "-fb$($FbFormat.Substring(3))" }
if ($TouchRange) { $variant += "-tr$TouchRange" }
if ($WithVector) { $variant += '-vector' }
if ($WithSvg) { $variant += '-svg' }
if ($WithLottie) { $variant += '-lottie' }
if ($Rotation) { $variant += "-rotate$Rotation" }
if ($EvidenceTag) { $variant += "-$EvidenceTag" }
$evidence=Join-Path $root "output/lvgl-evidence/$variant"
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
$assetArgs=@("$PSScriptRoot/stage_assets.py")
if ($WithFonts) { $assetArgs += '--fonts' }
if ($WithGif) { $assetArgs += '--gif' }
if ($WithAicp) { $assetArgs += '--aicp' }
if ($WithApng) { $assetArgs += '--apng' }
$def='d13x_d50t-2-lite_rt-thread_lvgl-aic-smoke_defconfig'
if ($Phase -eq 'mpp') {
    Run-Step 'assets' $assetArgs
    $def='d13x_d50t-2-lite_rt-thread_lvgl-aic-mpp_defconfig'
}
if ($Phase -eq 'ge2d') {
    # The GE2D profile is the MPP profile plus the draw unit, so the MPP
    # regression is exercised on the same image.
    Run-Step 'assets' $assetArgs
    $def='d13x_d50t-2-lite_rt-thread_lvgl-aic-ge2d_defconfig'
}
# SCons reloads defconfig on every invocation. Restore this isolated smoke
# profile's exact bytes on both success and failure.
$defPath=Join-Path $root "target/configs/$def"
$originalDef=[IO.File]::ReadAllBytes($defPath)
try {
    if ($WithFonts -or $WithGif -or $WithWidgets -or $Rotation -or $WithAicp -or $WithPlayer -or $WithApng -or $WithBarcode -or $WithSpi -or $WithCamera -or $WithDemos -or $WithMusic -or $OfficialDemos.Count -or $VirtualRes -or ($FbFormat -ne 'rgb888') -or $TouchRange -or $WithCanCapture -or $WithCanOta -or $WithVector) {
        [IO.File]::WriteAllBytes("$evidence/defconfig-original", $originalDef)
        $settings=@("CONFIG_AIC_LVGL_DISPLAY_ROTATION=$([int]($Rotation / 90))")
        if ($WithFonts) { $settings += @('CONFIG_AIC_LVGL_USE_FREETYPE=y', 'CONFIG_AIC_LVGL_USE_FT_CACHE=y', 'CONFIG_LPKG_USING_FREETYPE=y', 'CONFIG_AIC_LVGL_FREETYPE_GLYPHS=64') }
        if ($WithGif) { $settings += 'CONFIG_AIC_LVGL_USE_GIF=y' }
        if ($WithWidgets) { $settings += @('CONFIG_AIC_LVGL_USE_CANVAS=y', 'CONFIG_AIC_LVGL_USE_IMG_ROLLER=y', 'CONFIG_AIC_LVGL_USE_SWIPE_V1=y'); if ($Phase -eq 'ge2d') { $settings += 'CONFIG_AIC_LVGL_USE_VIDEO_WINDOW=y' } }
        if ($WithAicp) { $settings += 'CONFIG_AIC_MPP_AICP_DEC_ENABLE=y' }
        if ($WithPlayer) { $settings += @('CONFIG_AIC_LVGL_USE_VIDEO_PLANE=y', 'CONFIG_AIC_LVGL_USE_PLAYER=y', 'CONFIG_AIC_LVGL_USE_PLAYER_SESSION=y', 'CONFIG_AIC_MPP_PLAYER_INTERFACE=y', 'CONFIG_AIC_MPP_PLAYER_VIDEO_EXT_RENDER=y', 'CONFIG_AIC_MPP_H264_DEC_ENABLE=y') }
        if ($WithBarcode) { $settings += 'CONFIG_AIC_LVGL_USE_BARCODE=y' }
        if ($WithSpi) { $settings += 'CONFIG_AIC_LVGL_USE_SPI_SDK=y' }
        if ($WithLottie) { $settings += 'CONFIG_AIC_LVGL_USE_LOTTIE=y' }
        if ($WithSvg) { $settings += 'CONFIG_AIC_LVGL_USE_SVG=y' }
        if ($WithVector) { $settings += 'CONFIG_AIC_LVGL_USE_VECTOR=y' }
        if ($WithMusic) { $settings += 'CONFIG_AIC_LVGL_BUILD_DEMO_MUSIC=y' }
        # Official SDK demos are 1024x600 designs: imply the virtual resolution.
        if ($OfficialDemos.Count) { $settings += @('CONFIG_AIC_LVGL_OFFICIAL_DEMOS=y', 'CONFIG_AIC_LVGL_VIRTUAL_RES=y', 'CONFIG_AIC_LVGL_MPP_CACHE_ENTRIES=64') }
        if ($OfficialDemos -contains 'image') { $settings += 'CONFIG_AIC_LVGL_USE_CANVAS=y' }
        foreach ($demo in @('meter','dashboard','slide','multi_lang','demo_hub','image')) {
            $symbol='CONFIG_AIC_LVGL_OFFICIAL_DEMO_' + $demo.ToUpper()
            $settings += $(if ($OfficialDemos -contains $demo) { "$symbol=y" } else { "# $symbol is not set" })
        }
        if ($VirtualRes) { $settings += 'CONFIG_AIC_LVGL_VIRTUAL_RES=y' }
        # Touch controller coordinate range (SDK default 1024x600; the D50T product sets the panel size).
        if ($TouchRange) { $tx,$ty=$TouchRange -split 'x'; $settings += @("CONFIG_AIC_TOUCH_X_COORDINATE_RANGE=$tx", "CONFIG_AIC_TOUCH_Y_COORDINATE_RANGE=$ty") }
        # Framebuffer pixel format (a Kconfig choice: one on, the other off).
        if ($FbFormat -eq 'rgb565') { $settings += @('# CONFIG_AICFB_RGB888 is not set', 'CONFIG_AICFB_RGB565=y') }
        if ($WithCanCapture) { $settings += 'CONFIG_AIC_LVGL_USE_CAN_CAPTURE=y' }
        if ($WithCanOta) { $settings += @('CONFIG_AIC_LVGL_SMOKE_CAN_OTA=y', "CONFIG_AIC_LVGL_SMOKE_CAN_OTA_VERSION=`"$OtaVersion`"") }
        if ($WithDemos) { $settings += @('CONFIG_AIC_LVGL_BUILD_DEMO_WIDGETS=y','CONFIG_AIC_LVGL_BUILD_DEMO_BENCHMARK=y') }
        if ($WithCamera) { $settings += @('CONFIG_AIC_MPP_VIN=y','CONFIG_AIC_LVGL_USE_VIN=y','CONFIG_AIC_LVGL_USE_CAMERA=y') }
        if ($WithApng) { $settings += @('CONFIG_AIC_LVGL_USE_APNG=y', 'CONFIG_AIC_LVGL_USE_APNG_WIDGET=y') }
        $content=[IO.File]::ReadAllText($defPath)
        foreach ($setting in $settings) {
            # "# X is not set" settings name their symbol in the comment form.
            $symbol=[regex]::Replace(($setting -split '=')[0], '^# (\S+) is not set$', '$1')
            $content=[regex]::Replace($content, '(?m)^(?:# )?'+$symbol+'(?:=.*| is not set)\r?\n?', '')
        }
        $content=$content.TrimEnd()+[Environment]::NewLine+($settings -join [Environment]::NewLine)+[Environment]::NewLine
        [IO.File]::WriteAllText($defPath, $content, (New-Object Text.UTF8Encoding($false)))
        Copy-Item $defPath "$evidence/defconfig-effective"
    }
    Run-Step 'app-config' @($scons,"--apply-def=$def")
    Run-Step 'app-build' @($scons,"-j$Jobs")
} finally {
    if ($WithFonts -or $WithGif -or $WithWidgets -or $Rotation -or $WithAicp -or $WithPlayer -or $WithApng -or $WithBarcode -or $WithSpi -or $WithCamera -or $WithDemos -or $WithMusic -or $OfficialDemos.Count -or $VirtualRes -or ($FbFormat -ne 'rgb888') -or $TouchRange -or $WithCanCapture -or $WithCanOta -or $WithVector) { [IO.File]::WriteAllBytes($defPath, $originalDef) }
}
$app='output/'+($def -replace '_defconfig$','')+'/images'
$checkArgs=@("$PSScriptRoot/check_integration.py",'--root','.', '--map',"$app/d13x.map",'--phase',$Phase)
if ($AllowComponentDirty) { $checkArgs += '--allow-component-dirty' }
if ($WithFonts) { $checkArgs += '--with-fonts' }
if ($WithGif) { $checkArgs += '--with-gif' }
if ($WithWidgets) { $checkArgs += '--with-widgets' }
if ($WithAicp) { $checkArgs += '--with-aicp' }
if ($WithPlayer) { $checkArgs += '--with-player' }
if ($WithApng) { $checkArgs += '--with-apng' }
if ($WithBarcode) { $checkArgs += '--with-barcode' }
if ($WithSpi) { $checkArgs += '--with-spi' }
if ($WithCamera) { $checkArgs += '--with-camera' }
if ($WithDemos) { $checkArgs += '--with-demos' }
if ($WithMusic) { $checkArgs += '--with-music' }
if ($OfficialDemos.Count) { $checkArgs += @('--official-demos', ($OfficialDemos -join ',')) }
if ($WithCanCapture) { $checkArgs += '--with-can-capture' }
if ($WithCanOta) { $checkArgs += '--with-can-ota' }
if ($WithVector) { $checkArgs += '--with-vector' }
if ($WithLottie) { $checkArgs += '--with-lottie' }
if ($WithSvg) { $checkArgs += '--with-svg' }
$checkArgs += @('--rotation', "$Rotation")
Run-Step 'static-check' $checkArgs
Run-Step 'image-check' @("$PSScriptRoot/verify_image.py",$app,'output/d13x_d50t-2-lite_baremetal_bootloader/images',$Phase)
New-Item -ItemType Directory -Force "$evidence/images" | Out-Null
Get-ChildItem "$app/*.img" | Copy-Item -Destination "$evidence/images"
Copy-Item .config,rtconfig.h "$evidence/"
Copy-Item "$app/d13x.elf","$app/d13x.map" "$evidence/images/"
$env:PATH="$root/tools/env/tools/Python38;$env:PATH"
if ($Phase -eq 'ge2d' -and (Select-String -Path .config -Pattern '^CONFIG_AIC_GE_CMDQ=y$' -Quiet)) {
    Copy-Item build/lvgl-ge-cmdq.c,build/lvgl-ge-cmdq.json "$evidence/"
}
if ($WithLottie) { Copy-Item build/lvgl-lottie-builder.cpp "$evidence/" }
if ($WithSvg) { Copy-Item build/lvgl-svg-draw.c,build/lvgl-svg-decoder.c,build/lvgl-svg-render.c "$evidence/" }
Copy-Item build/lvgl-sw-image.c,build/lvgl-sw-area.c,build/lvgl-sw-transform.c,build/lvgl-sw-widget.c "$evidence/"
if ($WithVector) { Copy-Item build/lvgl-sw-vector.c "$evidence/" }
Run-Step 'manifest' @("$PSScriptRoot/write_manifest.py",$evidence,$variant)
Write-Host "Verified test image and provenance: $evidence"
