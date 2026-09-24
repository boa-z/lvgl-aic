/**
 * @file lvgl_aic_compat.h
 * @brief LVGL 9.6 compatibility boundary for the public port API.
 */

#ifndef LVGL_AIC_COMPAT_H
#define LVGL_AIC_COMPAT_H

#if defined(LV_LVGL_H_INCLUDE_SIMPLE)
#include "lvgl.h"
#else
#include <lvgl/lvgl.h>
#endif

#if (LVGL_VERSION_MAJOR != 9) || (LVGL_VERSION_MINOR != 6)
#error "lvgl-aic currently supports LVGL 9.6.x only"
#endif

#ifndef AIC_LVGL_BSP_MPP
#define AIC_LVGL_BSP_MPP 0
#endif

#ifndef AIC_LVGL_BSP_RTTHREAD
#define AIC_LVGL_BSP_RTTHREAD 0
#endif

#ifndef AIC_LVGL_DEBUG_STATS
#define AIC_LVGL_DEBUG_STATS 0
#endif

#ifndef AIC_LVGL_USE_PRIVATE_API
#define AIC_LVGL_USE_PRIVATE_API 0
#endif
#ifndef AIC_LVGL_USE_DISPLAY
#define AIC_LVGL_USE_DISPLAY 0
#endif
#ifndef AIC_LVGL_USE_TOUCH
#define AIC_LVGL_USE_TOUCH 0
#endif
#ifndef AIC_LVGL_USE_ENCODER
#define AIC_LVGL_USE_ENCODER 0
#endif
#ifndef AIC_LVGL_USE_MOUSE
#define AIC_LVGL_USE_MOUSE 0
#endif
#ifndef AIC_LVGL_USE_GE2D
#define AIC_LVGL_USE_GE2D 0
#endif
#ifndef AIC_LVGL_USE_MPP_DEC
#define AIC_LVGL_USE_MPP_DEC 0
#endif
#ifndef AIC_LVGL_USE_FT_CACHE
#define AIC_LVGL_USE_FT_CACHE 0
#endif

#endif /* LVGL_AIC_COMPAT_H */
