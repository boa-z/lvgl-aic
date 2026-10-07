/*
 * Copyright (c) 2023-2025, ArtInChip Technology Co., Ltd
 * SPDX-License-Identifier: Apache-2.0
 * Original author: Zequan Liang <zequan.liang@artinchip.com>
 * LVGL 9.6 adaptation of the SDK alpha-zero video window.
 */
#include "lv_aic_video_window.h"
#define AIC_LVGL_USE_PRIVATE_API 1
#include "lvgl_aic_private.h"
#if AIC_LVGL_USE_VIDEO_WINDOW
#include <stdio.h>
#include <string.h>
typedef struct {
    lv_image_t image;
    uint32_t width, height;
    lv_color_t color;
} video_window_t;

const lv_obj_class_t lv_aic_video_window_class={
    .width_def=LV_SIZE_CONTENT, .height_def=LV_SIZE_CONTENT,
    .base_class=&lv_image_class, .instance_size=sizeof(video_window_t),
    .name="aic_video_window",
};

static void update(lv_obj_t *obj, uint32_t width, uint32_t height, lv_color_t color)
{
    video_window_t *window=(video_window_t *)obj;
    char source[64];
    if (!width || !height) { window->color=color; return; }
    uint32_t rgb=((uint32_t)color.red<<16)|((uint32_t)color.green<<8)|color.blue;
    snprintf(source,sizeof(source),"L:/%ux%u_0_%08x.fake",
             (unsigned)width,(unsigned)height,(unsigned)rgb);
    const char *current=lv_image_get_src(obj);
    if (!current || lv_image_src_get_type(current)!=LV_IMAGE_SRC_FILE || strcmp(current,source))
        lv_image_set_src(obj,source);
    current=lv_image_get_src(obj);
    if (!current || lv_image_src_get_type(current)!=LV_IMAGE_SRC_FILE || strcmp(current,source)) return;
    window->width=width; window->height=height; window->color=color;
}

lv_obj_t *lv_aic_video_window_create(lv_obj_t *parent)
{
    lv_obj_t *obj=lv_obj_class_create_obj(&lv_aic_video_window_class,parent);
    if (obj) lv_obj_class_init_obj(obj);
    return obj;
}
void lv_aic_video_window_set_size(lv_obj_t *obj, uint32_t width, uint32_t height)
{
    LV_CHECK_OBJ(obj,&lv_aic_video_window_class,return);
    if (!width || !height || width>4096 || height>4096 || (uint64_t)width*height>8U*1024U*1024U) return;
    update(obj,width,height,((video_window_t *)obj)->color);
}
void lv_aic_video_window_set_color(lv_obj_t *obj, lv_color_t color)
{
    LV_CHECK_OBJ(obj,&lv_aic_video_window_class,return);
    video_window_t *window=(video_window_t *)obj;
    update(obj,window->width,window->height,color);
}
#endif
