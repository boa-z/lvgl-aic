/*
 * Copyright (C) 2025, ArtInChip Technology Co., Ltd
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * Authors:  Ning Fang <ning.fang@artinchip.com>
 */

#include "lv_img_roller.h"
#define AIC_LVGL_USE_PRIVATE_API 1
#include "lvgl_aic_private.h"

#if AIC_LVGL_USE_IMG_ROLLER

typedef struct {
    lv_obj_t obj;
    uint32_t img_cur;
    uint32_t img_last;
    uint32_t img_cnt;
    lv_dir_t img_direction;
    bool is_adjusting;
    int32_t threshold;
    bool is_switching;
    lv_roll_loop_enable_t loop_en;
    lv_obj_t *obj_switch;
    lv_anim_enable_t anim_switch;
    uint16_t transform_ratio;
} lv_img_roller_t;

#define MY_CLASS (&lv_img_roller_class)
static void lv_img_roller_constructor(const lv_obj_class_t *class_p, lv_obj_t *obj);
static void lv_img_roller_event(const lv_obj_class_t *class_p, lv_event_t *e);

#define MAX_U32 0xFFFFFFFF

#define LV_ULONG uintptr_t

#define GET_IMG_WIDTH(x) lv_obj_get_width(x)
#define GET_IMG_HEIGHT(x) lv_obj_get_height(x)

const lv_obj_class_t lv_img_roller_class = {
    .constructor_cb = lv_img_roller_constructor,
    .event_cb = lv_img_roller_event,
    .width_def = LV_PCT(100),
    .height_def = LV_PCT(100),
    .base_class = &lv_obj_class,
    .instance_size = sizeof(lv_img_roller_t),
#if LVGL_VERSION_MAJOR > 8
    .name = "img_roller",
#endif
};

lv_obj_t *lv_img_roller_create(lv_obj_t *parent)
{
    LV_LOG_INFO("begin");

    lv_obj_t * obj = lv_obj_class_create_obj(&lv_img_roller_class, parent);
    if (obj == NULL) return NULL;
    lv_obj_class_init_obj(obj);
    return obj;
}

static void image_clicked_event_cb(lv_event_t *e)
{
    lv_obj_t *image = lv_event_get_current_target(e);
    lv_obj_t *parent = lv_obj_get_parent(image);
    /* An application may move an image out of the carousel. The registered
     * callback belongs to its original owner, not an arbitrary new parent. */
    if (parent != lv_event_get_user_data(e)) return;
    lv_img_roller_t *img_roller = (lv_img_roller_t *)parent;

    img_roller->obj_switch = image;
    img_roller->is_switching = true;
    img_roller->anim_switch = LV_ANIM_ON;

    lv_obj_scroll_to_view(image, true);
}

lv_obj_t *lv_img_roller_add_child(lv_obj_t *obj, const void *src)
{
    LV_CHECK_OBJ(obj, MY_CLASS, return NULL);
    lv_img_roller_t *img_roller = (lv_img_roller_t *)obj;

    lv_obj_t *image = lv_image_create(obj);
    if (image == NULL) return NULL;
    lv_image_set_src(image, src);
    lv_obj_update_layout(image);
    lv_image_set_pivot(image, GET_IMG_WIDTH(image) / 2, GET_IMG_HEIGHT(image) / 2);

    lv_obj_set_clickable(image, true);
    lv_obj_set_user_data(image, (void *)(LV_ULONG)img_roller->img_cnt);
    lv_obj_add_event_cb(image, image_clicked_event_cb, LV_EVENT_CLICKED, obj);

    img_roller->img_cnt++;
    return image;
}

uint32_t lv_img_roller_child_count(lv_obj_t *obj)
{
    LV_CHECK_OBJ(obj, MY_CLASS, return 0);
    return lv_obj_get_child_count(obj);
}

void lv_img_roller_set_transform_ratio(lv_obj_t *obj, uint16_t ratio)
{
    LV_CHECK_OBJ(obj, MY_CLASS, return);

    lv_img_roller_t *img_roller = (lv_img_roller_t *)obj;
    img_roller->transform_ratio = LV_CLAMP(64, ratio, 256);
}

void lv_img_roller_set_active(lv_obj_t *obj, uint32_t id, lv_anim_enable_t anim_en)
{
    LV_CHECK_OBJ(obj, MY_CLASS, return);
    int32_t child_cnt;
    lv_img_roller_t *img_roller = (lv_img_roller_t *)obj;

    if (id >= img_roller->img_cnt) {
        return;
    }

    child_cnt = lv_obj_get_child_count(obj);
    for (int32_t i = 0; i < child_cnt; i++) {
        lv_obj_t *child = lv_obj_get_child(obj, i);
        uint32_t cur_id = (uint32_t)(LV_ULONG)lv_obj_get_user_data(child);

        if (cur_id == id) {
            img_roller->obj_switch = child;
            if (img_roller->img_last == MAX_U32) {
               img_roller->is_switching = false;
               img_roller->img_cur = id;
            } else {
                img_roller->is_switching = true;
            }

            img_roller->anim_switch = anim_en;
            lv_obj_scroll_to_view(child, anim_en);
        }
    }
}

