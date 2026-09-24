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

    if GetDepend('AIC_LVGL_MANUAL_TEST'):
        src += Glob('tests/manual/*.c')

    lvgl_root = os.path.join(AIC_ROOT, 'packages', 'third-party', 'lvgl')
    cpppath = [
        cwd,
        os.path.join(cwd, 'include'),
        os.path.join(cwd, 'compat'),
        os.path.join(lvgl_root, 'include'),
        os.path.join(lvgl_root, 'include', 'lvgl'),
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

Return('group')
