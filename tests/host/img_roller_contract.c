/* SPDX-License-Identifier: Apache-2.0 */
#include <assert.h>
#include <stdio.h>
#include "lv_img_roller.h"

static void advance(void)
{
    for (int i = 0; i < 60; i++) {
        lv_tick_inc(20);
        lv_timer_handler();
    }
}

static void delete_on_change(lv_event_t *event)
{
    bool *deleted = lv_event_get_user_data(event);
    *deleted = true;
    lv_obj_delete(lv_event_get_target_obj(event));
}

int main(void)
{
    static uint8_t pixels[80 * 80 * 4];
    lv_image_dsc_t img = {0};
    img.header.magic = LV_IMAGE_HEADER_MAGIC;
    img.header.cf = LV_COLOR_FORMAT_ARGB8888;
    img.header.w = img.header.h = 80;
    img.header.stride = 320;
    img.data_size = sizeof(pixels);
    img.data = pixels;
    lv_init();
    lv_display_t *display = lv_display_create(320, 240);
    assert(display);
    for (int cycle = 0; cycle < 10; cycle++) {
        lv_obj_t *roller = lv_img_roller_create(lv_screen_active());
        assert(roller);
        lv_obj_set_size(roller, 160, 120);
        lv_img_roller_ready(roller);
        assert(lv_img_roller_child_count(roller) == 0);
        assert(lv_img_roller_get_child_by_id(roller, 0) == NULL);
        lv_obj_t *items[5];
        for (int i = 0; i < 5; i++) {
            items[i] = lv_img_roller_add_child(roller, &img);
            assert(items[i]);
        }
        lv_obj_update_layout(roller);
        assert(lv_img_roller_child_count(roller) == 5);
        for (int i = 0; i < 5; i++) assert(lv_img_roller_get_child_by_id(roller, i) == items[i]);
        assert(lv_img_roller_get_child_by_id(roller, 5) == NULL);
        lv_img_roller_set_loop_mode(roller, LV_ROLL_LOOP_OFF);
        lv_img_roller_set_active(roller, 2, LV_ANIM_OFF);
        advance();
        assert(lv_img_get_active_id(roller) == 2);
        lv_img_roller_set_active(roller, 99, LV_ANIM_OFF);
        assert(lv_img_get_active_id(roller) == 2);
        lv_img_roller_set_transform_ratio(roller, 256);
        lv_img_roller_ready(roller);
        for (int i = 0; i < 5; i++) assert(lv_image_get_scale_x(items[i]) == 256);
        lv_img_roller_set_direction(roller, LV_DIR_VER);
        lv_obj_update_layout(roller);
        lv_img_roller_set_active(roller, 3, LV_ANIM_ON);
        advance();
        assert(lv_img_get_active_id(roller) == 3);
        lv_img_roller_set_direction(roller, LV_DIR_HOR);
        lv_img_roller_set_loop_mode(roller, LV_ROLL_LOOP_ON);
        lv_obj_update_layout(roller);
        lv_obj_scroll_to_x(roller, -20, LV_ANIM_OFF);
        advance();
        for (int i = 0; i < 5; i++) assert(lv_img_roller_get_child_by_id(roller, i) == items[i]);
        /* Delete with an animation in flight. */
        lv_img_roller_set_active(roller, 1, LV_ANIM_ON);
        lv_obj_delete(roller);
        advance();
    }
    {
        bool deleted = false;
        lv_obj_t *roller = lv_img_roller_create(lv_screen_active());
        lv_obj_set_size(roller, 160, 120);
        lv_img_roller_set_loop_mode(roller, LV_ROLL_LOOP_OFF);
        for (int i = 0; i < 5; i++) assert(lv_img_roller_add_child(roller, &img));
        lv_obj_update_layout(roller);
        lv_img_roller_set_active(roller, 2, LV_ANIM_OFF);
        advance();
        lv_obj_add_event_cb(roller, delete_on_change, LV_EVENT_VALUE_CHANGED, &deleted);
        lv_img_roller_set_active(roller, 3, LV_ANIM_ON);
        advance();
        assert(deleted);
    }
    /* Removing just the pending target must not leave scroll-end with a stale
     * pointer. Reparented images must also stop acting on their old owner. */
    for(unsigned move=0;move<2;move++) {
        lv_obj_t *roller=lv_img_roller_create(lv_screen_active());
        lv_obj_set_size(roller,160,120);
        lv_img_roller_set_loop_mode(roller,LV_ROLL_LOOP_OFF);
        for(unsigned i=0;i<5;i++) assert(lv_img_roller_add_child(roller,&img));
        lv_obj_update_layout(roller);
        lv_img_roller_set_active(roller,1,LV_ANIM_OFF);advance();
        lv_img_roller_set_active(roller,4,LV_ANIM_ON);
        lv_obj_t *pending=lv_img_roller_get_child_by_id(roller,4);
        if(move) {
            lv_obj_set_parent(pending,lv_screen_active());
            lv_obj_send_event(pending,LV_EVENT_CLICKED,NULL);
        } else lv_obj_delete(pending);
        assert(!lv_img_roller_get_child_by_id(roller,4));
        lv_obj_send_event(roller,LV_EVENT_SCROLL_END,NULL);advance();
        lv_img_roller_set_active(roller,2,LV_ANIM_OFF);advance();
        assert(lv_img_get_active_id(roller)==2);
        lv_obj_t *new_child=lv_img_roller_add_child(roller,&img);assert(new_child);
        assert(lv_img_roller_get_child_by_id(roller,5)==new_child);
        lv_obj_delete(roller);if(move) lv_obj_delete(pending);advance();
    }
    /* Loop reordering must preserve every retained child's screen position,
     * including the flex gap crossed by the moved edge item. */
    const int gaps[] = {0, 7, 19};
    for (unsigned vertical = 0; vertical < 2; vertical++)
    for (unsigned end = 0; end < 2; end++)
    for (unsigned gap = 0; gap < 3; gap++) {
        lv_obj_t *roller = lv_img_roller_create(lv_screen_active());
        lv_obj_set_size(roller, 160, 120);
        lv_img_roller_set_direction(roller, vertical ? LV_DIR_VER : LV_DIR_HOR);
        lv_img_roller_set_transform_ratio(roller, 256);
        lv_obj_set_style_pad_column(roller, gaps[gap], 0);
        lv_obj_set_style_pad_row(roller, gaps[gap], 0);
        lv_img_roller_set_loop_mode(roller, LV_ROLL_LOOP_OFF);
        lv_obj_t *items[5];
        for (unsigned i = 0; i < 5; i++) items[i] = lv_img_roller_add_child(roller, &img);
        lv_obj_update_layout(roller);
        int position = end ? 5 * 80 + 4 * gaps[gap] - (vertical ? 120 : 160) + 20 : -20;
        lv_obj_scroll_by(roller, vertical ? 0 : -position, vertical ? -position : 0, LV_ANIM_OFF);
        assert((vertical ? lv_obj_get_scroll_y(roller) : lv_obj_get_scroll_x(roller)) == position);
        lv_obj_update_layout(roller);
        lv_area_t before[5], after;
        for (unsigned i = 0; i < 5; i++) lv_obj_get_coords(items[i], &before[i]);
        lv_img_roller_set_loop_mode(roller, LV_ROLL_LOOP_ON);
        lv_img_roller_ready(roller);
        lv_obj_update_layout(roller);
        assert(lv_obj_get_child(roller, end ? 4 : 0) == items[end ? 0 : 4]);
        for (unsigned i = end ? 1 : 0; i < (end ? 5 : 4); i++) {
            lv_obj_get_coords(items[i], &after);
            if (after.x1 != before[i].x1 || after.y1 != before[i].y1)
                fprintf(stderr, "loop jump vertical=%u end=%u gap=%d id=%u delta=%ld,%ld\n",
                        vertical, end, gaps[gap], i,
                        (long)(after.x1 - before[i].x1), (long)(after.y1 - before[i].y1));
            assert(after.x1 == before[i].x1 && after.y1 == before[i].y1);
        }
        lv_obj_delete(roller);
    }
    lv_display_delete(display);
    lv_deinit();
    return 0;
}
