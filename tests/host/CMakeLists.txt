cmake_minimum_required(VERSION 3.20)
project(lvgl_aic_host_smoke LANGUAGES C)

set(AIC_ROOT "${CMAKE_CURRENT_LIST_DIR}/../.." CACHE PATH "lvgl-aic repository root")
set(LVGL_ROOT "" CACHE PATH "Separate LVGL v9.6.0 checkout")

if (LVGL_ROOT STREQUAL "")
    message(FATAL_ERROR "Set -DLVGL_ROOT to an external LVGL v9.6.0 checkout")
endif()
option(AIC_BUILD_GIF_TESTS "Test native LVGL GIF widget" OFF)
option(AIC_BUILD_AICP_TESTS "Test SDK AICP source/header contracts" OFF)
option(AIC_BUILD_AICP_V31_TESTS "Enable V31 AICP alpha contract" OFF)
option(AIC_BUILD_INPUT_TESTS "Test application-owned encoder and mouse providers" OFF)
option(AIC_BUILD_WIDGET_TESTS "Test native LVGL widget surface" OFF)
option(AIC_BUILD_IMG_ROLLER_TESTS "Test the adapted SDK image roller" OFF)
option(AIC_BUILD_SWIPE_TESTS "Test the adapted SDK swipe widget" OFF)
option(AIC_BUILD_CONTROL_WIDGET_TESTS "Test common native LVGL control widgets" OFF)
option(AIC_BUILD_MANUAL_PREVIEW "Build native software reference screenshots" OFF)
option(AIC_BUILD_FREETYPE_TESTS "Test native FreeType and glyph caches" OFF)
option(AIC_BUILD_SDL_SMOKE "Build the interactive SDL2 smoke window" OFF)
option(AIC_BUILD_SDL_DEMOS "Build the official LVGL 9.6 demos for the SDL target" OFF)
if(AIC_BUILD_SDL_DEMOS AND NOT AIC_BUILD_SDL_SMOKE)
    message(FATAL_ERROR "AIC_BUILD_SDL_DEMOS requires AIC_BUILD_SDL_SMOKE=ON")
endif()
if(AIC_BUILD_SDL_DEMOS)
    set(LV_BUILD_CONF_PATH "${AIC_ROOT}/tests/host/lv_conf_sdl_demos.h" CACHE FILEPATH "" FORCE)
    set(CONFIG_LV_BUILD_DEMOS ON CACHE BOOL "" FORCE)
else()
    set(LV_BUILD_CONF_PATH "${AIC_ROOT}/lv_conf.h" CACHE FILEPATH "" FORCE)
    set(CONFIG_LV_BUILD_DEMOS OFF CACHE BOOL "" FORCE)
