/**
 * @file lvgl_aic_private.h
 * @brief集中管理 LVGL private API 依赖。
 *
 * 目前 Phase 0/1 的 display/input 只使用 public API。GE2D 阶段如确实需要
 * LVGL private API，必须只从此处引入，并在 compatibility 文档中记录。
 */

#ifndef LVGL_AIC_PRIVATE_H
#define LVGL_AIC_PRIVATE_H

#include "lvgl_aic_compat.h"

#if defined(AIC_LVGL_USE_PRIVATE_API)
#include <lvgl_private.h>
#endif

#endif /* LVGL_AIC_PRIVATE_H */
