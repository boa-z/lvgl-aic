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
/* Component-generated bridges to the pinned native software pixel kernels. */
bool lv_aic_sw_recolor_copy(const lv_draw_buf_t *src, lv_draw_buf_t *dst,
                            lv_color_t color, lv_opa_t opacity);
bool lv_aic_sw_colorkey_copy(const lv_draw_buf_t *src, lv_draw_buf_t *dst,
                             const lv_image_colorkey_t *key);
int lv_aic_sw_layer_mask_copy(const lv_draw_image_dsc_t *dsc, const lv_area_t *area,
                              const lv_draw_buf_t *src, lv_draw_buf_t *dst);
int lv_aic_sw_image_mask_copy(const lv_draw_image_dsc_t *dsc, const lv_area_t *area,
                              const lv_draw_buf_t *src, lv_draw_buf_t *dst);
#endif

#endif /* LVGL_AIC_PRIVATE_H */
