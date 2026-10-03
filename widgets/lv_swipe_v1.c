/*
 * Copyright (C) 2025, ArtInChip Technology Co., Ltd
 * SPDX-License-Identifier: Apache-2.0
 * Original author: ZeQuan Liang <zequan.liang@artinchip.com>
 *
 * LVGL 9.6 adaptation of the SDK four-position swipe widget.
 */
#include "lv_swipe_v1.h"
#define AIC_LVGL_USE_PRIVATE_API 1
#include "lvgl_aic_private.h"

#if AIC_LVGL_USE_SWIPE_V1
typedef struct {
    const void *active;
    const void *deactive;
} source_pair_t;

typedef struct {
    lv_obj_t *obj;
    source_pair_t *sources;
    uint16_t id, count, current;
    int32_t x, y, scale_x, scale_y;
    int32_t end_x, end_y, end_scale_x, end_scale_y;
} child_t;

typedef struct {
    lv_obj_t obj;
    child_t children[4];
    uint16_t count, duration;
    bool switching;
    lv_anim_path_cb_t path;
} swipe_t;

#define MY_CLASS (&lv_swipe_v1_class)
static void progress(void *var, int32_t value);
static void child_event(lv_event_t *e);

static child_t *find_child(lv_obj_t *obj, uint16_t id)
{
    swipe_t *s = (swipe_t *)obj;
    for (uint16_t i = 0; i < s->count; i++)
        if (s->children[i].id == id) return &s->children[i];
    return NULL;
}

static void constructor(const lv_obj_class_t *cls, lv_obj_t *obj)
{
    (void)cls;
    swipe_t *s = (swipe_t *)obj;
    s->duration = 1000;
    s->path = lv_anim_path_linear;
    lv_obj_remove_style_all(obj);
    lv_obj_set_scrollable(obj, false);
    lv_obj_set_clickable(obj, true);
}

static void destructor(const lv_obj_class_t *cls, lv_obj_t *obj)
{
    (void)cls;
    swipe_t *s = (swipe_t *)obj;
    lv_anim_delete(obj, progress);
    for (uint16_t i = 0; i < s->count; i++) lv_free(s->children[i].sources);
}

const lv_obj_class_t lv_swipe_v1_class = {
    .constructor_cb = constructor, .destructor_cb = destructor,
    .width_def = LV_PCT(100), .height_def = LV_PCT(100),
    .base_class = &lv_obj_class, .instance_size = sizeof(swipe_t),
    .name = "swipe_v1",
};

lv_obj_t *lv_swipe_v1_create(lv_obj_t *parent)
{
    lv_obj_t *obj = lv_obj_class_create_obj(MY_CLASS, parent);
    if (obj) lv_obj_class_init_obj(obj);
    return obj;
}

void lv_swipe_v1_child_add_state_src(lv_obj_t *obj, uint16_t id, void *active, void *deactive)
{
    LV_CHECK_OBJ(obj, MY_CLASS, return);
    swipe_t *s = (swipe_t *)obj;
    if (id > INT16_MAX || s->switching || !active || !deactive) return;
    child_t *c = find_child(obj, id);
    if ((!c && s->count == 4) || (c && c->count == 32768U)) return;
    size_t count = c ? c->count : 0;
    source_pair_t *pairs = lv_realloc(c ? c->sources : NULL, (count + 1) * sizeof(*pairs));
    if (!pairs) return;
    pairs[count] = (source_pair_t){active, deactive};
    if (!c) {
        lv_obj_t *image = lv_image_create(obj);
        if (!image) { lv_free(pairs); return; }
        c = &s->children[s->count++];
        lv_memzero(c, sizeof(*c));
        c->obj = image;
        c->id = id;
        lv_obj_set_clickable(image, true);
        lv_obj_add_event_cb(image, child_event, LV_EVENT_ALL, obj);
    }
    c->sources = pairs;
    c->count = (uint16_t)(count + 1);
    if (count == 0) lv_image_set_src(c->obj, active);
}