endif()
set(CONFIG_LV_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(CONFIG_LV_USE_THORVG OFF CACHE BOOL "" FORCE)
set(CONFIG_LV_USE_THORVG_INTERNAL OFF CACHE BOOL "" FORCE)
set(LV_BUILD_SET_CONFIG_OPTS OFF CACHE BOOL "" FORCE)
set(LV_FETCH_DEPENDENCIES OFF CACHE BOOL "" FORCE)
set(LV_USE_FIND_PACKAGE OFF CACHE BOOL "" FORCE)
set(LV_USE_PKG_CONFIG OFF CACHE BOOL "" FORCE)

add_subdirectory("${LVGL_ROOT}" lvgl EXCLUDE_FROM_ALL)
if(AIC_BUILD_FREETYPE_TESTS)
    find_package(Freetype REQUIRED)
    target_compile_definitions(lvgl PUBLIC LV_USE_FREETYPE=1)
    target_link_libraries(lvgl PUBLIC Freetype::Freetype)
endif()

if(AIC_BUILD_GIF_TESTS)
    target_compile_definitions(lvgl PUBLIC AIC_LVGL_USE_GIF=1)
endif()
if(AIC_BUILD_MANUAL_PREVIEW)
    if(NOT AIC_BUILD_FREETYPE_TESTS)
        message(FATAL_ERROR "AIC_BUILD_MANUAL_PREVIEW requires AIC_BUILD_FREETYPE_TESTS")
    endif()
    target_compile_definitions(lvgl PUBLIC LV_USE_LODEPNG=1 LV_USE_TJPGD=1)
endif()

set(AIC_SOURCES
    "${AIC_ROOT}/common/lv_aic_yuv.c"
    "${AIC_ROOT}/image/mpp/lv_aic_yuv_image.c"
    "${AIC_ROOT}/port/lv_aic.c"
    "${AIC_ROOT}/port/lv_aic_display.c"
    "${AIC_ROOT}/port/lv_aic_indev.c"
    "${AIC_ROOT}/common/lv_aic_pixel_format.c"
    "${AIC_ROOT}/image/mpp/lv_aic_mpp_format.c"
    "${AIC_ROOT}/image/mpp/lv_aic_mpp_stream.c"
    "${AIC_ROOT}/image/mpp/lv_aic_mpp_decoder.c")
list(APPEND AIC_SOURCES "${AIC_ROOT}/image/mpp/lv_aic_bmp_header.c")
add_library(lvgl_aic STATIC ${AIC_SOURCES})
target_include_directories(lvgl_aic
    PUBLIC "${AIC_ROOT}/include"
    PRIVATE "${AIC_ROOT}/compat"
    PRIVATE "${AIC_ROOT}/common"
    PRIVATE "${AIC_ROOT}/image/mpp")
target_compile_definitions(lvgl_aic PRIVATE
    AIC_LVGL_BSP_MPP=0
    AIC_LVGL_BSP_RTTHREAD=0
    AIC_LVGL_USE_DISPLAY=1
    AIC_LVGL_USE_TOUCH=0
    AIC_LVGL_USE_MPP_DEC=0)
target_link_libraries(lvgl_aic PUBLIC lvgl)
if(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(lvgl_aic PRIVATE -Wall -Wextra -Werror)
endif()

add_executable(lvgl_aic_smoke
    smoke.c
    "${AIC_ROOT}/tests/manual/lv_aic_manual_test.c")
target_include_directories(lvgl_aic_smoke PRIVATE
    "${AIC_ROOT}/include"
    "${AIC_ROOT}/tests/manual")
target_compile_definitions(lvgl_aic_smoke PRIVATE
    AIC_LVGL_BSP_MPP=0
    AIC_LVGL_BSP_RTTHREAD=0
    AIC_LVGL_USE_DISPLAY=1
    AIC_LVGL_USE_TOUCH=0)
target_link_libraries(lvgl_aic_smoke PRIVATE lvgl)
if(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(lvgl_aic_smoke PRIVATE -Wall -Wextra -Werror)
endif()

enable_testing()
add_executable(lvgl_aic_yuv_image_contract yuv_image_contract.c
    "${AIC_ROOT}/common/lv_aic_yuv.c" "${AIC_ROOT}/image/mpp/lv_aic_yuv_image.c")
target_include_directories(lvgl_aic_yuv_image_contract PRIVATE "${AIC_ROOT}/include" "${AIC_ROOT}/compat")
target_link_libraries(lvgl_aic_yuv_image_contract PRIVATE lvgl)
if(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(lvgl_aic_yuv_image_contract PRIVATE -Wall -Wextra -Werror -UNDEBUG)
endif()
add_test(NAME lvgl_aic_yuv_image_contract COMMAND lvgl_aic_yuv_image_contract)
add_executable(lvgl_aic_yuv_contract yuv_contract.c "${AIC_ROOT}/common/lv_aic_yuv.c")
target_include_directories(lvgl_aic_yuv_contract PRIVATE "${AIC_ROOT}/include")
target_link_libraries(lvgl_aic_yuv_contract PRIVATE lvgl)
if(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(lvgl_aic_yuv_contract PRIVATE -Wall -Wextra -Werror -UNDEBUG)
endif()
add_test(NAME lvgl_aic_yuv_contract COMMAND lvgl_aic_yuv_contract)
add_executable(lvgl_aic_fake_image_contract fake_image_contract.c
    "${AIC_ROOT}/common/lv_aic_fake_image.c")
target_include_directories(lvgl_aic_fake_image_contract PRIVATE "${AIC_ROOT}/common")
if(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(lvgl_aic_fake_image_contract PRIVATE -Wall -Wextra -Werror -UNDEBUG)
endif()
add_test(NAME lvgl_aic_fake_image_contract COMMAND lvgl_aic_fake_image_contract)
add_executable(lvgl_aic_bmp_header_contract bmp_header_contract.c
    "${AIC_ROOT}/image/mpp/lv_aic_bmp_header.c")
target_include_directories(lvgl_aic_bmp_header_contract PRIVATE "${AIC_ROOT}/image/mpp")
if(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(lvgl_aic_bmp_header_contract PRIVATE -Wall -Wextra -Werror -UNDEBUG)
endif()
add_test(NAME lvgl_aic_bmp_header_contract COMMAND lvgl_aic_bmp_header_contract)
add_executable(lvgl_aic_os_contract os_contract.c "${AIC_ROOT}/compat/lv_aic_rtthread_os.c")
target_include_directories(lvgl_aic_os_contract PRIVATE "${CMAKE_CURRENT_LIST_DIR}/os_stubs" "${AIC_ROOT}/compat")
if(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(lvgl_aic_os_contract PRIVATE -Wall -Wextra -Werror -UNDEBUG)
endif()
add_test(NAME lvgl_aic_os_contract COMMAND lvgl_aic_os_contract)
add_test(NAME lvgl_aic_platform_smoke COMMAND lvgl_aic_smoke)

# Exercise both manual pages without requiring the board decoder or GE engine.
add_executable(lvgl_aic_manual_pages manual_pages.c
    "${AIC_ROOT}/tests/manual/lv_aic_manual_test.c"
    "${AIC_ROOT}/tests/manual/lv_aic_font_test.c")
target_include_directories(lvgl_aic_manual_pages PRIVATE
    "${AIC_ROOT}/include" "${AIC_ROOT}/tests/manual")
target_compile_definitions(lvgl_aic_manual_pages PRIVATE
    AIC_LVGL_BSP_MPP=0 AIC_LVGL_BSP_RTTHREAD=0
    AIC_LVGL_USE_GE2D=1 AIC_LVGL_USE_MPP_DEC=1)
target_link_libraries(lvgl_aic_manual_pages PRIVATE lvgl)
if(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(lvgl_aic_manual_pages PRIVATE -Wall -Wextra -Werror -UNDEBUG)
endif()
if(AIC_BUILD_MANUAL_PREVIEW)
    target_compile_definitions(lvgl_aic_manual_pages PRIVATE AIC_MANUAL_PREVIEW=1)
    file(MAKE_DIRECTORY "${CMAKE_BINARY_DIR}/preview")
    add_test(NAME lvgl_aic_manual_pages COMMAND lvgl_aic_manual_pages
        "${AIC_ROOT}/tests/data/mpp" "${CMAKE_BINARY_DIR}/preview"
        "${LVGL_ROOT}/examples/libs/freetype/Lato-Regular.ttf"
        "${LVGL_ROOT}/tests/src/test_files/fonts/noto/NotoSansSC-Regular.ttf")
elseif(AIC_BUILD_FREETYPE_TESTS)
    add_test(NAME lvgl_aic_manual_pages COMMAND lvgl_aic_manual_pages
        "${LVGL_ROOT}/examples/libs/freetype/Lato-Regular.ttf"
        "${LVGL_ROOT}/tests/src/test_files/fonts/noto/NotoSansSC-Regular.ttf")
else()
    add_test(NAME lvgl_aic_manual_pages COMMAND lvgl_aic_manual_pages)
endif()

if(AIC_BUILD_SDL_SMOKE)
    find_package(PkgConfig REQUIRED)
    pkg_check_modules(SDL2 REQUIRED IMPORTED_TARGET sdl2)

    # MSYS2's sdl2.pc is intended for SDL_main-based GUI applications. This
    # target owns a native main(), so retain only the SDL2 headers/library.
    if(WIN32)
        get_target_property(_sdl2_cflags PkgConfig::SDL2 INTERFACE_COMPILE_OPTIONS)
        if(_sdl2_cflags)
            list(REMOVE_ITEM _sdl2_cflags "-Dmain=SDL_main")
            set_property(TARGET PkgConfig::SDL2 PROPERTY INTERFACE_COMPILE_OPTIONS "${_sdl2_cflags}")
        endif()
        get_target_property(_sdl2_linkopts PkgConfig::SDL2 INTERFACE_LINK_OPTIONS)
        if(_sdl2_linkopts)
            list(REMOVE_ITEM _sdl2_linkopts "-mwindows")
            set_property(TARGET PkgConfig::SDL2 PROPERTY INTERFACE_LINK_OPTIONS "${_sdl2_linkopts}")
        endif()
        get_target_property(_sdl2_libs PkgConfig::SDL2 INTERFACE_LINK_LIBRARIES)
        if(_sdl2_libs)
            list(FILTER _sdl2_libs EXCLUDE REGEX "SDL2main|mingw32")
            set_property(TARGET PkgConfig::SDL2 PROPERTY INTERFACE_LINK_LIBRARIES "${_sdl2_libs}")
        endif()
    endif()

    add_executable(lvgl-aic-sdl-smoke
        sdl_smoke.c
        "${AIC_ROOT}/tests/manual/lv_aic_manual_test.c")
    set_target_properties(lvgl-aic-sdl-smoke PROPERTIES
        OUTPUT_NAME "lvgl-aic-sdl-smoke")
    target_include_directories(lvgl-aic-sdl-smoke PRIVATE
        "${AIC_ROOT}/include"
        "${AIC_ROOT}/tests/manual")
    target_compile_definitions(lvgl-aic-sdl-smoke PRIVATE
        AIC_LVGL_BSP_MPP=0
        AIC_LVGL_BSP_RTTHREAD=0
        AIC_LVGL_USE_DISPLAY=1
        AIC_LVGL_USE_TOUCH=0)
    if(AIC_BUILD_SDL_DEMOS)
        target_compile_definitions(lvgl-aic-sdl-smoke PRIVATE
            AIC_LVGL_AIC_SDL_DEMOS=1)
        target_link_libraries(lvgl-aic-sdl-smoke PRIVATE
            lvgl PkgConfig::SDL2 lvgl_demos)
    else()
        target_link_libraries(lvgl-aic-sdl-smoke PRIVATE lvgl PkgConfig::SDL2)
    endif()
    if(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
        target_compile_options(lvgl-aic-sdl-smoke PRIVATE -Wall -Wextra -Werror)
    endif()
    add_test(NAME lvgl_aic_sdl_smoke_self
             COMMAND lvgl-aic-sdl-smoke --self-test)
    set_tests_properties(lvgl_aic_sdl_smoke_self PROPERTIES
        ENVIRONMENT "SDL_VIDEODRIVER=dummy;SDL_RENDER_DRIVER=software"
        TIMEOUT 15)
    if(AIC_BUILD_SDL_DEMOS)
        foreach(_aic_demo widgets benchmark stress music keypad_encoder)
            if(_aic_demo STREQUAL "benchmark" OR _aic_demo STREQUAL "music")
                set(_aic_demo_frames 500)
            else()
                set(_aic_demo_frames 120)
            endif()
            add_test(NAME "lvgl_aic_sdl_${_aic_demo}"
                     COMMAND lvgl-aic-sdl-smoke --demo "${_aic_demo}"
                             --frames ${_aic_demo_frames})
            set_tests_properties("lvgl_aic_sdl_${_aic_demo}" PROPERTIES
                ENVIRONMENT "SDL_VIDEODRIVER=dummy;SDL_RENDER_DRIVER=software"
                TIMEOUT 30)
        endforeach()
    endif()
endif()

# Optional real-decoder contract test; SDK headers, mocked engine/OSAL only.
set(AIC_SDK_ROOT "" CACHE PATH "Luban-Lite SDK for MPP ABI tests")
if(AIC_SDK_ROOT)
    add_executable(lvgl_aic_player_allocator_contract player_allocator_contract.c
        "${AIC_ROOT}/port/lv_aic_player_allocator.c"
        "${AIC_ROOT}/common/lv_aic_yuv.c" "${AIC_ROOT}/common/lv_aic_yuv_mpp.c"
        "${AIC_ROOT}/common/lv_aic_pixel_format.c")
    target_include_directories(lvgl_aic_player_allocator_contract PRIVATE
        "${AIC_ROOT}/compat" "${AIC_ROOT}/common" "${AIC_ROOT}/include"
        "${CMAKE_CURRENT_LIST_DIR}/player_stubs"
        "${CMAKE_CURRENT_LIST_DIR}/mpp_stubs"
        "${AIC_SDK_ROOT}/packages/artinchip/mpp/include"
        "${AIC_SDK_ROOT}/bsp/artinchip/include/uapi")
    target_compile_definitions(lvgl_aic_player_allocator_contract PRIVATE AIC_LVGL_USE_PLAYER_SESSION=1
        AIC_LVGL_BSP_MPP=1 AIC_LVGL_BSP_RTTHREAD=0)
    target_link_libraries(lvgl_aic_player_allocator_contract PRIVATE lvgl)
    if(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
        target_compile_options(lvgl_aic_player_allocator_contract PRIVATE -Wall -Wextra -Werror -UNDEBUG)
    endif()
    add_test(NAME lvgl_aic_player_allocator_contract COMMAND lvgl_aic_player_allocator_contract)
    add_executable(lvgl_aic_player_session_contract player_session_contract.c
        "${AIC_ROOT}/port/lv_aic_player_session.c")
    target_include_directories(lvgl_aic_player_session_contract PRIVATE
        "${AIC_ROOT}/compat" "${CMAKE_CURRENT_LIST_DIR}/mpp_stubs"
        "${AIC_SDK_ROOT}/packages/artinchip/mpp/middle_media/player/include"
        "${AIC_SDK_ROOT}/packages/artinchip/mpp/include"
        "${AIC_SDK_ROOT}/bsp/artinchip/include/uapi")
    target_compile_definitions(lvgl_aic_player_session_contract PRIVATE AIC_LVGL_USE_PLAYER_SESSION=1 AIC_MPP_PLAYER_VIDEO_EXT_RENDER=1)
    if(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
        target_compile_options(lvgl_aic_player_session_contract PRIVATE -Wall -Wextra -Werror -UNDEBUG)
    endif()
    add_test(NAME lvgl_aic_player_session_contract COMMAND lvgl_aic_player_session_contract)
    add_executable(lvgl_aic_vin_session_contract vin_session_contract.c
        "${AIC_ROOT}/common/lv_aic_yuv.c" "${AIC_ROOT}/port/lv_aic_vin_frame.c"
        "${AIC_ROOT}/port/lv_aic_vin_session.c")
    target_include_directories(lvgl_aic_vin_session_contract PRIVATE
        "${AIC_ROOT}/include"
        "${AIC_ROOT}/compat" "${CMAKE_CURRENT_LIST_DIR}/vin_stubs"
        "${AIC_SDK_ROOT}/packages/artinchip/mpp/include"
        "${AIC_SDK_ROOT}/bsp/artinchip/include/uapi"
        "${AIC_SDK_ROOT}/bsp/artinchip/hal/dvp/v1")
    target_compile_definitions(lvgl_aic_vin_session_contract PRIVATE AIC_LVGL_USE_VIN=1
        AIC_IOCTL_HEADER="${AIC_SDK_ROOT}/kernel/rt-thread/components/libc/compilers/common/include/sys/ioctl.h")
    if(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
        target_compile_options(lvgl_aic_vin_session_contract PRIVATE -Wall -Wextra -Werror -UNDEBUG)
    endif()
    add_test(NAME lvgl_aic_vin_session_contract COMMAND lvgl_aic_vin_session_contract)
    target_link_libraries(lvgl_aic_vin_session_contract PRIVATE lvgl)
    find_package(Threads REQUIRED)
    add_executable(lvgl_aic_player_clock_contract player_clock_contract.c
        "${AIC_ROOT}/common/lv_aic_player_clock.c")
    target_include_directories(lvgl_aic_player_clock_contract PRIVATE "${AIC_ROOT}/compat")
    add_executable(lvgl_aic_player_events_contract player_events_contract.c
        "${AIC_ROOT}/port/lv_aic_player_events.c")
    target_include_directories(lvgl_aic_player_events_contract PRIVATE
        "${AIC_ROOT}/compat" "${CMAKE_CURRENT_LIST_DIR}/player_stubs"
        "${CMAKE_CURRENT_LIST_DIR}/mpp_stubs"
        "${AIC_SDK_ROOT}/packages/artinchip/mpp/middle_media/player/include"
        "${AIC_SDK_ROOT}/packages/artinchip/mpp/include"
        "${AIC_SDK_ROOT}/bsp/artinchip/include/uapi")
    target_compile_definitions(lvgl_aic_player_events_contract PRIVATE AIC_LVGL_USE_PLAYER_SESSION=1)
    target_link_libraries(lvgl_aic_player_events_contract PRIVATE Threads::Threads)
    foreach(contract player_clock player_events)
        if(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
            target_compile_options(lvgl_aic_${contract}_contract PRIVATE -Wall -Wextra -Werror -UNDEBUG)
        endif()
        add_test(NAME lvgl_aic_${contract}_contract COMMAND lvgl_aic_${contract}_contract)
        set_tests_properties(lvgl_aic_${contract}_contract PROPERTIES TIMEOUT 15)
    endforeach()
    add_executable(lvgl_aic_rgb_image_contract rgb_image_contract.c
        "${AIC_ROOT}/image/mpp/lv_aic_rgb_image.c" "${AIC_ROOT}/common/lv_aic_rgb_mpp.c"
        "${AIC_ROOT}/common/lv_aic_pixel_format.c")
    target_include_directories(lvgl_aic_rgb_image_contract PRIVATE
        "${AIC_ROOT}/compat" "${AIC_ROOT}/common" "${AIC_ROOT}/include"
        "${CMAKE_CURRENT_LIST_DIR}/mpp_stubs" "${AIC_SDK_ROOT}/bsp/artinchip/include/uapi")
    target_compile_definitions(lvgl_aic_rgb_image_contract PRIVATE AIC_LVGL_BSP_MPP=1 AIC_LVGL_BSP_RTTHREAD=0)
    target_link_libraries(lvgl_aic_rgb_image_contract PRIVATE lvgl)
    if(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
        target_compile_options(lvgl_aic_rgb_image_contract PRIVATE -Wall -Wextra -Werror -UNDEBUG)
    endif()
    add_test(NAME lvgl_aic_rgb_image_contract COMMAND lvgl_aic_rgb_image_contract)
    add_executable(lvgl_aic_player_frames_contract player_frames_contract.c
        "${AIC_ROOT}/image/mpp/lv_aic_rgb_image.c" "${AIC_ROOT}/common/lv_aic_rgb_mpp.c"
        "${AIC_ROOT}/port/lv_aic_player_frames.c" "${AIC_ROOT}/port/lv_aic_player_session.c"
        "${AIC_ROOT}/port/lv_aic_player_allocator.c" "${AIC_ROOT}/common/lv_aic_yuv.c"
        "${AIC_ROOT}/common/lv_aic_yuv_mpp.c" "${AIC_ROOT}/common/lv_aic_pixel_format.c"
        "${AIC_ROOT}/image/mpp/lv_aic_yuv_image.c")
    target_include_directories(lvgl_aic_player_frames_contract PRIVATE
        "${AIC_ROOT}/compat" "${AIC_ROOT}/common" "${AIC_ROOT}/include"
        "${CMAKE_CURRENT_LIST_DIR}/player_stubs" "${CMAKE_CURRENT_LIST_DIR}/mpp_stubs"
        "${AIC_SDK_ROOT}/packages/artinchip/mpp/middle_media/player/include"
        "${AIC_SDK_ROOT}/packages/artinchip/mpp/include"
        "${AIC_SDK_ROOT}/bsp/artinchip/include/uapi")
    target_compile_definitions(lvgl_aic_player_frames_contract PRIVATE AIC_LVGL_USE_PLAYER_SESSION=1
        AIC_MPP_PLAYER_VIDEO_EXT_RENDER=1 AIC_LVGL_BSP_MPP=1 AIC_LVGL_BSP_RTTHREAD=0)
    target_link_libraries(lvgl_aic_player_frames_contract PRIVATE lvgl Threads::Threads)
    if(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
        target_compile_options(lvgl_aic_player_frames_contract PRIVATE -Wall -Wextra -Werror -UNDEBUG)
    endif()
    add_test(NAME lvgl_aic_player_frames_contract COMMAND lvgl_aic_player_frames_contract)
    set_tests_properties(lvgl_aic_player_frames_contract PROPERTIES TIMEOUT 15)
    add_executable(lvgl_aic_player_playback_contract player_playback_contract.c
        "${AIC_ROOT}/image/mpp/lv_aic_rgb_image.c" "${AIC_ROOT}/common/lv_aic_rgb_mpp.c"
        "${AIC_ROOT}/port/lv_aic_player_playback.c" "${AIC_ROOT}/port/lv_aic_player_events.c"
        "${AIC_ROOT}/port/lv_aic_player_frames.c" "${AIC_ROOT}/port/lv_aic_player_session.c"
        "${AIC_ROOT}/port/lv_aic_player_allocator.c" "${AIC_ROOT}/common/lv_aic_yuv.c"
        "${AIC_ROOT}/common/lv_aic_yuv_mpp.c" "${AIC_ROOT}/common/lv_aic_pixel_format.c"
        "${AIC_ROOT}/image/mpp/lv_aic_yuv_image.c")
    target_include_directories(lvgl_aic_player_playback_contract PRIVATE
        "${AIC_ROOT}/compat" "${AIC_ROOT}/common" "${AIC_ROOT}/include"
        "${CMAKE_CURRENT_LIST_DIR}/player_stubs" "${CMAKE_CURRENT_LIST_DIR}/mpp_stubs"
        "${AIC_SDK_ROOT}/packages/artinchip/mpp/middle_media/player/include"
        "${AIC_SDK_ROOT}/packages/artinchip/mpp/include"
        "${AIC_SDK_ROOT}/bsp/artinchip/include/uapi")
    target_compile_definitions(lvgl_aic_player_playback_contract PRIVATE AIC_LVGL_USE_PLAYER_SESSION=1
        AIC_MPP_PLAYER_VIDEO_EXT_RENDER=1 AIC_LVGL_BSP_MPP=1 AIC_LVGL_BSP_RTTHREAD=0)
    target_link_libraries(lvgl_aic_player_playback_contract PRIVATE lvgl Threads::Threads)
    if(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
        target_compile_options(lvgl_aic_player_playback_contract PRIVATE -Wall -Wextra -Werror -UNDEBUG)
    endif()
    add_test(NAME lvgl_aic_player_playback_contract COMMAND lvgl_aic_player_playback_contract)
    set_tests_properties(lvgl_aic_player_playback_contract PROPERTIES TIMEOUT 15)
    add_executable(lvgl_aic_camera_capture_contract camera_capture_contract.c
        "${AIC_ROOT}/widgets/lv_aic_camera.c"
        "${AIC_ROOT}/port/lv_aic_camera_capture.c" "${AIC_ROOT}/port/lv_aic_vin_session.c"
        "${AIC_ROOT}/port/lv_aic_vin_frame.c" "${AIC_ROOT}/common/lv_aic_yuv.c"
        "${AIC_ROOT}/image/mpp/lv_aic_yuv_image.c")
    target_include_directories(lvgl_aic_camera_capture_contract PRIVATE
        "${AIC_SDK_ROOT}/bsp/peripheral/camera"
        "${AIC_ROOT}/include" "${AIC_ROOT}/compat" "${CMAKE_CURRENT_LIST_DIR}/vin_stubs"
        "${AIC_SDK_ROOT}/packages/artinchip/mpp/include" "${AIC_SDK_ROOT}/bsp/artinchip/include/uapi"
        "${AIC_SDK_ROOT}/bsp/artinchip/hal/dvp/v1")
    target_compile_definitions(lvgl_aic_camera_capture_contract PRIVATE AIC_LVGL_USE_VIN=1 AIC_LVGL_USE_CAMERA=1
        AIC_IOCTL_HEADER="${AIC_SDK_ROOT}/kernel/rt-thread/components/libc/compilers/common/include/sys/ioctl.h")
    target_link_libraries(lvgl_aic_camera_capture_contract PRIVATE lvgl Threads::Threads)
    if(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
        target_compile_options(lvgl_aic_camera_capture_contract PRIVATE -Wall -Wextra -Werror -UNDEBUG)
    endif()
    add_test(NAME lvgl_aic_camera_capture_contract COMMAND lvgl_aic_camera_capture_contract)
    set_tests_properties(lvgl_aic_camera_capture_contract PROPERTIES TIMEOUT 15)
    add_executable(lvgl_aic_ge2d_yuv_contract ge2d_yuv_contract.c
        "${AIC_ROOT}/draw/ge2d/lv_draw_aic_ge2d_scale.c"
        "${AIC_ROOT}/common/lv_aic_yuv.c" "${AIC_ROOT}/common/lv_aic_yuv_mpp.c"
        "${AIC_ROOT}/common/lv_aic_pixel_format.c" "${AIC_ROOT}/image/mpp/lv_aic_yuv_image.c"
        "${AIC_ROOT}/draw/ge2d/lv_draw_aic_ge2d_rotate.c")
    target_include_directories(lvgl_aic_ge2d_yuv_contract PRIVATE
        "${AIC_ROOT}/include" "${AIC_ROOT}/compat" "${AIC_ROOT}/common" "${AIC_ROOT}/draw/ge2d"
        "${CMAKE_CURRENT_LIST_DIR}/mpp_stubs" "${AIC_SDK_ROOT}/packages/artinchip/mpp/include"
        "${AIC_SDK_ROOT}/bsp/artinchip/include/drv"
        "${AIC_SDK_ROOT}/bsp/artinchip/include/uapi")
    target_compile_definitions(lvgl_aic_ge2d_yuv_contract PRIVATE
        AIC_LVGL_USE_GE2D=1 AIC_LVGL_BSP_MPP=1 AIC_LVGL_BSP_RTTHREAD=0 AIC_CHIP_D13X=1)
    target_link_libraries(lvgl_aic_ge2d_yuv_contract PRIVATE lvgl)
    if(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
        target_compile_options(lvgl_aic_ge2d_yuv_contract PRIVATE -Wall -Wextra -Werror -UNDEBUG)
    endif()
    add_test(NAME lvgl_aic_ge2d_yuv_contract COMMAND lvgl_aic_ge2d_yuv_contract)
    add_executable(lvgl_aic_yuv_mpp_contract yuv_mpp_contract.c
        "${AIC_ROOT}/common/lv_aic_yuv.c" "${AIC_ROOT}/common/lv_aic_yuv_mpp.c"
        "${AIC_ROOT}/common/lv_aic_pixel_format.c" "${AIC_ROOT}/image/mpp/lv_aic_mpp_format.c")
    target_include_directories(lvgl_aic_yuv_mpp_contract PRIVATE
        "${AIC_ROOT}/include" "${AIC_ROOT}/compat" "${AIC_ROOT}/common"
        "${AIC_ROOT}/image/mpp" "${AIC_SDK_ROOT}/bsp/artinchip/include/uapi")
    target_compile_definitions(lvgl_aic_yuv_mpp_contract PRIVATE
        AIC_LVGL_BSP_MPP=1 AIC_LVGL_USE_MPP_DEC=1 AIC_LVGL_BSP_RTTHREAD=0)
    target_link_libraries(lvgl_aic_yuv_mpp_contract PRIVATE lvgl)
    if(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
        target_compile_options(lvgl_aic_yuv_mpp_contract PRIVATE -Wall -Wextra -Werror -UNDEBUG)
    endif()
    add_test(NAME lvgl_aic_yuv_mpp_contract COMMAND lvgl_aic_yuv_mpp_contract)
    add_executable(lvgl_aic_display_flush_contract display_flush_contract.c)
    target_include_directories(lvgl_aic_display_flush_contract PRIVATE
        "${CMAKE_CURRENT_LIST_DIR}/display_stubs" "${CMAKE_CURRENT_LIST_DIR}/mpp_stubs"
        "${AIC_ROOT}/include" "${AIC_ROOT}/compat" "${AIC_ROOT}/draw/ge2d"
        "${AIC_SDK_ROOT}/packages/artinchip/mpp/include"
        "${AIC_SDK_ROOT}/bsp/artinchip/include/uapi")
    target_compile_definitions(lvgl_aic_display_flush_contract PRIVATE
        AIC_LVGL_USE_DISPLAY=1 AIC_LVGL_USE_GE2D=1 AIC_LVGL_BSP_MPP=1 AIC_LVGL_BSP_RTTHREAD=0)
    target_link_libraries(lvgl_aic_display_flush_contract PRIVATE lvgl)
    if(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
        target_compile_options(lvgl_aic_display_flush_contract PRIVATE -Wall -Wextra -Werror -UNDEBUG)
    endif()
    add_test(NAME lvgl_aic_display_flush_contract COMMAND lvgl_aic_display_flush_contract)
    add_executable(lvgl_aic_ge2d_display_contract ge2d_display_contract.c
        "${AIC_ROOT}/common/lv_aic_pixel_format.c"
        "${AIC_ROOT}/draw/ge2d/lv_draw_aic_ge2d_display.c")
    target_include_directories(lvgl_aic_ge2d_display_contract PRIVATE
        "${AIC_SDK_ROOT}/bsp/artinchip/include/drv"
        "${CMAKE_CURRENT_LIST_DIR}/mpp_stubs" "${AIC_ROOT}/include"
        "${AIC_ROOT}/compat" "${AIC_ROOT}/common" "${AIC_ROOT}/draw/ge2d"
        "${AIC_SDK_ROOT}/packages/artinchip/mpp/include"
        "${AIC_SDK_ROOT}/bsp/artinchip/include/uapi")
    target_compile_definitions(lvgl_aic_ge2d_display_contract PRIVATE
        AIC_LVGL_USE_GE2D=1 AIC_LVGL_BSP_MPP=1 AIC_LVGL_BSP_RTTHREAD=0)
    target_link_libraries(lvgl_aic_ge2d_display_contract PRIVATE lvgl)
    if(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
        target_compile_options(lvgl_aic_ge2d_display_contract PRIVATE -Wall -Wextra -Werror -UNDEBUG)
    endif()
    add_test(NAME lvgl_aic_ge2d_display_contract COMMAND lvgl_aic_ge2d_display_contract)
    add_executable(lvgl_aic_ge2d_fill_contract ge2d_fill_contract.c
        "${AIC_ROOT}/common/lv_aic_fake_image.c"
        "${AIC_ROOT}/common/lv_aic_pixel_format.c")
    target_include_directories(lvgl_aic_ge2d_fill_contract PRIVATE
        "${CMAKE_CURRENT_LIST_DIR}/mpp_stubs" "${AIC_ROOT}/include"
        "${AIC_ROOT}/compat" "${AIC_ROOT}/common" "${AIC_ROOT}/draw/ge2d"
        "${AIC_SDK_ROOT}/packages/artinchip/mpp/include"
        "${AIC_SDK_ROOT}/bsp/artinchip/include/drv"
        "${AIC_SDK_ROOT}/bsp/artinchip/include/uapi")
    target_compile_definitions(lvgl_aic_ge2d_fill_contract PRIVATE
        AIC_LVGL_USE_GE2D=1 AIC_LVGL_BSP_MPP=1 AIC_LVGL_BSP_RTTHREAD=0)
    target_link_libraries(lvgl_aic_ge2d_fill_contract PRIVATE lvgl)
    if(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
        target_compile_options(lvgl_aic_ge2d_fill_contract PRIVATE -Wall -Wextra -Werror -UNDEBUG)
    endif()
    add_test(NAME lvgl_aic_ge2d_fill_contract COMMAND lvgl_aic_ge2d_fill_contract)
    add_executable(lvgl_aic_ge2d_scale_contract ge2d_scale_contract.c
        "${AIC_ROOT}/image/mpp/lv_aic_rgb_image.c"
        "${AIC_ROOT}/common/lv_aic_fake_image.c"
        "${AIC_ROOT}/common/lv_aic_pixel_format.c"
        "${AIC_ROOT}/draw/ge2d/lv_draw_aic_ge2d_scale.c"
        "${AIC_ROOT}/draw/ge2d/lv_draw_aic_ge2d_rotate.c")
    target_include_directories(lvgl_aic_ge2d_scale_contract PRIVATE
        "${CMAKE_CURRENT_LIST_DIR}/mpp_stubs" "${AIC_ROOT}/include"
        "${AIC_ROOT}/compat" "${AIC_ROOT}/common" "${AIC_ROOT}/draw/ge2d"
        "${AIC_SDK_ROOT}/packages/artinchip/mpp/include"
        "${AIC_SDK_ROOT}/bsp/artinchip/include/drv"
        "${AIC_SDK_ROOT}/bsp/artinchip/include/uapi")
    target_compile_definitions(lvgl_aic_ge2d_scale_contract PRIVATE
        AIC_LVGL_USE_GE2D=1 AIC_LVGL_BSP_MPP=1 AIC_LVGL_BSP_RTTHREAD=0)
    target_link_libraries(lvgl_aic_ge2d_scale_contract PRIVATE lvgl)
    if(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
        target_compile_options(lvgl_aic_ge2d_scale_contract PRIVATE -Wall -Wextra -Werror -UNDEBUG)
    endif()
    add_test(NAME lvgl_aic_ge2d_scale_contract COMMAND lvgl_aic_ge2d_scale_contract)
    add_executable(lvgl_aic_mpp_contract mpp_contract.c
        "${AIC_ROOT}/widgets/lv_aic_video_window.c"
        "${AIC_ROOT}/image/mpp/lv_aic_fake_fs.c"
        "${AIC_ROOT}/common/lv_aic_fake_image.c"
        "${AIC_ROOT}/image/mpp/lv_aic_bmp_header.c"
        "${AIC_ROOT}/common/lv_aic_pixel_format.c"
        "${AIC_ROOT}/image/mpp/lv_aic_mpp_format.c"
        "${AIC_ROOT}/image/mpp/lv_aic_mpp_stream.c")
    target_include_directories(lvgl_aic_mpp_contract PRIVATE
        "${CMAKE_CURRENT_LIST_DIR}/mpp_stubs" "${AIC_ROOT}/include"
        "${AIC_ROOT}/compat" "${AIC_ROOT}/common" "${AIC_ROOT}/image/mpp"
        "${AIC_SDK_ROOT}/packages/artinchip/mpp/include"
        "${AIC_SDK_ROOT}/bsp/artinchip/include/uapi")
    target_compile_definitions(lvgl_aic_mpp_contract PRIVATE
        AIC_LVGL_USE_VIDEO_WINDOW=1
        AIC_LVGL_USE_GE2D=1 AIC_LVGL_USE_MPP_DEC=1 AIC_LVGL_BSP_MPP=1 AIC_LVGL_BSP_RTTHREAD=0)
    target_link_libraries(lvgl_aic_mpp_contract PRIVATE lvgl)
    if(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
        target_compile_options(lvgl_aic_mpp_contract PRIVATE -Wall -Wextra -Werror -UNDEBUG)
    endif()
    add_test(NAME lvgl_aic_mpp_contract COMMAND lvgl_aic_mpp_contract "${AIC_ROOT}/tests/data/mpp")
    add_test(NAME lvgl_aic_bmp_pixel_contract COMMAND lvgl_aic_mpp_contract
        "${AIC_ROOT}/tests/data/mpp" --bmp)
    if(AIC_BUILD_AICP_TESTS)
        target_compile_definitions(lvgl_aic_mpp_contract PRIVATE AIC_MPP_AICP_DEC_ENABLE=1)
        if(AIC_BUILD_AICP_V31_TESTS)
            target_compile_definitions(lvgl_aic_mpp_contract PRIVATE AIC_VE_DRV_V31=1)
        endif()
        add_test(NAME lvgl_aic_aicp_fixture_contract COMMAND lvgl_aic_mpp_contract
            "${AIC_SDK_ROOT}/packages/artinchip/lvgl-ui/aic_demo/aic_widget_demo/img_usage_demo/assets/image"
            --aicp)
    endif()
    add_test(NAME lvgl_aic_mpp_memory_cache_contract COMMAND lvgl_aic_mpp_contract "${AIC_ROOT}/tests/data/mpp" --memory-cache)
endif()

add_executable(lvgl_aic_disabled_features lifecycle_disabled.c "${AIC_ROOT}/port/lv_aic.c")
target_include_directories(lvgl_aic_disabled_features PRIVATE
    "${AIC_ROOT}/include" "${AIC_ROOT}/compat" "${AIC_ROOT}/port")
target_compile_definitions(lvgl_aic_disabled_features PRIVATE
    AIC_LVGL_USE_MPP_DEC=0 AIC_LVGL_USE_TOUCH=0 AIC_LVGL_BSP_RTTHREAD=0)
target_link_libraries(lvgl_aic_disabled_features PRIVATE lvgl)
if(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(lvgl_aic_disabled_features PRIVATE -Wall -Wextra -Werror -UNDEBUG)
endif()
add_test(NAME lvgl_aic_disabled_features COMMAND lvgl_aic_disabled_features)

if(AIC_BUILD_FREETYPE_TESTS)
    add_executable(lvgl_aic_font_contract font_contract.c
        "${AIC_ROOT}/tests/manual/lv_aic_font_test.c")
    target_include_directories(lvgl_aic_font_contract PRIVATE
        "${AIC_ROOT}/include" "${AIC_ROOT}/tests/manual")
    target_compile_definitions(lvgl_aic_font_contract PRIVATE AIC_LVGL_BSP_RTTHREAD=0)
    target_link_libraries(lvgl_aic_font_contract PRIVATE lvgl)
    if(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
        target_compile_options(lvgl_aic_font_contract PRIVATE -Wall -Wextra -Werror -UNDEBUG)
    endif()
    add_test(NAME lvgl_aic_font_contract COMMAND lvgl_aic_font_contract
        "${LVGL_ROOT}/examples/libs/freetype/Lato-Regular.ttf"
        "${LVGL_ROOT}/tests/src/test_files/fonts/noto/NotoSansSC-Regular.ttf"
        "${AIC_ROOT}/tests/data/mpp/basn2c08.png")
endif()

if(AIC_BUILD_GIF_TESTS)
    add_executable(lvgl_aic_gif_contract gif_contract.c
        "${AIC_ROOT}/tests/manual/lv_aic_gif_test.c")
    target_include_directories(lvgl_aic_gif_contract PRIVATE
        "${AIC_ROOT}/include" "${AIC_ROOT}/tests/manual")
    target_compile_definitions(lvgl_aic_gif_contract PRIVATE AIC_LVGL_BSP_RTTHREAD=0)
    target_link_libraries(lvgl_aic_gif_contract PRIVATE lvgl)
    if(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
        target_compile_options(lvgl_aic_gif_contract PRIVATE -Wall -Wextra -Werror -UNDEBUG)
    endif()
    configure_file("${LVGL_ROOT}/examples/libs/gif/bulb.gif"
                   "${CMAKE_CURRENT_BINARY_DIR}/bulb.gif" COPYONLY)
    add_test(NAME lvgl_aic_gif_contract COMMAND lvgl_aic_gif_contract
        "${CMAKE_CURRENT_BINARY_DIR}/bulb.gif")
    set_tests_properties(lvgl_aic_gif_contract PROPERTIES
        WORKING_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}")
endif()

if(AIC_BUILD_INPUT_TESTS)
    add_executable(lvgl_aic_input_contract input_contract.c
        "${AIC_ROOT}/port/lv_aic.c"
        "${AIC_ROOT}/port/lv_aic_indev.c"
        "${AIC_ROOT}/port/lv_aic_input.c")
    target_include_directories(lvgl_aic_input_contract PRIVATE
        "${AIC_ROOT}/include" "${AIC_ROOT}/compat" "${AIC_ROOT}/port")
    target_compile_definitions(lvgl_aic_input_contract PRIVATE
        AIC_LVGL_BSP_MPP=0 AIC_LVGL_BSP_RTTHREAD=0
        AIC_LVGL_USE_DISPLAY=1 AIC_LVGL_USE_TOUCH=0
        AIC_LVGL_USE_ENCODER=1 AIC_LVGL_USE_MOUSE=1
        AIC_LVGL_USE_MPP_DEC=0 AIC_LVGL_USE_GE2D=0)
    target_link_libraries(lvgl_aic_input_contract PRIVATE lvgl)
    if(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
        target_compile_options(lvgl_aic_input_contract PRIVATE -Wall -Wextra -Werror -UNDEBUG)
    endif()
    add_test(NAME lvgl_aic_input_contract COMMAND lvgl_aic_input_contract)
endif()

if(AIC_BUILD_IMG_ROLLER_TESTS)
    add_executable(lvgl_aic_img_roller_contract img_roller_contract.c
        "${AIC_ROOT}/widgets/lv_img_roller.c")
    target_include_directories(lvgl_aic_img_roller_contract PRIVATE
        "${AIC_ROOT}/include" "${AIC_ROOT}/compat")
    target_compile_definitions(lvgl_aic_img_roller_contract PRIVATE AIC_LVGL_USE_IMG_ROLLER=1)
    target_link_libraries(lvgl_aic_img_roller_contract PRIVATE lvgl)
    if(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
        target_compile_options(lvgl_aic_img_roller_contract PRIVATE -Wall -Wextra -Werror -UNDEBUG)
    endif()
    add_test(NAME lvgl_aic_img_roller_contract COMMAND lvgl_aic_img_roller_contract)
endif()

if(AIC_BUILD_SWIPE_TESTS)
    if(AIC_BUILD_IMG_ROLLER_TESTS)
        target_sources(lvgl_aic_manual_pages PRIVATE
            "${AIC_ROOT}/widgets/lv_img_roller.c" "${AIC_ROOT}/widgets/lv_swipe_v1.c")
        target_include_directories(lvgl_aic_manual_pages PRIVATE "${AIC_ROOT}/compat")
        target_compile_definitions(lvgl_aic_manual_pages PRIVATE
            AIC_LVGL_USE_IMG_ROLLER=1 AIC_LVGL_USE_SWIPE_V1=1)
    endif()
    add_executable(lvgl_aic_swipe_contract swipe_contract.c
        "${AIC_ROOT}/widgets/lv_swipe_v1.c")
    target_include_directories(lvgl_aic_swipe_contract PRIVATE
        "${AIC_ROOT}/include" "${AIC_ROOT}/compat")
    target_compile_definitions(lvgl_aic_swipe_contract PRIVATE AIC_LVGL_USE_SWIPE_V1=1)
    target_link_libraries(lvgl_aic_swipe_contract PRIVATE lvgl)
    if(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
        target_compile_options(lvgl_aic_swipe_contract PRIVATE -Wall -Wextra -Werror -UNDEBUG)
    endif()
    add_test(NAME lvgl_aic_swipe_contract COMMAND lvgl_aic_swipe_contract)
endif()

if(AIC_BUILD_WIDGET_TESTS)
    target_compile_definitions(lvgl PUBLIC
        LV_USE_CANVAS=1
        LV_USE_CHART=1
        LV_USE_DROPDOWN=1
        LV_USE_ROLLER=1
        LV_USE_SLIDER=1
        LV_USE_TABLE=1
        LV_USE_TABVIEW=1
        LV_USE_TEXTAREA=1
        LV_USE_TILEVIEW=1)
    add_executable(lvgl_aic_widget_contract widget_contract.c)
    target_compile_definitions(lvgl_aic_widget_contract PRIVATE
        LV_USE_CANVAS=1 LV_USE_CHART=1 LV_USE_DROPDOWN=1 LV_USE_ROLLER=1
        LV_USE_SLIDER=1 LV_USE_TABLE=1 LV_USE_TABVIEW=1 LV_USE_TEXTAREA=1
        LV_USE_TILEVIEW=1)
    target_link_libraries(lvgl_aic_widget_contract PRIVATE lvgl)
    if(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
        target_compile_options(lvgl_aic_widget_contract PRIVATE -Wall -Wextra -Werror -UNDEBUG)
    endif()
    add_test(NAME lvgl_aic_widget_contract COMMAND lvgl_aic_widget_contract)
endif()

if(AIC_BUILD_CONTROL_WIDGET_TESTS)
    target_compile_definitions(lvgl PUBLIC
        LV_USE_ARC=1
        LV_USE_BUTTON=1
        LV_USE_BUTTONMATRIX=1
        LV_USE_CALENDAR=1
        LV_USE_CHECKBOX=1
        LV_USE_KEYBOARD=1
        LV_USE_LED=1
        LV_USE_LINE=1
        LV_USE_MSGBOX=1
        LV_USE_SPINBOX=1
        LV_USE_SWITCH=1)
    add_executable(lvgl_aic_control_widget_contract control_widget_contract.c)
    target_compile_definitions(lvgl_aic_control_widget_contract PRIVATE
        LV_USE_ARC=1 LV_USE_BUTTON=1 LV_USE_BUTTONMATRIX=1 LV_USE_CALENDAR=1
        LV_USE_CHECKBOX=1 LV_USE_KEYBOARD=1 LV_USE_LED=1 LV_USE_LINE=1
        LV_USE_MSGBOX=1 LV_USE_SPINBOX=1 LV_USE_SWITCH=1)
    target_link_libraries(lvgl_aic_control_widget_contract PRIVATE lvgl)
    if(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
        target_compile_options(lvgl_aic_control_widget_contract PRIVATE -Wall -Wextra -Werror -UNDEBUG)
    endif()
    add_test(NAME lvgl_aic_control_widget_contract COMMAND lvgl_aic_control_widget_contract)
endif()

add_executable(lvgl_aic_camera_widget_contract camera_widget_contract.c
    "${AIC_ROOT}/widgets/lv_aic_camera.c"
    "${AIC_ROOT}/common/lv_aic_yuv.c" "${AIC_ROOT}/image/mpp/lv_aic_yuv_image.c")
target_include_directories(lvgl_aic_camera_widget_contract PRIVATE
    "${AIC_ROOT}/include" "${AIC_ROOT}/compat")
target_compile_definitions(lvgl_aic_camera_widget_contract PRIVATE AIC_LVGL_USE_CAMERA=1)
target_link_libraries(lvgl_aic_camera_widget_contract PRIVATE lvgl)
if(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(lvgl_aic_camera_widget_contract PRIVATE -Wall -Wextra -Werror -UNDEBUG)
endif()
add_test(NAME lvgl_aic_camera_widget_contract COMMAND lvgl_aic_camera_widget_contract)

set_tests_properties(lvgl_aic_camera_widget_contract PROPERTIES TIMEOUT 15)

add_executable(lvgl_aic_player_widget_contract player_widget_contract.c
    "${AIC_ROOT}/widgets/lv_aic_player.c"
    "${AIC_ROOT}/common/lv_aic_yuv.c" "${AIC_ROOT}/image/mpp/lv_aic_yuv_image.c"
    "${AIC_ROOT}/image/mpp/lv_aic_rgb_image.c")
target_include_directories(lvgl_aic_player_widget_contract PRIVATE
    "${AIC_ROOT}/include" "${AIC_ROOT}/compat")
target_compile_definitions(lvgl_aic_player_widget_contract PRIVATE AIC_LVGL_USE_PLAYER=1)
target_link_libraries(lvgl_aic_player_widget_contract PRIVATE lvgl)
if(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(lvgl_aic_player_widget_contract PRIVATE -Wall -Wextra -Werror -UNDEBUG)
endif()
add_test(NAME lvgl_aic_player_widget_contract COMMAND lvgl_aic_player_widget_contract)
set_tests_properties(lvgl_aic_player_widget_contract PROPERTIES TIMEOUT 15)

add_executable(lvgl_aic_apng_contract apng_contract.c "${AIC_ROOT}/common/lv_aic_apng.c" "${AIC_ROOT}/common/lv_aic_apng_compose.c")
target_include_directories(lvgl_aic_apng_contract PRIVATE "${AIC_ROOT}/common")
if(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(lvgl_aic_apng_contract PRIVATE -Wall -Wextra -Werror -UNDEBUG)
endif()
add_test(NAME lvgl_aic_apng_contract COMMAND lvgl_aic_apng_contract)
set_tests_properties(lvgl_aic_apng_contract PROPERTIES TIMEOUT 15)

add_executable(lvgl_aic_apng_compose_contract apng_compose_contract.c "${AIC_ROOT}/common/lv_aic_apng_compose.c")
target_include_directories(lvgl_aic_apng_compose_contract PRIVATE "${AIC_ROOT}/common")
if(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(lvgl_aic_apng_compose_contract PRIVATE -Wall -Wextra -Werror -UNDEBUG)
endif()
add_test(NAME lvgl_aic_apng_compose_contract COMMAND lvgl_aic_apng_compose_contract)
set_tests_properties(lvgl_aic_apng_compose_contract PROPERTIES TIMEOUT 15)

add_executable(lvgl_aic_apng_timeline_contract apng_timeline_contract.c "${AIC_ROOT}/common/lv_aic_apng_timeline.c")
target_include_directories(lvgl_aic_apng_timeline_contract PRIVATE "${AIC_ROOT}/common")
if(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(lvgl_aic_apng_timeline_contract PRIVATE -Wall -Wextra -Werror -UNDEBUG)
endif()
add_test(NAME lvgl_aic_apng_timeline_contract COMMAND lvgl_aic_apng_timeline_contract)
set_tests_properties(lvgl_aic_apng_timeline_contract PROPERTIES TIMEOUT 15)