void lv_img_roller_ready(lv_obj_t *obj)
{
    LV_CHECK_OBJ(obj, MY_CLASS, return);

#if LVGL_VERSION_MAJOR == 8
    lv_event_send(obj, LV_EVENT_SCROLL, NULL);
#else
    lv_obj_send_event(obj, LV_EVENT_SCROLL, NULL);
#endif
}

uint32_t lv_img_get_active_id(lv_obj_t *obj)
{
    LV_CHECK_OBJ(obj, MY_CLASS, return 0);
    lv_img_roller_t *img_roller = (lv_img_roller_t *)obj;
    return img_roller->img_cur;
}

lv_obj_t *lv_img_roller_get_child_by_id(lv_obj_t *obj, uint32_t id)
{
    LV_CHECK_OBJ(obj, MY_CLASS, return NULL);
    int32_t child_cnt;
    lv_img_roller_t *img_roller = (lv_img_roller_t *)obj;

    if (id >= img_roller->img_cnt) {
        return NULL;
    }

    child_cnt = lv_obj_get_child_count(obj);
    for (int32_t i = 0; i < child_cnt; i++) {
        lv_obj_t *child = lv_obj_get_child(obj, i);
        uint32_t cur_id = (uint32_t)(LV_ULONG)lv_obj_get_user_data(child);

        if (cur_id == id)
            return child;
    }

    return NULL;
}

void lv_img_roller_set_loop_mode(lv_obj_t *obj, lv_roll_loop_enable_t loop_en)
{
    LV_CHECK_OBJ(obj, MY_CLASS, return);
    lv_img_roller_t *img_roller = (lv_img_roller_t *)obj;

    img_roller->loop_en = loop_en;
}

static void lv_img_roller_constructor(const lv_obj_class_t *class_p, lv_obj_t *obj)
{
    LV_UNUSED(class_p);
    lv_img_roller_t *img_roller = (lv_img_roller_t *)obj;

    img_roller->img_cur = 0;
    img_roller->img_direction = LV_DIR_HOR;
    img_roller->is_adjusting = false;
    img_roller->threshold = 0;
    img_roller->img_cnt = 0;
    img_roller->is_switching = false;
    img_roller->anim_switch = false;
    img_roller->loop_en = LV_ROLL_LOOP_ON;
    img_roller->img_last = MAX_U32;
    img_roller->transform_ratio = 153; /* 256 * 0.6 */

    lv_obj_set_size(obj, LV_PCT(100), LV_PCT(100));
    lv_obj_set_style_radius(obj, 0, 0);
    lv_obj_set_style_bg_opa(obj, LV_OPA_0, 0);
    lv_obj_set_style_border_width(obj, 0, 0);
    lv_obj_set_style_outline_width(obj, 0, 0);
    lv_obj_set_style_pad_column(obj, 0, 0);
    lv_obj_set_style_pad_row(obj, 0, 0);
    lv_obj_set_style_pad_all(obj, 0, 0);
    lv_obj_set_scrollbar_mode(obj, LV_SCROLLBAR_MODE_OFF);
    lv_obj_set_scroll_dir(obj, LV_DIR_HOR);
    lv_obj_set_flex_flow(obj, LV_FLEX_FLOW_ROW);
    lv_obj_set_scroll_snap_x(obj, LV_SCROLL_SNAP_CENTER);
    lv_obj_set_scroll_snap_y(obj, LV_SCROLL_SNAP_NONE);
}

void lv_img_roller_set_direction(lv_obj_t *obj, lv_dir_t dir)
{
    LV_CHECK_OBJ(obj, MY_CLASS, return);
    lv_img_roller_t * img_roller = (lv_img_roller_t *)obj;

    if (dir != LV_DIR_HOR && dir != LV_DIR_VER) {
        dir = LV_DIR_HOR;
    }

    if (dir == LV_DIR_HOR) {
        lv_obj_set_scroll_dir(obj, LV_DIR_HOR);
        lv_obj_set_flex_flow(obj, LV_FLEX_FLOW_ROW);
        lv_obj_set_scroll_snap_x(obj, LV_SCROLL_SNAP_CENTER);
        lv_obj_set_scroll_snap_y(obj, LV_SCROLL_SNAP_NONE);
    } else {
        lv_obj_set_scroll_dir(obj, LV_DIR_VER);
        lv_obj_set_flex_flow(obj, LV_FLEX_FLOW_COLUMN);
        lv_obj_set_scroll_snap_x(obj, LV_SCROLL_SNAP_NONE);
        lv_obj_set_scroll_snap_y(obj, LV_SCROLL_SNAP_CENTER);
    }

    img_roller->img_direction = dir;
}