static void set_source(lv_obj_t *obj, uint16_t id, uint16_t index, bool active)
{
    child_t *c = find_child(obj, id);
    if (!c || index >= c->count) return;
    c->current = index;
    lv_image_set_src(c->obj, active ? c->sources[index].active : c->sources[index].deactive);
}

void lv_swipe_v1_set_child_active_src(lv_obj_t *obj, uint16_t id, uint16_t index)
{
    LV_CHECK_OBJ(obj, MY_CLASS, return);
    set_source(obj, id, index, true);
}

void lv_swipe_v1_set_child_activate(lv_obj_t *obj, uint16_t id, uint16_t index)
{
    lv_swipe_v1_set_child_active_src(obj, id, index);
}

void lv_swipe_v1_set_child_deactive_src(lv_obj_t *obj, uint16_t id, uint16_t index)
{
    LV_CHECK_OBJ(obj, MY_CLASS, return);
    set_source(obj, id, index, false);
}

/* One object-bound animation owns the complete transition. No sentinel or
 * per-child animation can outlive the widget or report a canceled completion. */
static int32_t interpolate(int32_t start, int32_t end, int32_t value)
{
    return (int32_t)(start + ((int64_t)end - start) * value / 1024);
}

static void progress(void *var, int32_t value)
{
    swipe_t *s = var;
    for (uint16_t i = 0; i < s->count; i++) {
        child_t *c = &s->children[i];
        lv_obj_set_pos(c->obj, interpolate(c->x, c->end_x, value),
                       interpolate(c->y, c->end_y, value));
        lv_obj_set_style_transform_scale_x(c->obj, interpolate(c->scale_x, c->end_scale_x, value), 0);
        lv_obj_set_style_transform_scale_y(c->obj, interpolate(c->scale_y, c->end_scale_y, value), 0);
    }
}

static void finish(lv_obj_t *obj)
{
    swipe_t *s = (swipe_t *)obj;
    /* Retain the guard during SCROLL_END to reject nested transitions. */
    if (lv_obj_send_event(obj, LV_EVENT_SCROLL_END, obj) != LV_RESULT_OK) return;
    s->switching = false;
    lv_obj_send_event(obj, LV_EVENT_VALUE_CHANGED, obj);
}

static void completed(lv_anim_t *anim)
{
    finish(anim->var);
}

static void transition(lv_obj_t *obj, bool next, lv_anim_enable_t enable)
{
    LV_CHECK_OBJ(obj, MY_CLASS, return);
    swipe_t *s = (swipe_t *)obj;
    if (s->count != 4 || s->switching) return;
    s->switching = true;
    if (lv_obj_send_event(obj, LV_EVENT_SCROLL_BEGIN, obj) != LV_RESULT_OK) return;
    /* A callback may remove a child and cancel this transition. */
    if (s->count != 4 || !s->switching) return;
    lv_obj_update_layout(obj);
    for (uint16_t i = 0; i < 4; i++) {
        child_t *c = &s->children[i];
        c->x = lv_obj_get_x_aligned(c->obj);
        c->y = lv_obj_get_y_aligned(c->obj);
        c->scale_x = lv_obj_get_style_transform_scale_x(c->obj, 0);
        c->scale_y = lv_obj_get_style_transform_scale_y(c->obj, 0);
    }
    for (uint16_t i = 0; i < 4; i++) {
        child_t *c = &s->children[i];
        child_t *to = &s->children[(i + (next ? 1 : 3)) % 4];
        c->end_x = to->x; c->end_y = to->y;
        c->end_scale_x = to->scale_x; c->end_scale_y = to->scale_y;
    }
    child_t saved = s->children[next ? 3 : 0];
    if (next) {
        for (int i = 3; i > 0; i--) s->children[i] = s->children[i - 1];
        s->children[0] = saved;
    }
    else {
        for (int i = 0; i < 3; i++) s->children[i] = s->children[i + 1];
        s->children[3] = saved;
    }
    for (uint16_t i = 0; i < 4; i++) {
        child_t *c = &s->children[i];
        set_source(obj, c->id, c->current, i == 3);
        lv_obj_move_to_index(c->obj, i);
    }
    if (!enable || s->duration == 0) {
        progress(obj, 1024);
        finish(obj);
        return;
    }
    lv_anim_t anim;
    lv_anim_init(&anim);
    lv_anim_set_var(&anim, obj);
    lv_anim_set_values(&anim, 0, 1024);
    lv_anim_set_duration(&anim, s->duration);
    lv_anim_set_exec_cb(&anim, progress);
    lv_anim_set_path_cb(&anim, s->path);
    lv_anim_set_completed_cb(&anim, completed);
    if (!lv_anim_start(&anim)) {
        progress(obj, 1024);
        finish(obj);
    }
}

