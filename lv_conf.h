/**
 * @file lv_conf.h
 * @brief Minimal LVGL 9.6 configuration baseline for lvgl-aic.
 *
 * This file is intentionally not a copy of the LVGL 9.1 ArtInChip
 * configuration. Platform/target configuration may override these defaults
 * through the normal Luban-Lite include path or a generated lv_conf.h.
 */

#ifndef LV_CONF_H
#define LV_CONF_H

/* Build glue checks this marker before starting a target compilation. */
#define LV_AIC_LV_CONF_MARKER 0x4C56414Du

/* Luban-Lite targets provide rtconfig.h. Host-side LVGL builds do not. */
#if defined(LPKG_USING_LVGL) || defined(KERNEL_RTTHREAD) || defined(__RTTHREAD__)
#include <rtconfig.h>
#endif

/* v9.6 configuration names. Keep the OS setting aligned with the RT-Thread
 * OSAL; do not maintain a second LVGL mutex/thread abstraction here. */
#if defined(KERNEL_RTTHREAD) || defined(__RTTHREAD__)
#define LV_USE_OS LV_OS_RTTHREAD
#else
#define LV_USE_OS LV_OS_NONE
#endif

#if defined(KERNEL_RTTHREAD) || defined(__RTTHREAD__)
#ifndef LV_USE_STDLIB_MALLOC
#define LV_USE_STDLIB_MALLOC LV_STDLIB_RTTHREAD
#endif
#ifndef LV_USE_STDLIB_STRING
#define LV_USE_STDLIB_STRING LV_STDLIB_RTTHREAD
#endif
#ifndef LV_USE_STDLIB_SPRINTF
#define LV_USE_STDLIB_SPRINTF LV_STDLIB_RTTHREAD
#endif
#else
#ifndef LV_USE_STDLIB_MALLOC
#define LV_USE_STDLIB_MALLOC LV_STDLIB_CLIB
#endif
#ifndef LV_USE_STDLIB_STRING
#define LV_USE_STDLIB_STRING LV_STDLIB_CLIB
#endif
#ifndef LV_USE_STDLIB_SPRINTF
#define LV_USE_STDLIB_SPRINTF LV_STDLIB_CLIB
#endif
#endif

/* The D13x baseline uses an RGB565 LVGL surface. The display port maps the
 * physical framebuffer format independently. */
#ifndef LV_COLOR_FORMAT_DEFAULT
#define LV_COLOR_FORMAT_DEFAULT LV_COLOR_FORMAT_RGB565
#endif

#ifndef LV_DEF_REFR_PERIOD
#define LV_DEF_REFR_PERIOD 10
#endif
#ifndef LV_DPI_DEF
#define LV_DPI_DEF 130
#endif
#ifndef LV_DRAW_BUF_STRIDE_ALIGN
#define LV_DRAW_BUF_STRIDE_ALIGN 8
#endif
#ifndef LV_DRAW_BUF_ALIGN
#if defined(CACHE_LINE_SIZE)
#define LV_DRAW_BUF_ALIGN CACHE_LINE_SIZE
#else
#define LV_DRAW_BUF_ALIGN 8
#endif
#endif
#ifndef LV_DRAW_THREAD_PRIO
#define LV_DRAW_THREAD_PRIO 20
#endif
#ifndef LV_DRAW_THREAD_STACK_SIZE
#define LV_DRAW_THREAD_STACK_SIZE 4096
#endif

/* Phase 1 deliberately enables only the software renderer. */
#ifndef LV_USE_DRAW_SW
#define LV_USE_DRAW_SW 1
#endif
#ifndef LV_DRAW_SW_SUPPORT_RGB565
#define LV_DRAW_SW_SUPPORT_RGB565 1
#endif
#ifndef LV_DRAW_SW_DRAW_UNIT_CNT
#define LV_DRAW_SW_DRAW_UNIT_CNT 1
#endif
#ifndef LV_USE_DRAW_SW_ASM
#define LV_USE_DRAW_SW_ASM LV_DRAW_SW_ASM_NONE
#endif
#ifndef LV_USE_VECTOR_GRAPHIC
#define LV_USE_VECTOR_GRAPHIC 0
#endif
#ifndef LV_USE_THORVG
#define LV_USE_THORVG 0
#endif
#ifndef LV_USE_THORVG_INTERNAL
#define LV_USE_THORVG_INTERNAL 0
#endif

/* Keep diagnostics available without enabling high-frequency logging. */
#ifndef LV_USE_LOG
#define LV_USE_LOG 1
#endif
#ifndef LV_LOG_LEVEL
#define LV_LOG_LEVEL LV_LOG_LEVEL_WARN
#endif
#ifndef LV_LOG_PRINTF
#define LV_LOG_PRINTF 0
#endif
#ifndef LV_USE_SYSMON
#define LV_USE_SYSMON 0
#endif
#ifndef LV_USE_MEM_MONITOR
#define LV_USE_MEM_MONITOR 0
#endif
#ifndef LV_USE_PERF_MONITOR
#define LV_USE_PERF_MONITOR 0
#endif
#ifndef LV_USE_PROFILER
#define LV_USE_PROFILER 0
#endif
#ifndef LV_USE_ASSERT
#define LV_USE_ASSERT 1
#endif

/* Do not enable demos or optional media/font components implicitly. */
#ifndef LV_USE_DEMO_WIDGETS
#define LV_USE_DEMO_WIDGETS 0
#endif
#ifndef LV_USE_DEMO_BENCHMARK
#define LV_USE_DEMO_BENCHMARK 0
#endif
#ifndef LV_USE_DEMO_MUSIC
#define LV_USE_DEMO_MUSIC 0
#endif
#ifndef LV_USE_FREETYPE
#define LV_USE_FREETYPE 0
#endif
#ifndef LV_USE_GIF
#define LV_USE_GIF 0
#endif

#endif /* LV_CONF_H */
