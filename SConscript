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
# Keep premultiplied FILE/VARIABLE software fallback consistent with GE.
import runpy
stage_sw = runpy.run_path(os.path.join(cwd, 'tools', 'sdk', 'stage_sw_premult.py'))
generated_sw = stage_sw['generate'](
    os.path.join(lvgl_root, 'src', 'draw', 'sw', 'lv_draw_sw_img.c'),
    os.path.join(AIC_ROOT, 'build', 'lvgl-sw-image.c'))
src = [source for source in src if os.path.basename(str(source)) != 'lv_draw_sw_img.c']
src += [File(generated_sw)]

stage_rotation = runpy.run_path(os.path.join(cwd, 'tools', 'sdk', 'stage_sw_rotation.py'))
generated_rotation = stage_rotation['generate'](lvgl_root, os.path.join(AIC_ROOT, 'build'))
src = [source for source in src if os.path.basename(str(source)) not in ('lv_area.c', 'lv_draw_sw_transform.c', 'lv_image.c')]
src += [File(path) for path in generated_rotation]

if GetDepend('AIC_LVGL_USE_VECTOR'):
    stage_vector = runpy.run_path(os.path.join(cwd, 'tools', 'sdk', 'stage_vector.py'))
    generated_vector = stage_vector['generate'](
        os.path.join(lvgl_root, 'src', 'draw', 'sw', 'lv_draw_sw_vector.c'),
        os.path.join(AIC_ROOT, 'build', 'lvgl-sw-vector.c'))
    src = [source for source in src if os.path.basename(str(source)) != 'lv_draw_sw_vector.c']
    src += [File(generated_vector)]

if GetDepend('AIC_LVGL_USE_SVG'):
    stage_svg = runpy.run_path(os.path.join(cwd, 'tools', 'sdk', 'stage_svg.py'))
    generated_svg = stage_svg['generate'](lvgl_root, os.path.join(AIC_ROOT, 'build'))
    src = [source for source in src if os.path.basename(str(source)) not in ('lv_draw_image.c', 'lv_svg_decoder.c', 'lv_svg_render.c')]
    src += [File(path) for path in generated_svg]

demo_enabled = (GetDepend('AIC_LVGL_BUILD_DEMO_WIDGETS') or GetDepend('AIC_LVGL_BUILD_DEMO_BENCHMARK') or
                GetDepend('AIC_LVGL_BUILD_DEMO_MUSIC'))
if demo_enabled:
    src += Glob('../lvgl/demos/lv_demos.c', ondisk=True, source=True)
    demo_dirs = []
    if GetDepend('AIC_LVGL_BUILD_DEMO_WIDGETS') or GetDepend('AIC_LVGL_BUILD_DEMO_BENCHMARK'):
        demo_dirs += ['widgets']
    if GetDepend('AIC_LVGL_BUILD_DEMO_MUSIC'):
        demo_dirs += ['music']
    if GetDepend('AIC_LVGL_BUILD_DEMO_BENCHMARK'):
        demo_dirs += ['benchmark']
    for demo in demo_dirs:
        for directory, directories, files in os.walk(os.path.join(lvgl_root, 'demos', demo)):
            directories.sort()
            for name in sorted(files):
                if not name.endswith('.c'):
                    continue
                if name in ('lv_demo_widgets.c', 'lv_demo_benchmark.c', 'lv_demo_music.c', 'lv_demo_music_main.c'):
                    src += [File('compat/' + name.replace('lv_demo_', 'lv_aic_demo_'))]
                else:
                    relative = os.path.relpath(directory, cwd).replace(os.sep, '/')
                    src += Glob(relative + '/' + name, ondisk=True, source=True)
# Official SDK demos, vendored verbatim under demos/official/<sdk dir>.
# (Kconfig symbol, runner name, SDK aic_demo directory)
OFFICIAL_DEMOS = (('AIC_LVGL_OFFICIAL_DEMO_METER', 'meter', 'meter_demo'),
                  ('AIC_LVGL_OFFICIAL_DEMO_DASHBOARD', 'dashboard', 'dashboard_demo'),
                  ('AIC_LVGL_OFFICIAL_DEMO_SLIDE', 'slide', 'slide_demo'),
                  ('AIC_LVGL_OFFICIAL_DEMO_MULTI_LANG', 'multi_lang', 'multi_lang_demo'),
                  ('AIC_LVGL_OFFICIAL_DEMO_DEMO_HUB', 'demo_hub', 'demo_hub'),
                  ('AIC_LVGL_OFFICIAL_DEMO_IMAGE', 'image', 'image_demo'))


