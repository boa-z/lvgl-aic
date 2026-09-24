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

    cppdefines = [
        'AIC_LVGL_BSP_RTTHREAD=%d' % (1 if GetDepend('KERNEL_RTTHREAD') else 0),
        'AIC_LVGL_BSP_MPP=%d' % (1 if GetDepend('LPKG_MPP') else 0),
        'AIC_LVGL_DEBUG_STATS=%d' % (1 if GetDepend('AIC_LVGL_DEBUG_STATS') else 0),
    ]
    for symbol in (
            'AIC_LVGL_USE_DISPLAY',
            'AIC_LVGL_USE_TOUCH',
            'AIC_LVGL_USE_ENCODER',
            'AIC_LVGL_USE_MOUSE',
            'AIC_LVGL_USE_GE2D',
            'AIC_LVGL_USE_MPP_DEC',
            'AIC_LVGL_USE_FT_CACHE',
            'AIC_LVGL_MANUAL_TEST'):
        cppdefines.append('%s=%d' % (symbol, 1 if GetDepend(symbol) else 0))

    group = DefineGroup(
        'AIC-LVGL',
        src,
        depend=['LPKG_USING_LVGL', 'AIC_LVGL_PORT'],
        CPPPATH=cpppath,
        CPPDEFINES=cppdefines,
    )

Return('group')
