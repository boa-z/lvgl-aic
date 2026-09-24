cmake_minimum_required(VERSION 3.20)
project(lvgl_aic_host_smoke LANGUAGES C)

set(AIC_ROOT "${CMAKE_CURRENT_LIST_DIR}/../.." CACHE PATH "lvgl-aic repository root")
set(LVGL_ROOT "" CACHE PATH "Separate LVGL v9.6.0 checkout")

if (LVGL_ROOT STREQUAL "")
    message(FATAL_ERROR "Set -DLVGL_ROOT to an external LVGL v9.6.0 checkout")
endif()

set(LV_BUILD_CONF_PATH "${AIC_ROOT}/lv_conf.h" CACHE FILEPATH "" FORCE)
set(CONFIG_LV_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(CONFIG_LV_BUILD_DEMOS OFF CACHE BOOL "" FORCE)
set(CONFIG_LV_USE_THORVG OFF CACHE BOOL "" FORCE)
set(CONFIG_LV_USE_THORVG_INTERNAL OFF CACHE BOOL "" FORCE)
set(LV_BUILD_SET_CONFIG_OPTS OFF CACHE BOOL "" FORCE)
set(LV_FETCH_DEPENDENCIES OFF CACHE BOOL "" FORCE)
set(LV_USE_FIND_PACKAGE OFF CACHE BOOL "" FORCE)
set(LV_USE_PKG_CONFIG OFF CACHE BOOL "" FORCE)

add_subdirectory("${LVGL_ROOT}" lvgl EXCLUDE_FROM_ALL)

set(AIC_SOURCES
    "${AIC_ROOT}/port/lv_aic.c"
    "${AIC_ROOT}/port/lv_aic_display.c"
    "${AIC_ROOT}/port/lv_aic_indev.c")
add_library(lvgl_aic STATIC ${AIC_SOURCES})
target_include_directories(lvgl_aic
    PUBLIC "${AIC_ROOT}/include"
    PRIVATE "${AIC_ROOT}/compat")
target_compile_definitions(lvgl_aic PRIVATE
    AIC_LVGL_BSP_MPP=0
    AIC_LVGL_BSP_RTTHREAD=0
    AIC_LVGL_USE_DISPLAY=1
    AIC_LVGL_USE_TOUCH=0)
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
add_test(NAME lvgl_aic_platform_smoke COMMAND lvgl_aic_smoke)

option(AIC_BUILD_SDL_SMOKE "Build the interactive SDL2 smoke window" OFF)
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
    target_link_libraries(lvgl-aic-sdl-smoke PRIVATE lvgl PkgConfig::SDL2)
    if(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
        target_compile_options(lvgl-aic-sdl-smoke PRIVATE -Wall -Wextra -Werror)
    endif()
    add_test(NAME lvgl_aic_sdl_smoke_self
             COMMAND lvgl-aic-sdl-smoke --self-test)
    set_tests_properties(lvgl_aic_sdl_smoke_self PROPERTIES
        ENVIRONMENT "SDL_VIDEODRIVER=dummy;SDL_RENDER_DRIVER=software"
        TIMEOUT 15)
endif()