def official_installs(name, sdk_dir):
    """(source relative to the demo directory, rodata destination) pairs.
    Default: assets/ -> rodata/lvgl_data/<name>. A source file installs into
    a destination ending in '/'."""
    if name == 'demo_hub':
        # Per-resolution sets; the virtual 1024x600 display takes that one.
        return [('assets/1024x600/lvgl_data/', 'rodata/lvgl_data/demo_hub')]
    if name == 'image':
        # Images under its own path, and only the font it opens at the
        # hard-coded /rodata/lvgl_data/font/ (Lato-Regular.ttf is unused).
        assets = os.path.join(cwd, 'demos', 'official', sdk_dir, 'assets')
        return ([('assets/' + f, 'rodata/lvgl_data/image/') for f in sorted(os.listdir(assets))
                 if os.path.isfile(os.path.join(assets, f))] +
                [('assets/font/DroidSansFallback.ttf', 'rodata/lvgl_data/font/')])
    return [('assets/', 'rodata/lvgl_data/' + name)]


# Extra global symbols renamed per demo where two demos define the same one
# (demo_hub's dashboard app and dashboard_demo both have dashboard_ui_init).
OFFICIAL_RENAMES = {'demo_hub': ['dashboard_ui_init']}


def official_tree(sdk_dir):
    """C sources and include directories of a vendored demo (any depth,
    e.g. multi_lang_demo/screen), excluding its assets. Every directory is an
    include root: demo_hub includes "./components/x.h" relative to app/common."""
    root = os.path.join(cwd, 'demos', 'official', sdk_dir)
    sources, paths = [], []
    for directory, subdirs, files in os.walk(root):
        subdirs[:] = sorted(d for d in subdirs if d != 'assets')
        paths.append(directory)
        # Relative to this SConscript so objects land in the variant (output)
        # tree; an absolute File() is built in place, inside the submodule.
        relative = os.path.relpath(directory, cwd).replace(os.sep, '/')
        sources += [File(relative + '/' + f) for f in sorted(files) if f.endswith('.c')]
    return sources, paths


official_demos = []
if GetDepend('AIC_LVGL_OFFICIAL_DEMOS'):
    src += [File('demos/official/lv_aic_official_demo.c')]
    official_demos = [demo for demo in OFFICIAL_DEMOS if GetDepend(demo[0])]
if GetDepend('AIC_LVGL_USE_LOTTIE'):
    src = [source for source in src if os.path.basename(str(source)) != 'lv_lottie.c']
    src += [File('compat/lv_aic_lottie.c'), File('widgets/lv_aic_lottie_resource.c')]
src += [File('compat/lvgl_aic_config_probe.c'), File('compat/lv_aic_rtthread_os.c')]
includes = [os.path.join(lvgl_root, 'src', 'widgets', 'image'), os.path.join(lvgl_root, 'src', 'misc'), os.path.join(lvgl_root, 'src', 'draw'), os.path.join(lvgl_root, 'src', 'image', 'svg'),
            os.path.join(lvgl_root, 'src', 'draw', 'sw'), cwd, os.path.join(cwd, 'include'), os.path.join(cwd, 'compat'),
            lvgl_root, os.path.join(lvgl_root, 'include'), os.path.join(lvgl_root, 'include', 'lvgl'),
            os.path.join(lvgl_root, 'src', 'osal'), os.path.join(lvgl_root, 'env_support', 'rt-thread')]
group = DefineGroup('Application-LVGL-9.6', src, depend=['AIC_LVGL_PORT'],
    CPPPATH=includes, CPPDEFINES=['LV_KCONFIG_IGNORE=1', 'LV_BUILD_EXAMPLES=0', 'LV_BUILD_DEMOS=' + ('1' if demo_enabled else '0')])
