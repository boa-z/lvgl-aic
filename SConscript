# SPDX-License-Identifier: Apache-2.0
from building import *
import os

Import('AIC_ROOT')
cwd = GetCurrentDir()
group = []
if not GetDepend('AIC_LVGL_PORT'):
    Return('group')
if GetDepend('LPKG_USING_LVGL'):
    raise RuntimeError('Application LVGL cannot be combined with the SDK legacy LVGL package')
# 部分 SDK 版本只导出 RT_USING_EVENT，不导出 KERNEL_RTTHREAD 别名；事件
# 同步接口已由 RT-Thread 提供，不能因别名缺失误拒绝应用级 LVGL。
if not GetDepend('LPKG_MPP') or not (GetDepend('KERNEL_RTTHREAD') or GetDepend('RT_USING_EVENT')):
    raise RuntimeError('lvgl-aic requires the RT-Thread and MPP platform interfaces')
if not GetDepend('RT_USING_EVENT'):
    raise RuntimeError('Application LVGL synchronization requires RT_USING_EVENT')

# 应用固定相邻 third_party/lvgl，不从 SDK packages 寻找可变依赖。
lvgl_root = os.path.abspath(os.path.join(cwd, '..', 'lvgl'))
version_header = os.path.join(lvgl_root, 'include', 'lvgl', 'lv_version.h')
import io
with io.open(version_header, encoding='utf-8') as stream:
    version = stream.read()
if '#define LVGL_VERSION_MAJOR 9' not in version or '#define LVGL_VERSION_MINOR 6' not in version:
    raise RuntimeError('Application third_party/lvgl must pin LVGL 9.6.x')

src = []
for directory, directories, files in os.walk(os.path.join(lvgl_root, 'src')):
    # ArtInChip uses this component's GE2D backend, never the separate VG Lite
    # kernel/HAL. Skip disabled foreign driver units (their nested includes
    # also exceed the bundled Windows GCC path limit in long app checkouts).
    if os.path.relpath(directory, lvgl_root).replace(os.sep, '/') == 'src/libs':
        directories[:] = [name for name in directories if name != 'vg_lite_driver']
    directories.sort()
    relative = os.path.relpath(directory, cwd).replace(os.sep, '/')
    src += Glob(relative + '/*.c', ondisk=True, source=True)
if not src:
    raise RuntimeError('Application LVGL sources are missing')
src += [File('compat/lvgl_aic_config_probe.c'), File('compat/lv_aic_rtthread_os.c')]
includes = [cwd, os.path.join(cwd, 'include'), os.path.join(cwd, 'compat'),
            lvgl_root, os.path.join(lvgl_root, 'include'), os.path.join(lvgl_root, 'include', 'lvgl'),
            os.path.join(lvgl_root, 'src', 'osal'), os.path.join(lvgl_root, 'env_support', 'rt-thread')]
group = DefineGroup('Application-LVGL-9.6', src, depend=['AIC_LVGL_PORT'],
    CPPPATH=includes, CPPDEFINES=['LV_KCONFIG_IGNORE=1', 'LV_BUILD_EXAMPLES=0', 'LV_BUILD_DEMOS=0'])
# SCons must receive two tokens; DefineGroup treats a CCFLAGS string as one.
Env.AppendUnique(CCFLAGS=['-include', 'lvgl_aic_build_config.h'])

# SDK codec callers ignore a failed finite VE lock wait. Keep arbitration
# inside the application link boundary until the driver grants ownership.
if GetDepend('AIC_LVGL_USE_PLAYER_SESSION') or GetDepend('AIC_LVGL_USE_APNG'):
    Env.AppendUnique(LINKFLAGS=['-Wl,--wrap=ve_get_client'])

if GetDepend('AIC_LVGL_USE_BARCODE'):
    if GetDepend('AIC_USING_BARCODE_DEMO'):
        raise RuntimeError('Application barcode requires exclusive decoder ownership')
    barcode = os.path.join(AIC_ROOT, 'packages', 'artinchip', 'barcode')
    includes += [os.path.join(barcode, 'include')]
    Env.AppendUnique(LIBPATH=[os.path.join(barcode, 'lib')], LIBS=['decoder.a'])
    Env.AppendUnique(LINKFLAGS=['-Wl,-u,lv_aic_barcode_decode'])

