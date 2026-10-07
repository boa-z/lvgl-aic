/* SPDX-License-Identifier: Apache-2.0 */
#include "lv_swipe_v1.h"
#include <assert.h>
#include <stdio.h>

/* Track LVGL's allocation boundary, including the widget's source arrays. */
static struct { void *p; size_t size; } live[4096];
static size_t bytes_live;
void *__real_lv_malloc_core(size_t);
void *__real_lv_realloc_core(void *, size_t);
void __real_lv_free_core(void *);
static void remember(void *p, size_t size)
{
    if (!p) return;
    for (unsigned i = 0; i < 4096; i++) if (!live[i].p) {
        live[i].p = p; live[i].size = size; bytes_live += size; return;
    }
    assert(false);
}
static void forget(void *p)
{
    if (!p) return;
    for (unsigned i = 0; i < 4096; i++) if (live[i].p == p) {
        bytes_live -= live[i].size; live[i].p = NULL; return;
    }
    assert(false);
}
void *__wrap_lv_malloc_core(size_t size)
{ void *p = __real_lv_malloc_core(size); remember(p, size); return p; }
void *__wrap_lv_realloc_core(void *p, size_t size)
{
    void *next = __real_lv_realloc_core(p, size);
    if (next || !size) { forget(p); remember(next, size); }
    return next;
}
void __wrap_lv_free_core(void *p) { forget(p); __real_lv_free_core(p); }

static uint16_t framebuffer[320 * 240], image_pixels[4][16 * 16];
static const uint16_t colors[] = {0xf800, 0x001f, 0x07e0, 0xffe0};
static lv_image_dsc_t images[4];
static lv_display_t *display;
static lv_point_t pointer;
static bool pressed;
static unsigned changes, clicks;
static void flush(lv_display_t *d, const lv_area_t *a, uint8_t *p)
{ (void)a; (void)p; lv_display_flush_ready(d); }
static void read_pointer(lv_indev_t *i, lv_indev_data_t *data)
{ (void)i; data->point = pointer; data->state = pressed ? LV_INDEV_STATE_PRESSED : LV_INDEV_STATE_RELEASED; }
static void advance(void)
{ for (unsigned i = 0; i < 30; i++) { lv_tick_inc(20); lv_timer_handler(); } }
static void changed(lv_event_t *e) { (void)e; changes++; }
static void clicked(lv_event_t *e) { (void)e; clicks++; }
static void add(lv_obj_t *s, uint16_t id)
{
    lv_swipe_v1_child_add_state_src(s, id, &images[0], &images[1]);
    lv_swipe_v1_child_add_state_src(s, id, &images[2], &images[3]);
}
static lv_obj_t *create(void)
{
    lv_obj_t *s = lv_swipe_v1_create(lv_screen_active());
    assert(s);
    for (unsigned i = 0; i < 4; i++) {
        add(s, i * 10);
        lv_obj_set_pos(lv_swipe_v1_get_child(s, i * 10), 25 + 65 * i, 60);
    }
    lv_swipe_v1_set_anim_params(s, 100, NULL);
    lv_obj_add_event_cb(s, changed, LV_EVENT_VALUE_CHANGED, NULL);
    lv_obj_update_layout(s);
    return s;
}
static void click(lv_indev_t *input, lv_obj_t *obj)
{
    lv_obj_update_layout(lv_screen_active());
    lv_area_t a; lv_obj_get_coords(obj, &a);
    assert(a.x1 >= 0 && a.x2 < 320 && a.y1 >= 0 && a.y2 < 240);
    pointer = (lv_point_t){(a.x1 + a.x2) / 2, (a.y1 + a.y2) / 2};
    pressed = true; lv_tick_inc(40); lv_indev_read(input);
    pressed = false; lv_tick_inc(40); lv_indev_read(input);
    advance();
}
static void check_pixels(lv_obj_t *image)
{
    lv_refr_now(display);
    lv_area_t a; lv_obj_get_coords(image, &a);
    const void *src = lv_image_get_src(image);
    unsigned i;
    for (i = 0; i < 4; i++) if (src == &images[i]) break;
    assert(i < 4);
    int x = (a.x1 + a.x2) / 2, y = (a.y1 + a.y2) / 2;
    assert(x >= 0 && x < 320 && y >= 0 && y < 240);
    assert(framebuffer[y * 320 + x] == colors[i]);
}

static void detach_cases(void)
{
    for (unsigned id = 0; id < 4; id++) for (unsigned animate = 0; animate < 2; animate++)
    for (unsigned owner_first = 0; owner_first < 2; owner_first++) {
        lv_obj_t *s = create(), *moved = lv_swipe_v1_get_child(s, id * 10);
        lv_obj_add_event_cb(moved, clicked, LV_EVENT_CLICKED, NULL);
        unsigned before = changes;
        if (animate) lv_swipe_v1_set_next(s, LV_ANIM_ON);
        lv_obj_set_parent(moved, lv_screen_active());
        assert(lv_swipe_v1_get_child_count(s) == 3);
        assert(!lv_swipe_v1_get_child(s, id * 10));
        assert(lv_swipe_v1_get_child_active_src(s, id * 10) == -1);
        assert(lv_obj_get_event_count(moved) == 1); /* Application callback survives. */
        const void *src = lv_image_get_src(moved);
        lv_obj_set_pos(moved, 8, 8);
        advance();
        assert(changes == before && lv_obj_get_x(moved) == 8 && lv_obj_get_y(moved) == 8);
        assert(lv_image_get_src(moved) == src);
        /* The freed ID can be reused and the remaining widget still works. */
        add(s, id * 10);
        assert(lv_swipe_v1_get_child(s, id * 10) != moved);
        lv_swipe_v1_set_next(s, LV_ANIM_ON); advance();
        assert(changes == before + 1);
        if (owner_first) lv_obj_delete(s);
        unsigned before_click = clicks;
        lv_obj_send_event(moved, LV_EVENT_CLICKED, NULL);
        assert(clicks == before_click + 1 && lv_image_get_src(moved) == src);
        lv_obj_delete(moved);
        if (!owner_first) lv_obj_delete(s);
    }
}