for symbol, name, sdk_dir in official_demos:
    # One group per demo so its LOCAL_ defines stay private: the SDK sources
    # are compiled unmodified, with ui_init/ui_font_regular renamed so several
    # demos link together, and a per-demo storage path (compat/aic_ui.h turns
    # the AIC_OFFICIAL_DEMO_NAME token into "/rodata/lvgl_data/<name>").
    # Assets ride INSTALL -> rodata.fatfs; the trailing separator is
    # load-bearing: fsinstall.py derives the destination suffix with
    # str.replace(srcpath, ''), otherwise files scatter outside the image.
    demo_dir = 'demos/official/' + sdk_dir
    demo_sources, demo_paths = official_tree(sdk_dir)
    compat_header = os.path.join(cwd, 'demos', 'official', 'compat',
                                 'lv_aic_v8_compat.h').replace('\\', '/')
    group += DefineGroup('Application-LVGL-Official-' + name,
                         demo_sources,
                         depend=[symbol],
                         # v8 names for files that never include aic_ui.h.
                         LOCAL_CCFLAGS=' -include ' + compat_header,
                         LOCAL_CPPPATH=[os.path.join(cwd, 'demos', 'official', 'compat')] + demo_paths,
                         LOCAL_CPPDEFINES=['ui_init=lv_aic_official_%s_ui_init' % name,
                                           'ui_font_regular=lv_aic_official_%s_font' % name,
                                           'AIC_OFFICIAL_DEMO_NAME=' + name] +
                                          ['%s=%s_%s' % (sym, name, sym)
                                           for sym in OFFICIAL_RENAMES.get(name, [])],
                         INSTALL=[(demo_dir + '/' + source, destination)
                                  for source, destination in official_installs(name, sdk_dir)])
if GetDepend('AIC_LVGL_USE_VECTOR'):
    # Keep C++ flags local to the pinned software vector backend.
    excluded_loaders = ('tvgSvg', 'tvgXmlParser')
    if not GetDepend('AIC_LVGL_USE_LOTTIE'):
        excluded_loaders += ('tvgLottie',)
    vector_src = [source for source in Glob('../lvgl/src/libs/thorvg/*.cpp', ondisk=True, source=True)
                  if not os.path.basename(str(source)).startswith(excluded_loaders)]
    if GetDepend('AIC_LVGL_USE_LOTTIE'):
        import runpy
        stage_lottie = runpy.run_path(os.path.join(cwd, 'tools', 'sdk', 'stage_lottie.py'))
        generated_lottie = stage_lottie['generate'](
            os.path.join(lvgl_root, 'src', 'libs', 'thorvg', 'tvgLottieBuilder.cpp'),
            os.path.join(AIC_ROOT, 'build', 'lvgl-lottie-builder.cpp'))
        vector_src = [source for source in vector_src if os.path.basename(str(source)) != 'tvgLottieBuilder.cpp']
        vector_src += [File(generated_lottie)]
    vector_src += [File('compat/lvgl_aic_thorvg_probe.cpp')]
    group += DefineGroup('Application-LVGL-ThorVG', vector_src, depend=['AIC_LVGL_PORT'],
                         LOCAL_CXXFLAGS=' -std=c++14 -fno-exceptions -fno-rtti -fno-sized-deallocation -include lvgl_aic_thorvg_config.h',
                         CPPPATH=includes + [os.path.join(lvgl_root, 'src', 'libs', 'thorvg')])
    if GetDepend('AIC_LVGL_SMOKE_APP'):
        for api in ('lvgl_aic_thorvg_config_probe', 'lv_draw_vector', 'lv_vector_path_create', 'lv_draw_vector_dsc_create'):
            Env.AppendUnique(LINKFLAGS=['-Wl,-u,' + api])
if GetDepend('AIC_LVGL_USE_LOTTIE') and GetDepend('AIC_LVGL_SMOKE_APP'):
    for api in ('lv_lottie_create', 'lv_lottie_set_buffer', 'lv_lottie_set_draw_buf',
                'lv_lottie_set_src_data', 'lv_lottie_set_src_file', 'lv_lottie_get_anim',
                'lv_aic_lottie_load_data', 'lv_aic_lottie_load_file'):
        Env.AppendUnique(LINKFLAGS=['-Wl,-u,' + api])
if GetDepend('AIC_LVGL_USE_SVG') and GetDepend('AIC_LVGL_SMOKE_APP'):
    for api in ('lv_svg_decoder_init', 'lv_svg_load_data', 'lv_svg_render_create', 'lv_draw_svg_render'):
        Env.AppendUnique(LINKFLAGS=['-Wl,-u,' + api])