src = Glob('port/*.c') + Glob('image/mpp/*.c') + Glob('common/*.c') + Glob('draw/ge2d/*.c')
if GetDepend('AIC_LVGL_USE_CANVAS'):
    src += Glob('widgets/lv_aic_canvas.c')
    for api in ('lv_ge_fill', 'lv_mpp_image_alloc', 'lv_mpp_image_flush_cache', 'lv_mpp_image_free', 'lv_aic_mpp_image_alloc_bounded'):
        Env.AppendUnique(LINKFLAGS=['-Wl,-u,' + api])
    for api in ('create', 'set_budget', 'alloc_buffer', 'draw_text', 'draw_text_to_center'):
        Env.AppendUnique(LINKFLAGS=['-Wl,-u,lv_aic_canvas_' + api])
if GetDepend('AIC_LVGL_USE_IMG_ROLLER'):
    src += Glob('widgets/lv_img_roller.c')
if GetDepend('AIC_LVGL_USE_SWIPE_V1'):
    src += Glob('widgets/lv_swipe_v1.c')
if GetDepend('AIC_LVGL_USE_VIDEO_WINDOW'):
    src += Glob('widgets/lv_aic_video_window.c')
if GetDepend('AIC_LVGL_USE_PLAYER') or GetDepend('AIC_LVGL_USE_APNG_WIDGET'):
    src += Glob('widgets/lv_aic_player_control.c')
if GetDepend('AIC_LVGL_USE_PLAYER'):
    src += Glob('widgets/lv_aic_player.c')
if GetDepend('AIC_LVGL_USE_APNG_WIDGET'):
    src += Glob('widgets/lv_aic_apng_widget.c')
if GetDepend('AIC_LVGL_USE_CAMERA'):
    src += Glob('widgets/lv_aic_camera.c')
if GetDepend('AIC_LVGL_MANUAL_TEST'):
    src += Glob('tests/manual/*.c')
includes += [os.path.join(cwd, 'common'), os.path.join(cwd, 'image', 'mpp'),
             os.path.join(cwd, 'draw', 'ge2d'), os.path.join(AIC_ROOT, 'packages', 'artinchip', 'mpp', 'include'),
             os.path.join(AIC_ROOT, 'packages', 'artinchip', 'mpp', 've', 'include'),
             os.path.join(AIC_ROOT, 'bsp', 'artinchip', 'include', 'uapi')]
if GetDepend('AIC_LVGL_USE_SPI_SDK'):
    # Smoke profile links enabled SPI APIs without initializing a panel or bus.
    if GetDepend('AIC_LVGL_SMOKE_APP'):
        for api in ('lv_aic_spi_display_create_buffered', 'lv_aic_spi_display_create', 'lv_aic_spi_display_get', 'lv_aic_spi_display_result', 'lv_aic_spi_display_close', 'lv_aic_spi_panel_create', 'lv_aic_spi_panel_prepare', 'lv_aic_spi_panel_close', 'lv_aic_spi_session_open_owned', 'lv_aic_spi_session_close'):
            Env.AppendUnique(LINKFLAGS=['-Wl,-u,' + api])
    includes += [os.path.join(AIC_ROOT, 'bsp', 'artinchip', 'include', 'hal')]

group += DefineGroup('Application-LVGL-AIC', src, depend=['AIC_LVGL_PORT'], CPPPATH=includes,
                     CPPDEFINES=['AIC_LVGL_BSP_RTTHREAD=1', 'AIC_LVGL_BSP_MPP=1'])

# 兼容旧应用的原生 list/menu 入口仅在控件 smoke 配置中保留，不自动创建 UI。
if GetDepend('AIC_LVGL_SMOKE_APP') and GetDepend('AIC_LVGL_USE_IMG_ROLLER') and GetDepend('AIC_LVGL_USE_SWIPE_V1'):
    for api in ('lv_list_create', 'lv_list_add_text', 'lv_list_add_button', 'lv_list_get_button_text',
                'lv_menu_create', 'lv_menu_page_create', 'lv_menu_cont_create',
                'lv_menu_set_load_page_event', 'lv_menu_set_page', 'lv_menu_get_cur_main_page'):
        Env.AppendUnique(LINKFLAGS=['-Wl,-u,' + api])

