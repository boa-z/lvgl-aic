Import('AIC_ROOT')
from building import *
import os

cwd = GetCurrentDir()
group = []
src = []

if GetDepend('AIC_LVGL_PORT'):
    # The port sources contain explicit compile-time feature guards. Keeping
    # them in one group makes the legacy/new LVGL selection auditable.
    src = Glob('port/*.c')

    # Phase 2 decoder boundary is always compiled; the sources stub out when
    # AIC_LVGL_USE_MPP_DEC is off. GE2D lives in draw/ge2d and is never pulled
    # in by the image decoder; it stubs out when AIC_LVGL_USE_GE2D is off.
    src += Glob('image/mpp/*.c')

    # Shared LVGL<->MPP pixel format mapper. Both the decoder (image/mpp) and
    # the GE2D backend (draw/ge2d) depend on it, so it lives outside either.
    src += Glob('common/*.c')

    # Phase 3A GE2D draw unit. Compiling it unconditionally keeps the source in
    # the dependency graph; the file body is guarded by AIC_LVGL_USE_GE2D.
    src += Glob('draw/ge2d/*.c')

    if GetDepend('AIC_LVGL_MANUAL_TEST'):
        src += Glob('tests/manual/*.c')

    lvgl_root = os.path.join(AIC_ROOT, 'packages', 'third-party', 'lvgl')
    cpppath = [
        cwd,
        os.path.join(cwd, 'include'),
        os.path.join(cwd, 'compat'),
        os.path.join(cwd, 'common'),
        os.path.join(cwd, 'image', 'mpp'),
        os.path.join(cwd, 'draw', 'ge2d'),
        os.path.join(lvgl_root, 'include'),
        os.path.join(lvgl_root, 'include', 'lvgl'),
    ]
    if GetDepend('AIC_LVGL_USE_MPP_DEC') or GetDepend('AIC_LVGL_USE_GE2D'):
        # MPP decoder API (mpp_decoder.h/frame_allocator.h), GE2D API
        # (mpp_ge.h) and UAPI pixel formats. The GE2D sources also need
        # bsp/artinchip/include/drv for aic_drv_ge.h, which the artinchip BSP
        # build already puts on the global include path.
        cpppath += [
            os.path.join(AIC_ROOT, 'packages', 'artinchip', 'mpp', 'include'),
            os.path.join(AIC_ROOT, 'bsp', 'artinchip', 'include', 'uapi'),
        ]

    # Feature symbols are generated in rtconfig.h by the Luban-Lite Kconfig
    # bridge. Redefining them with -D creates preprocessor redefinition
    # warnings, so only the internal BSP feature flags are added here.
    cppdefines = [
        'AIC_LVGL_BSP_RTTHREAD=%d' % (1 if GetDepend('KERNEL_RTTHREAD') else 0),
        'AIC_LVGL_BSP_MPP=%d' % (1 if GetDepend('LPKG_MPP') else 0),
    ]

    group = DefineGroup(
        'AIC-LVGL',
        src,
        depend=['LPKG_USING_LVGL', 'AIC_LVGL_PORT'],
        CPPPATH=cpppath,
        CPPDEFINES=cppdefines,
    )

# Filesystem packing is a link post-action in this SDK. Make fixture edits,
# additions and removals invalidate the ELF so the packed image cannot stay stale.
if GetDepend('AIC_LVGL_USE_MPP_DEC') and GetDepend('AIC_LVGL_SMOKE_APP'):
    Import('PRJ_OUT_DIR', 'PRJ_CHIP')
    from SCons.Script import Depends, File, Value
    import SCons.Errors
    stage = os.path.join(AIC_ROOT, 'build', 'lvgl-mpp-data', 'mpp_test')
    if not os.path.isfile(os.path.join(stage, 'SHA256.json')):
        raise SCons.Errors.UserError('Run python packages/custom/lvgl-aic/tools/sdk/stage_assets.py before the MPP build')
    assets = sorted(os.path.join(stage, name) for name in os.listdir(stage)
                    if os.path.isfile(os.path.join(stage, name)))
    target = '#/' + PRJ_OUT_DIR + PRJ_CHIP + '.elf'
    Depends(target, [File(path) for path in assets])
    Depends(target, Value('\n'.join(os.path.basename(path) for path in assets)))

Return('group')