void lv_swipe_v1_set_next(lv_obj_t *obj, lv_anim_enable_t enable) { transition(obj, true, enable); }
void lv_swipe_v1_set_prev(lv_obj_t *obj, lv_anim_enable_t enable) { transition(obj, false, enable); }

void lv_swipe_v1_set_anim_params(lv_obj_t *obj, uint16_t time, lv_anim_path_cb_t path)
{
    LV_CHECK_OBJ(obj, MY_CLASS, return);
    swipe_t *s = (swipe_t *)obj;
    s->duration = time;
    s->path = path ? path : lv_anim_path_linear;
}

int16_t lv_swipe_v1_get_active_child(lv_obj_t *obj)
{
    LV_CHECK_OBJ(obj, MY_CLASS, return -1);
    swipe_t *s = (swipe_t *)obj;
    return s->count ? (int16_t)s->children[s->count - 1].id : -1;
}

uint16_t lv_swipe_v1_get_child_count(lv_obj_t *obj)
{
    LV_CHECK_OBJ(obj, MY_CLASS, return 0);
    return ((swipe_t *)obj)->count;
}

int16_t lv_swipe_v1_get_child_active_src(lv_obj_t *obj, uint16_t id)
{
    LV_CHECK_OBJ(obj, MY_CLASS, return -1);
    child_t *c = find_child(obj, id);
    return c ? (int16_t)c->current : -1;
}

lv_obj_t *lv_swipe_v1_get_child(lv_obj_t *obj, uint16_t id)
{
    LV_CHECK_OBJ(obj, MY_CLASS, return NULL);
    child_t *c = find_child(obj, id);
    return c ? c->obj : NULL;
}

static void child_event(lv_event_t *e)
{
    swipe_t *s = lv_event_get_user_data(e);
    lv_obj_t *image = lv_event_get_current_target_obj(e);
    uint16_t i;
    for (i = 0; i < s->count; i++) if (s->children[i].obj == image) break;
    if (i == s->count) return;
    if (lv_event_get_code(e) == LV_EVENT_DELETE) {
        lv_anim_delete(s, progress);
        s->switching = false;
        lv_free(s->children[i].sources);
        for (; i + 1 < s->count; i++) s->children[i] = s->children[i + 1];
        s->count--;
        return;
    }
    if (lv_event_get_code(e) != LV_EVENT_CLICKED || s->count != 4 || s->switching) return;
    if (i == 0) lv_swipe_v1_set_prev((lv_obj_t *)s, LV_ANIM_ON);
    else if (i == 2) lv_swipe_v1_set_next((lv_obj_t *)s, LV_ANIM_ON);
    else {
        child_t *c = &s->children[i];
        set_source((lv_obj_t *)s, c->id, (uint16_t)((c->current + 1) % c->count), true);
    }
}

void lv_swipe_v1_toggle_child_active(lv_obj_t *obj)
{
    LV_CHECK_OBJ(obj, MY_CLASS, return);
    swipe_t *s = (swipe_t *)obj;
    if (s->count) lv_obj_send_event(s->children[s->count - 1].obj, LV_EVENT_CLICKED, NULL);
}
#endif
