/* SPDX-License-Identifier: Apache-2.0 */
#include "lv_img_roller.h"
#include <assert.h>
#include <stdio.h>

static uint16_t framebuffer[320 * 240], pixels[5][80 * 80];
static lv_image_dsc_t images[5];
static lv_point_t pointer;
static bool pressed;
static void flush(lv_display_t *d, const lv_area_t *a, uint8_t *p)
{ (void)a; (void)p; lv_display_flush_ready(d); }
static void read_pointer(lv_indev_t *i, lv_indev_data_t *data)
{ (void)i; data->point = pointer; data->state = pressed ? LV_INDEV_STATE_PRESSED : LV_INDEV_STATE_RELEASED; }
static void advance(void)
{ for (unsigned i = 0; i < 60; i++) { lv_tick_inc(20); lv_timer_handler(); } }
static void sample(lv_indev_t *input, bool down)
{ pressed = down; lv_tick_inc(20); lv_indev_read(input); lv_timer_handler(); }

static unsigned changed;
static lv_obj_t *retired;
/* Detect post-callback traversal even if the freed heap still looks readable. */
lv_obj_t *__real_lv_obj_get_child(const lv_obj_t *, int32_t);
lv_obj_t *__wrap_lv_obj_get_child(const lv_obj_t *obj, int32_t index)
{
    assert(!retired || obj != retired);
    return __real_lv_obj_get_child(obj, index);
}
static void count_change(lv_event_t *e) { (void)e; changed++; }
static void delete_on_change(lv_event_t *e)
{
    bool *deleted = lv_event_get_user_data(e);
    *deleted = true;
    lv_obj_t *obj = lv_event_get_target_obj(e);
    lv_obj_delete(obj);
    retired = obj;
}
static lv_obj_t *create(unsigned vertical, unsigned loop)
{
    retired = NULL;
    lv_obj_t *roller = lv_img_roller_create(lv_screen_active());
    lv_obj_set_pos(roller, 40, 20); lv_obj_set_size(roller, 220, 180);
    lv_img_roller_set_direction(roller, vertical ? LV_DIR_VER : LV_DIR_HOR);
    lv_img_roller_set_loop_mode(roller, loop ? LV_ROLL_LOOP_ON : LV_ROLL_LOOP_OFF);
    lv_img_roller_set_transform_ratio(roller, 256);
    lv_obj_set_style_pad_column(roller, 12, 0);
    lv_obj_set_style_pad_row(roller, 12, 0);
    for (unsigned i = 0; i < 5; i++) assert(lv_img_roller_add_child(roller, &images[i]));
    lv_img_roller_set_active(roller, 2, LV_ANIM_OFF); advance();
    assert(lv_img_get_active_id(roller) == 2);
    return roller;
}
static void point_at(lv_obj_t *obj)
{
    lv_obj_update_layout(obj);
    lv_area_t a; lv_obj_get_coords(obj, &a);
    pointer = (lv_point_t){a.x1 + lv_area_get_width(&a) / 2, a.y1 + lv_area_get_height(&a) / 2};
    assert(pointer.x >= 40 && pointer.x < 260 && pointer.y >= 20 && pointer.y < 200);
}
static void drag(lv_indev_t *input, unsigned vertical, int sign)
{
    sample(input, true);
    for (unsigned i = 0; i < 8; i++) {
        if (vertical) pointer.y += sign * 10;
        else pointer.x += sign * 10;
        sample(input, true);
    }
    sample(input, false); advance();
}

static int32_t unrelated_value;
static void unrelated_exec(void *obj, int32_t value)
{
    (void)obj;
    assert(value >= 0 && value <= 100);
    unrelated_value = value;
}
static void check_center(lv_display_t *display, lv_obj_t *roller, unsigned vertical,
                         const uint16_t *colors)
{
    uint32_t id = lv_img_get_active_id(roller);
    assert(id < 5);
    point_at(lv_img_roller_get_child_by_id(roller, id));
    assert(vertical ? pointer.y == 110 : pointer.x == 150);
    lv_refr_now(display);
    assert(framebuffer[pointer.y * 320 + pointer.x] == colors[id]);
}