static lv_obj_t *detached;
static void detach_and_restart(lv_event_t *e)
{
    lv_obj_t *s = lv_event_get_target_obj(e);
    lv_obj_remove_event_cb(s, detach_and_restart);
    detached = lv_swipe_v1_get_child(s, 0);
    lv_obj_set_parent(detached, lv_screen_active());
    assert(lv_swipe_v1_get_child_count(s) == 3);
    add(s, 0);
    lv_swipe_v1_set_next(s, LV_ANIM_ON);
}
static void callback_cases(void)
{
    for (unsigned event = 0; event < 2; event++) for (unsigned animate = 0; animate < 2; animate++) {
        lv_obj_t *s = create(); unsigned before = changes;
        lv_obj_add_event_cb(s, detach_and_restart, event ? LV_EVENT_SCROLL_END : LV_EVENT_SCROLL_BEGIN, NULL);
        lv_swipe_v1_set_next(s, animate ? LV_ANIM_ON : LV_ANIM_OFF);
        advance(); advance();
        assert(changes == before + 1 && lv_obj_get_event_count(detached) == 0);
        lv_obj_delete(s); lv_obj_delete(detached);
    }
    /* A returned image or a foreign image is not implicitly adopted. */
    lv_obj_t *s = create(), *other = create();
    lv_obj_t *moved = lv_swipe_v1_get_child(s, 0);
    lv_obj_set_parent(moved, other);
    assert(lv_swipe_v1_get_child_count(s) == 3 && lv_swipe_v1_get_child_count(other) == 4);
    assert(lv_swipe_v1_get_child(other, 0) != moved);
    lv_obj_set_parent(moved, s);
    assert(lv_swipe_v1_get_child_count(s) == 3 && !lv_swipe_v1_get_child(s, 0));
    lv_obj_delete(other); lv_obj_delete(s);
}

static void pointer_cases(lv_indev_t *input)
{
    lv_obj_t *s = create();
    for (unsigned repeat = 0; repeat < 8; repeat++) {
        int16_t active = lv_swipe_v1_get_active_child(s);
        unsigned before = changes;
        click(input, lv_obj_get_child(s, 0));
        assert(changes == before + 1 && lv_swipe_v1_get_active_child(s) != active);
        for (unsigned i = 0; i < 4; i++) check_pixels(lv_obj_get_child(s, i));
        click(input, lv_obj_get_child(s, 2));
        assert(changes == before + 2 && lv_swipe_v1_get_active_child(s) == active);
        for (unsigned slot = 1; slot < 4; slot += 2) {
            lv_obj_t *image = lv_obj_get_child(s, slot);
            const void *src = lv_image_get_src(image);
            click(input, image);
            assert(lv_image_get_src(image) != src && changes == before + 2);
            check_pixels(image);
        }
    }
    lv_obj_delete(s);
}

int main(void)
{
    for (unsigned i = 0; i < 4; i++) {
        for (unsigned j = 0; j < 16 * 16; j++) image_pixels[i][j] = colors[i];
        images[i] = (lv_image_dsc_t){.header = {.magic = LV_IMAGE_HEADER_MAGIC,
            .cf = LV_COLOR_FORMAT_RGB565, .w = 16, .h = 16, .stride = 32},
            .data_size = sizeof image_pixels[i], .data = (const uint8_t *)image_pixels[i]};
    }
    for (unsigned cycle = 0; cycle < 5; cycle++) {
        lv_init(); display = lv_display_create(320, 240); assert(display);
        lv_display_set_color_format(display, LV_COLOR_FORMAT_RGB565);
        lv_display_set_buffers(display, framebuffer, NULL, sizeof framebuffer, LV_DISPLAY_RENDER_MODE_FULL);
        lv_display_set_flush_cb(display, flush);
        lv_indev_t *input = lv_indev_create(); assert(input);
        lv_indev_set_type(input, LV_INDEV_TYPE_POINTER); lv_indev_set_read_cb(input, read_pointer);
        detach_cases(); callback_cases(); pointer_cases(input);
        advance(); assert(!lv_anim_count_running());
        lv_indev_delete(input); lv_display_delete(display); lv_deinit();
        assert(bytes_live == 0);
    }
    puts("PASS swipe ownership: 80 detach lifetimes, 20 callback replacements, 160 pointer clicks, rendered state and zero tracked LVGL heap after teardown");
    return 0;
}