static int32_t lv_get_content_width(lv_obj_t *obj)
{
    int32_t w = 0;
    int32_t pad_column = lv_obj_get_style_pad_column(obj, 0);
    int total = lv_obj_get_child_count(obj);

    for (int i = 0; i < total; i++) {
        w += lv_obj_get_width(lv_obj_get_child(obj, i));
        if (i < total - 1) w += pad_column;
    }
    return w + lv_obj_get_style_pad_left(obj, 0) + lv_obj_get_style_pad_right(obj, 0);
}

static int32_t lv_get_content_height(lv_obj_t *obj)
{
    int32_t h = 0;
    int32_t pad_row = lv_obj_get_style_pad_row(obj, 0);
    int total = lv_obj_get_child_count(obj);

    for (int i = 0; i < total; i++) {
        h += lv_obj_get_height(lv_obj_get_child(obj, i));
        if (i < total - 1) h += pad_row;
    }
    return h + lv_obj_get_style_pad_top(obj, 0) + lv_obj_get_style_pad_bottom(obj, 0);
}

static void lv_img_loop_event(lv_obj_t *target)
{
    lv_img_roller_t *img_roller;
    lv_obj_t *child;
    lv_dir_t dir;

    img_roller = (lv_img_roller_t *)target;
    dir = img_roller->img_direction;
    child = lv_obj_get_child(target, 0);
    if (!child)
        return;

    if (img_roller->loop_en && !img_roller->is_adjusting &&
        lv_obj_get_child_count(target) > 1) {
        int32_t child_size;
        int32_t threshold;
        int32_t scroll_pos;
        int32_t cont_size;
        int32_t content_size;

        threshold = img_roller->threshold;
        if (dir == LV_DIR_HOR) {
            child_size = GET_IMG_WIDTH(child);
            scroll_pos = lv_obj_get_scroll_x(target);
            cont_size = lv_obj_get_width(target);
            content_size = lv_get_content_width(target);
        } else {
            child_size = GET_IMG_HEIGHT(child);
            scroll_pos = lv_obj_get_scroll_y(target);
            cont_size = lv_obj_get_height(target);
            content_size = lv_get_content_height(target);
        }

        /* No wrap is needed if all items fit in the viewport. */
        if (child_size <= 0 || content_size <= cont_size) return;
        img_roller->is_adjusting = true;
        if (scroll_pos < -threshold) {
            lv_obj_t *last_child = lv_obj_get_child(target, lv_obj_get_child_count(target) - 1);
            lv_obj_move_to_index(last_child, 0);
            if (dir == LV_DIR_HOR)
                lv_obj_scroll_to_x(target, scroll_pos + child_size, LV_ANIM_OFF);
            else
                lv_obj_scroll_to_y(target, scroll_pos + child_size, LV_ANIM_OFF);

        } else if (scroll_pos > content_size - cont_size + threshold) {
            lv_obj_t *first_child = lv_obj_get_child(target, 0);
            lv_obj_move_to_index(first_child, lv_obj_get_child_count(target) - 1);

            if (dir == LV_DIR_HOR)
                lv_obj_scroll_to_x(target, scroll_pos - child_size, LV_ANIM_OFF);
            else
                lv_obj_scroll_to_y(target, scroll_pos - child_size, LV_ANIM_OFF);
        }
        img_roller->is_adjusting = false;
    }
}

static void lv_img_zoom_event(lv_obj_t *target)
{
    lv_img_roller_t *img_roller;
    lv_obj_t *child;
    lv_dir_t dir;
    int32_t region;
    lv_area_t cont_a;
    int32_t cont_center;

    img_roller = (lv_img_roller_t *)target;
    dir = img_roller->img_direction;
    child = lv_obj_get_child(target, 0);
    if (!child)
        return;

    lv_obj_get_coords(target, &cont_a);

    if (dir == LV_DIR_HOR) {
        region = GET_IMG_WIDTH(child);
        cont_center = cont_a.x1 + lv_area_get_width(&cont_a) / 2;
    } else {
        region = GET_IMG_HEIGHT(child);
        cont_center = cont_a.y1 + lv_area_get_height(&cont_a) / 2;
    }

    int32_t child_cnt = lv_obj_get_child_count(target);
    for (int32_t i = 0; i < child_cnt; i++) {
        lv_obj_t * child = lv_obj_get_child(target, i);
        lv_area_t child_a;
        int32_t child_center;
        int32_t diff;
        int32_t ratio;

        lv_obj_get_coords(child, &child_a);

        if (dir == LV_DIR_HOR)
            child_center = child_a.x1 + lv_area_get_width(&child_a) / 2;
        else
            child_center = child_a.y1 + lv_area_get_height(&child_a) / 2;

        diff = LV_ABS(child_center - cont_center);
        if (diff >= region) {
            diff = region;
        }

        diff = region - diff;
        if (region <= 1) continue;
        ratio = lv_map(diff, 0, region - region / 10, img_roller->transform_ratio, 256);
        ratio = LV_CLAMP(img_roller->transform_ratio, ratio, 256);
        lv_image_set_scale(child, ratio);
    }
}