int main(int argc, char **argv)
{
    (void)argv;
    const uint16_t colors[] = {0xf800, 0x07e0, 0x001f, 0xffe0, 0xf81f};
    for (unsigned i = 0; i < 5; i++) {
        for (unsigned j = 0; j < 80 * 80; j++) pixels[i][j] = colors[i];
        images[i] = (lv_image_dsc_t){.header = {.magic = LV_IMAGE_HEADER_MAGIC,
            .cf = LV_COLOR_FORMAT_RGB565, .w = 80, .h = 80, .stride = 160},
            .data_size = sizeof pixels[i], .data = (const uint8_t *)pixels[i]};
    }
    lv_init(); lv_display_t *display = lv_display_create(320, 240); assert(display);
    lv_display_set_color_format(display, LV_COLOR_FORMAT_RGB565);
    lv_display_set_buffers(display, framebuffer, NULL, sizeof framebuffer, LV_DISPLAY_RENDER_MODE_FULL);
    lv_display_set_flush_cb(display, flush);
    lv_indev_t *input = lv_indev_create(); assert(input);
    lv_indev_set_type(input, LV_INDEV_TYPE_POINTER); lv_indev_set_read_cb(input, read_pointer);
    /* An optional argument isolates synchronous deletion for the pre-fix repro. */
    if (argc == 1) {
        for (unsigned repeat = 0; repeat < 2; repeat++)
        for (unsigned vertical = 0; vertical < 2; vertical++)
        for (unsigned loop = 0; loop < 2; loop++)
        for (unsigned request = 0; request < 3; request++)
        for (int sign = -1; sign <= 1; sign += 2) {
            lv_obj_t *roller = create(vertical, loop);
            unsigned before = changed;
            lv_obj_add_event_cb(roller, count_change, LV_EVENT_VALUE_CHANGED, NULL);
            point_at(lv_img_roller_get_child_by_id(roller, 2));
            if (request == 0) { sample(input, true); sample(input, false); }
            else lv_img_roller_set_active(roller, 2, request == 1 ? LV_ANIM_OFF : LV_ANIM_ON);
            advance(); assert(changed == before);
            drag(input, vertical, sign);
            uint32_t id = lv_img_get_active_id(roller);
            if (id == 2) fprintf(stderr, "drag retained old target: vertical=%u loop=%u request=%u sign=%d\n",
                                  vertical, loop, request, sign);
            assert(id != 2 && id < 5 && changed > before);
            point_at(lv_img_roller_get_child_by_id(roller, id));
            if (!(vertical ? pointer.y == 110 : pointer.x == 150))
                fprintf(stderr, "center vertical=%u loop=%u request=%u sign=%d id=%lu at=%ld,%ld\n",
                        vertical, loop, request, sign, (unsigned long)id, (long)pointer.x, (long)pointer.y);
            assert(vertical ? pointer.y == 110 : pointer.x == 150);
            lv_refr_now(display);
            assert(framebuffer[pointer.y * 320 + pointer.x] == colors[id]);
            for (unsigned i = 0; i < 5; i++) assert(lv_img_roller_get_child_by_id(roller, i));
            lv_obj_delete(roller); advance();
        }
    }
    if (argc == 1) {
        for (unsigned vertical = 0; vertical < 2; vertical++)
        for (unsigned loop = 0; loop < 2; loop++)
        for (unsigned animate = 0; animate < 2; animate++) {
            lv_obj_t *roller = create(vertical, loop);
            for (int step = 0; step < 10; step++) {
                unsigned id = step < 5 ? step : 9 - step;
                lv_img_roller_set_active(roller, id, animate ? LV_ANIM_ON : LV_ANIM_OFF);
                advance(); assert(lv_img_get_active_id(roller) == id);
                check_center(display, roller, vertical, colors);
            }
            lv_obj_delete(roller); advance();
        }
        for (unsigned vertical = 0; vertical < 2; vertical++)
        for (unsigned loop = 0; loop < 2; loop++) {
            lv_obj_t *roller = create(vertical, loop);
            lv_img_roller_set_active(roller, 4, LV_ANIM_ON);
            for (unsigned tick = 0; tick < 3; tick++) { lv_tick_inc(20); lv_timer_handler(); }
            pointer = vertical ? (lv_point_t){80, 110} : (lv_point_t){150, 60};
            drag(input, vertical, 1);
            assert(lv_img_get_active_id(roller) != 4);
            check_center(display, roller, vertical, colors);
            lv_obj_delete(roller); advance();
        }
        for (unsigned vertical = 0; vertical < 2; vertical++)
        for (int sign = -1; sign <= 1; sign += 2) {
            lv_obj_t *roller = create(vertical, 1);
            /* An unrelated animation on the same owner must not be rebased. */
            lv_anim_t anim; lv_anim_init(&anim); lv_anim_set_var(&anim, roller);
            lv_anim_set_values(&anim, 0, 100); lv_anim_set_duration(&anim, 5000);
            lv_anim_set_exec_cb(&anim, unrelated_exec); assert(lv_anim_start(&anim));
            for (unsigned step = 0; step < 10; step++) {
                uint32_t before = lv_img_get_active_id(roller);
                point_at(lv_img_roller_get_child_by_id(roller, before));
                drag(input, vertical, sign);
                assert(lv_img_get_active_id(roller) != before);
                check_center(display, roller, vertical, colors);
            }
            assert(unrelated_value == 100);
            lv_obj_delete(roller); advance();
        }
    }
    for (unsigned vertical = 0; vertical < 2; vertical++)
    for (unsigned loop = 0; loop < 2; loop++)
    for (unsigned animate = 0; animate < 2; animate++) {
        lv_obj_t *roller = create(vertical, loop);
        bool deleted = false;
        lv_obj_add_event_cb(roller, delete_on_change, LV_EVENT_VALUE_CHANGED, &deleted);
        lv_img_roller_set_active(roller, 3, animate ? LV_ANIM_ON : LV_ANIM_OFF);
        advance(); assert(deleted);
    }
    retired = NULL;
    lv_indev_delete(input); lv_display_delete(display); lv_deinit();
    puts(argc == 1 ? "PASS image roller: 48 no-op selection drags, 40 looping drags, four animated takeovers, 80 explicit selections, rendered centered IDs and eight callback deletions"
                   : "PASS image roller: eight synchronous/animated callback deletions");
    return 0;
}