if demo_enabled and GetDepend('AIC_LVGL_SMOKE_APP'):
    if GetDepend('AIC_LVGL_BUILD_DEMO_WIDGETS') or GetDepend('AIC_LVGL_BUILD_DEMO_BENCHMARK'):
        for api in ('lv_demo_widgets', 'lv_demo_widgets_with_args'):
            Env.AppendUnique(LINKFLAGS=['-Wl,-u,' + api])
    if GetDepend('AIC_LVGL_BUILD_DEMO_MUSIC'):
        for api in ('lv_demo_music', 'lv_demo_music_with_args', 'lv_demo_music_play',
                    'lv_demo_music_pause', 'lv_demo_music_resume', 'lv_demo_music_album_next'):
            Env.AppendUnique(LINKFLAGS=['-Wl,-u,' + api])
    if GetDepend('AIC_LVGL_BUILD_DEMO_BENCHMARK'):
        for api in ('lv_demo_benchmark', 'lv_demo_benchmark_set_end_cb', 'lv_demo_benchmark_summary_display'):
            Env.AppendUnique(LINKFLAGS=['-Wl,-u,' + api])
if GetDepend('AIC_LVGL_SMOKE_APP') and official_demos:
    for api in ('lv_aic_official_demo_show', 'lv_aic_official_demo_close',
                'lv_aic_demo_test_poll', 'lv_aic_demo_test_deinit'):
        Env.AppendUnique(LINKFLAGS=['-Wl,-u,' + api])
if GetDepend('AIC_LVGL_SMOKE_APP') and GetDepend('AIC_LVGL_USE_CAN_CAPTURE'):
    for api in ('poll', 'deinit'):
        Env.AppendUnique(LINKFLAGS=['-Wl,-u,lv_aic_can_capture_' + api])
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
if GetDepend('AIC_LVGL_USE_FT_CACHE'):
    src += Glob('font/ft_cache/*.c')
    for api in ('get_stats', 'print_stats', 'drop_all', 'drop_specific'):
        Env.AppendUnique(LINKFLAGS=['-Wl,-u,aic_lv_ft_cache_' + api])
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
        for api in ('lv_aic_spi_display_create_pipelined', 'lv_aic_spi_display_poll', 'lv_aic_spi_pipeline_create', 'lv_aic_spi_pipeline_submit', 'lv_aic_spi_pipeline_take', 'lv_aic_spi_pipeline_stop', 'lv_aic_spi_pipeline_close', 'lv_aic_spi_display_stats', 'lv_aic_spi_display_claim_blit', 'lv_aic_spi_display_blit', 'lv_aic_spi_display_blit_take', 'lv_aic_spi_display_create_buffered', 'lv_aic_spi_display_create', 'lv_aic_spi_display_get', 'lv_aic_spi_display_result', 'lv_aic_spi_display_close', 'lv_aic_spi_panel_set_lifecycle', 'lv_aic_spi_panel_create', 'lv_aic_spi_panel_prepare', 'lv_aic_spi_panel_close', 'lv_aic_spi_session_enable_ge2d', 'lv_aic_spi_session_enable_overlap', 'lv_aic_spi_session_open_owned', 'lv_aic_spi_session_close'):
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
                'set_width', 'set_height', 'set_pivot', 'get_pivot', 'set_rotation', 'get_rotation', 'set_scale', 'get_scale', 'set_scale_x', 'get_scale_x', 'set_scale_y', 'get_scale_y', 'set_offset_x', 'get_offset_x', 'set_offset_y', 'get_offset_y', 'set_inner_align', 'get_inner_align',
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

# Camera smoke roots verify closure without creating a widget or starting VIN.
if GetDepend('AIC_LVGL_SMOKE_APP') and GetDepend('AIC_LVGL_USE_CAMERA'):
    for api in ('create', 'configure', 'set_format', 'set_channel', 'get_channel',
                'get_channel_status', 'set_video_plane', 'open', 'start', 'stop',
                'pause', 'resume', 'close', 'get_state', 'pending_cleanup',
                'barcode_enable', 'barcode_disable', 'barcode_only', 'barcode_callback'):
        Env.AppendUnique(LINKFLAGS=['-Wl,-u,lv_aic_camera_' + api])

Return('group')