static void lv_img_find_cur_img(lv_obj_t *target)
{
    lv_img_roller_t *img_roller;
    lv_obj_t *child;
    lv_dir_t dir;
    int32_t region;
    lv_area_t cont_a;
    int32_t cont_center;

    img_roller = (lv_img_roller_t *)target;
    dir = img_roller->img_direction;
    child = lv_obj_get_child(target, 0);
    if (!child)
        return;

    lv_obj_get_coords(target, &cont_a);

    if (dir == LV_DIR_HOR) {
        region = GET_IMG_WIDTH(child);
        cont_center = cont_a.x1 + lv_area_get_width(&cont_a) / 2;
    } else {
        region = GET_IMG_HEIGHT(child);
        cont_center = cont_a.y1 + lv_area_get_height(&cont_a) / 2;
    }

    int32_t child_cnt = lv_obj_get_child_count(target);

    if (child_cnt > 0) {
        lv_obj_t *min_child;
        int32_t min_diff;

        for (int32_t i = 0; i < child_cnt; i++) {
            lv_obj_t * child = lv_obj_get_child(target, i);
            lv_area_t child_a;
            int32_t child_center;
            int32_t diff;

            lv_obj_get_coords(child, &child_a);

            if (dir == LV_DIR_HOR)
                child_center = child_a.x1 + lv_area_get_width(&child_a) / 2;
            else
                child_center = child_a.y1 + lv_area_get_height(&child_a) / 2;

            diff = LV_ABS(child_center - cont_center);
            if (diff >= region) {
                diff = region;
            }

            if (i == 0) {
                min_child = child;
                min_diff = diff;
            } else {
                if (diff < min_diff) {
                    min_child = child;
                    min_diff = diff;
                }
            }
        }

        img_roller->img_cur = (uint32_t)(LV_ULONG)lv_obj_get_user_data(min_child);
    }
}

static bool contains_child(lv_obj_t *parent, const lv_obj_t *child)
{
    /* Compare membership without dereferencing a possibly deleted target. */
    uint32_t count = lv_obj_get_child_count(parent);
    for (uint32_t i = 0; i < count; i++)
        if (lv_obj_get_child(parent, i) == child) return true;
    return false;
}

static void lv_img_roller_event(const lv_obj_class_t *class_p, lv_event_t *e)
{
    LV_UNUSED(class_p);
    lv_obj_t *target;
    lv_img_roller_t *img_roller;
    lv_result_t res;

    target = lv_event_get_current_target(e);
    lv_event_code_t code = lv_event_get_code(e);
    img_roller = (lv_img_roller_t *)target;

    res = lv_obj_event_base(&lv_img_roller_class, e);
    if(res != LV_RESULT_OK)
        return;

    if ((code == LV_EVENT_CHILD_CHANGED || code == LV_EVENT_SCROLL_END) &&
        img_roller->obj_switch && !contains_child(target, img_roller->obj_switch)) {
        img_roller->obj_switch = NULL;
        img_roller->is_switching = false;
    }
    if (img_roller->is_adjusting) return;
    if (code == LV_EVENT_SCROLL) {
        lv_img_loop_event(target);
        lv_img_zoom_event(target);
    } else if (code == LV_EVENT_SCROLL_END) {
        if (img_roller->is_switching) {
            img_roller->is_switching = false;
            lv_obj_scroll_to_view(img_roller->obj_switch, img_roller->anim_switch);
        }

        if (img_roller->img_last == MAX_U32) {
            img_roller->img_last = img_roller->img_cur;
        }

        lv_img_find_cur_img(target);
        if (img_roller->img_last != img_roller->img_cur) {
            /* VALUE_CHANGED handlers may delete this widget. */
            img_roller->img_last = img_roller->img_cur;
#if LVGL_VERSION_MAJOR == 8
            lv_event_send(target, LV_EVENT_VALUE_CHANGED, NULL);
#else
            lv_obj_send_event(target, LV_EVENT_VALUE_CHANGED, NULL);
#endif
            return;
        }
        img_roller->img_last = img_roller->img_cur;
    }
}

#endif /*LV_USE_IMG_ROLLER*/