# 媒体构建配置保留完整控件入口，验证链接闭包；不自动播放或占用设备。
if GetDepend('AIC_LVGL_SMOKE_APP') and GetDepend('AIC_LVGL_USE_PLAYER'):
    for api in ('create', 'configure', 'set_video_plane', 'set_video_plane_rotation_budget', 'set_src', 'start', 'stop', 'close',
                'pause', 'resume', 'seek', 'set_auto_restart', 'get_auto_restart',
                'get_auto_restart_count', 'set_group', 'get_group', 'set_rate', 'get_rate', 'set_volume', 'get_state', 'get_status', 'pending_cleanup'):
        Env.AppendUnique(LINKFLAGS=['-Wl,-u,lv_aic_player_' + api])

    for api in ('create', 'add', 'remove', 'get_count', 'control'):
        Env.AppendUnique(LINKFLAGS=['-Wl,-u,lv_aic_player_group_' + api])

    for api in ('create', 'set_master', 'get_master'):
        Env.AppendUnique(LINKFLAGS=['-Wl,-u,lv_aic_slave_player_' + api])

if GetDepend('AIC_LVGL_SMOKE_APP') and (GetDepend('AIC_LVGL_USE_PLAYER') or GetDepend('AIC_LVGL_USE_APNG_WIDGET')):
    Env.AppendUnique(LINKFLAGS=['-Wl,-u,lv_aic_player_control', '-Wl,-u,lv_aic_player_get_media_info'])

if GetDepend('AIC_LVGL_SMOKE_APP') and GetDepend('AIC_LVGL_USE_PLAYER') and GetDepend('AIC_LVGL_USE_APNG'):
    Env.AppendUnique(LINKFLAGS=['-Wl,-u,lv_aic_player_configure_apng'])

# Keep all APNG widget roots live in the opt-in smoke profile. No autoplay.
if GetDepend('AIC_LVGL_SMOKE_APP') and GetDepend('AIC_LVGL_USE_APNG_WIDGET'):
    for api in ('create', 'configure', 'set_src', 'start', 'pause', 'set_rate',
                'restart', 'close', 'get_status', 'pending_cleanup',
                'slave_create', 'slave_set_master', 'slave_get_master'):
        Env.AppendUnique(LINKFLAGS=['-Wl,-u,lv_aic_apng_' + api])

# 打包素材变更必须使最终镜像失效，避免复用旧素材。
if GetDepend('AIC_LVGL_USE_MPP_DEC') and GetDepend('AIC_LVGL_SMOKE_APP'):
    Import('PRJ_OUT_DIR', 'PRJ_CHIP')
    from SCons.Script import Depends, File, Value
    stage = os.path.join(AIC_ROOT, 'build', 'lvgl-mpp-data', 'mpp_test')
    if not os.path.isfile(os.path.join(stage, 'SHA256.json')):
        raise RuntimeError('Run this component tools/sdk/stage_assets.py before the MPP build')
    assets = sorted(os.path.join(stage, name) for name in os.listdir(stage) if os.path.isfile(os.path.join(stage, name)))
    target = '#/' + PRJ_OUT_DIR + PRJ_CHIP + '.elf'
    Depends(target, [File(path) for path in assets])
    Depends(target, Value(str([os.path.basename(path) for path in assets])))

if GetDepend('AIC_LVGL_SMOKE_APP') and GetDepend('AIC_LVGL_USE_VIDEO_PLANE'):
    for api in ('open', 'enable_ui_alpha', 'present', 'present_rotated', 'hide', 'close', 'faulted'):
        Env.AppendUnique(LINKFLAGS=['-Wl,-u,lv_aic_video_plane_' + api])

if GetDepend('AIC_LVGL_USE_GE2D'):
    Env.AppendUnique(LINKFLAGS=['-Wl,--wrap=lv_draw_buf_create'])

# 应用内生成经过指纹校验的 CMDQ 后端，不修改 SDK 源文件。
# 链接替换仅作用于本应用；SDK 版本变化必须先复核补丁。
if GetDepend('AIC_LVGL_USE_GE2D') and GetDepend('AIC_GE_CMDQ'):
    import runpy
    from SCons.Script import File
    stage_ge = runpy.run_path(os.path.join(cwd, 'tools', 'sdk', 'stage_ge_cmdq.py'))
    generated_ge = stage_ge['generate'](AIC_ROOT, os.path.join(AIC_ROOT, 'build', 'lvgl-ge-cmdq.c'))
    ge_paths = includes + [os.path.join(AIC_ROOT, 'packages', 'artinchip', 'mpp', 'ge', 'include'),
                          os.path.join(AIC_ROOT, 'packages', 'artinchip', 'mpp', 'base', 'include')]
    group += DefineGroup('Application-GE-CMDQ', [File(generated_ge)], depend=['AIC_LVGL_PORT'],
                         CPPPATH=ge_paths)
    Env.AppendUnique(LINKFLAGS=['-Wl,--wrap=ge_cmdq_ops'])

Return('group')
