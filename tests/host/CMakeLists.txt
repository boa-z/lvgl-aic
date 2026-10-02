cmake_minimum_required(VERSION 3.20)
project(lvgl_aic_host_smoke LANGUAGES C)

set(AIC_ROOT "${CMAKE_CURRENT_LIST_DIR}/../.." CACHE PATH "lvgl-aic repository root")
set(LVGL_ROOT "" CACHE PATH "Separate LVGL v9.6.0 checkout")

if (LVGL_ROOT STREQUAL "")
    message(FATAL_ERROR "Set -DLVGL_ROOT to an external LVGL v9.6.0 checkout")
endif()
option(AIC_BUILD_GIF_TESTS "Test native LVGL GIF widget" OFF)
option(AIC_BUILD_INPUT_TESTS "Test application-owned encoder and mouse providers" OFF)
option(AIC_BUILD_WIDGET_TESTS "Test native LVGL widget surface" OFF)
option(AIC_BUILD_IMG_ROLLER_TESTS "Test the adapted SDK image roller" OFF)
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
    "${AIC_ROOT}/port/lv_aic.c"
    "${AIC_ROOT}/port/lv_aic_display.c"
    "${AIC_ROOT}/port/lv_aic_indev.c"
    "${AIC_ROOT}/common/lv_aic_pixel_format.c"
    "${AIC_ROOT}/image/mpp/lv_aic_mpp_format.c"
    "${AIC_ROOT}/image/mpp/lv_aic_mpp_stream.c"
    "${AIC_ROOT}/image/mpp/lv_aic_mpp_decoder.c")
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
    add_executable(lvgl_aic_ge2d_fill_contract ge2d_fill_contract.c
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
        "${AIC_ROOT}/common/lv_aic_pixel_format.c"
        "${AIC_ROOT}/image/mpp/lv_aic_mpp_format.c"
        "${AIC_ROOT}/image/mpp/lv_aic_mpp_stream.c")
    target_include_directories(lvgl_aic_mpp_contract PRIVATE
        "${CMAKE_CURRENT_LIST_DIR}/mpp_stubs" "${AIC_ROOT}/include"
        "${AIC_ROOT}/compat" "${AIC_ROOT}/common" "${AIC_ROOT}/image/mpp"
        "${AIC_SDK_ROOT}/packages/artinchip/mpp/include"
        "${AIC_SDK_ROOT}/bsp/artinchip/include/uapi")
    target_compile_definitions(lvgl_aic_mpp_contract PRIVATE
        AIC_LVGL_USE_MPP_DEC=1 AIC_LVGL_BSP_MPP=1 AIC_LVGL_BSP_RTTHREAD=0)
    target_link_libraries(lvgl_aic_mpp_contract PRIVATE lvgl)
    if(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
        target_compile_options(lvgl_aic_mpp_contract PRIVATE -Wall -Wextra -Werror -UNDEBUG)
    endif()
    add_test(NAME lvgl_aic_mpp_contract COMMAND lvgl_aic_mpp_contract "${AIC_ROOT}/tests/data/mpp")
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
