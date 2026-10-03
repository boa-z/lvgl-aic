/*
 * Copyright (C) 2025, ArtInChip Technology Co., Ltd
 * SPDX-License-Identifier: Apache-2.0
 * Original author: ZeQuan Liang <zequan.liang@artinchip.com>
 */
#ifndef LV_SWIPE_V1_H
#define LV_SWIPE_V1_H
#include "lvgl.h"
#ifndef AIC_LVGL_USE_SWIPE_V1
#define AIC_LVGL_USE_SWIPE_V1 0
#endif
#ifdef __cplusplus
extern "C" {
#endif
#if AIC_LVGL_USE_SWIPE_V1
#if !LV_USE_IMAGE
#error "AIC swipe requires LV_USE_IMAGE"
#endif
extern const lv_obj_class_t lv_swipe_v1_class;
lv_obj_t *lv_swipe_v1_create(lv_obj_t *parent);
/**
 * Append a borrowed active/deactive source pair. Up to four children with
 * stable IDs 0..32767; each child supports 32768 pairs. Invalid requests and
 * allocation failure leave existing pairs intact. Initial geometry is owned
 * by the application. Children must remain parented to this widget.
 */
void lv_swipe_v1_child_add_state_src(lv_obj_t *obj, uint16_t child_idx,
                                   void *active_src, void *deactive_src);
void lv_swipe_v1_set_next(lv_obj_t *obj, lv_anim_enable_t enable);
void lv_swipe_v1_set_prev(lv_obj_t *obj, lv_anim_enable_t enable);
void lv_swipe_v1_set_anim_params(lv_obj_t *obj, uint16_t time, lv_anim_path_cb_t path);
void lv_swipe_v1_set_child_active_src(lv_obj_t *obj, uint16_t child_idx, uint16_t src_idx);
/* Compatibility with the SDK implementation's spelling. */
void lv_swipe_v1_set_child_activate(lv_obj_t *obj, uint16_t child_idx, uint16_t src_idx);
void lv_swipe_v1_set_child_deactive_src(lv_obj_t *obj, uint16_t child_idx, uint16_t src_idx);
void lv_swipe_v1_toggle_child_active(lv_obj_t *obj);
int16_t lv_swipe_v1_get_active_child(lv_obj_t *obj);
uint16_t lv_swipe_v1_get_child_count(lv_obj_t *obj);
int16_t lv_swipe_v1_get_child_active_src(lv_obj_t *obj, uint16_t child_idx);
lv_obj_t *lv_swipe_v1_get_child(lv_obj_t *obj, uint16_t child_idx);
#endif
#ifdef __cplusplus
}
#endif
#endif
