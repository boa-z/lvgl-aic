/* SPDX-License-Identifier: Apache-2.0 */
#include "lv_aic_manual_test.h"
#include "lv_aic_font_test.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifdef AIC_MANUAL_PREVIEW
#include "manual_preview.h"
#endif

static uint8_t pixels[800 * 480 * 2];
static lv_point_t pointer;
static bool pressed;
static void flush(lv_display_t *display, const lv_area_t *area, uint8_t *data)
{
    (void)area; (void)data;
    lv_display_flush_ready(display);
}
static void read_pointer(lv_indev_t *indev, lv_indev_data_t *data)
{
    (void)indev;
    data->point = pointer;
    data->state = pressed ? LV_INDEV_STATE_PRESSED : LV_INDEV_STATE_RELEASED;
}
static void click(lv_indev_t *indev, lv_obj_t *object)
{
    lv_obj_update_layout(lv_screen_active());
    lv_obj_update_layout(lv_layer_top());
    lv_area_t area;
    lv_obj_get_coords(object, &area);
    assert(area.x1 >= 0 && area.x2 < 800 && area.y1 >= 0 && area.y2 < 480);
    pointer.x = (area.x1 + area.x2) / 2;
    pointer.y = (area.y1 + area.y2) / 2;
    pressed = true; lv_tick_inc(40); lv_indev_read(indev);
    pressed = false; lv_tick_inc(40); lv_indev_read(indev); lv_timer_handler();
}
static lv_obj_t *find_button(lv_obj_t *root, const char *text)
{
    for (uint32_t i = 0; i < lv_obj_get_child_count(root); i++) {
        lv_obj_t *child = lv_obj_get_child(root, i);
        if (!lv_obj_check_type(child, &lv_button_class)) continue;
        lv_obj_t *label = lv_obj_get_child(child, 0);
        if (label && !strcmp(lv_label_get_text(label), text)) return child;
    }
    return NULL;
}
static void check_page(lv_obj_t *screen, lv_obj_t *nav, uint32_t first, int page)
{
    if (lv_aic_manual_page_current() != page)
        fprintf(stderr, "page expected=%d actual=%d pointer=%d,%d\n",
                page, lv_aic_manual_page_current(), (int)pointer.x, (int)pointer.y);
    assert(lv_aic_manual_page_current() == page);
    for (int i = 0; i < 3; i++)
        assert(lv_obj_is_hidden(lv_obj_get_child(screen, first + i)) == (i != page));
    char position[16];
    snprintf(position, sizeof(position), "%d / 3", page + 1);
    assert(!strcmp(lv_label_get_text(lv_obj_get_child(nav, 1)), position));
    assert(!lv_obj_is_hidden(nav));
}
int main(int argc, char **argv)
{
#if !LV_USE_FREETYPE && !defined(AIC_MANUAL_PREVIEW)
    (void)argc; (void)argv;
#endif
    lv_init();
#ifdef AIC_MANUAL_PREVIEW
    assert(argc == 5);
    preview_fs(argv[1]);
    const char *latin = argv[3], *cjk = argv[4];
#elif LV_USE_FREETYPE
    assert(argc == 3);
    const char *latin = argv[1], *cjk = argv[2];
#endif
    lv_display_t *display = lv_display_create(800, 480);
    assert(display);
    lv_display_set_color_format(display, LV_COLOR_FORMAT_RGB565);
    lv_display_set_buffers(display, pixels, NULL, sizeof(pixels), LV_DISPLAY_RENDER_MODE_FULL);
    lv_display_set_flush_cb(display, flush);
    lv_indev_t *indev = lv_indev_create();
    assert(indev);
    lv_indev_set_type(indev, LV_INDEV_TYPE_POINTER);
    lv_indev_set_read_cb(indev, read_pointer);
    lv_indev_set_display(indev, display);
    lv_obj_t *screen = lv_screen_active();
    uint32_t initial = lv_obj_get_child_count(screen);
    uint32_t top_initial = lv_obj_get_child_count(lv_layer_top());
    assert(lv_aic_manual_page_current() == -1);
    lv_aic_manual_page_request(2); /* Pre-create requests cannot leak into a later UI. */
    for (int cycle = 0; cycle < 3; cycle++) {
        assert(lv_aic_manual_test_create() == LV_AIC_OK);
        assert(lv_aic_manual_test_create() == LV_AIC_ERR_INVALID_STATE);
        assert(lv_aic_manual_page_count() == 3);
        assert(lv_obj_get_child_count(screen) == initial + 3);
        lv_obj_t *baseline = lv_obj_get_child(screen, initial);
        lv_obj_t *nav = lv_obj_get_child(lv_layer_top(), top_initial);
        lv_obj_t *next = find_button(nav, "Next >");
        lv_obj_t *prev = find_button(nav, "< Prev");
        lv_obj_t *touch = find_button(baseline, "Test touch");
        assert(next && prev && touch);
        lv_obj_update_layout(screen);
        lv_obj_update_layout(lv_layer_top());
        lv_area_t a, b;
        lv_obj_get_coords(prev, &a); lv_obj_get_coords(next, &b);
        assert(a.x2 < b.x1 && a.y1 >= 0 && b.y2 < 64);
        check_page(screen, nav, initial, 0);
        /* Actual pointer hit testing catches overlapping buttons, unlike sending
         * CLICKED directly to objects that may be hidden behind another object. */
        for (int i = 1; i <= 12; i++) { click(indev, next); check_page(screen, nav, initial, i % 3); }
        for (int i = 1; i <= 12; i++) { click(indev, prev); check_page(screen, nav, initial, (3 - i % 3) % 3); }
        lv_aic_manual_page_request(-1); lv_aic_manual_page_request(99);
        lv_aic_manual_page_poll(); check_page(screen, nav, initial, 0);
        for (int page = 0; page < 3; page++) {
            lv_aic_manual_page_request(page); lv_aic_manual_page_poll();
            check_page(screen, nav, initial, page);
            lv_refr_now(display);
#ifdef AIC_MANUAL_PREVIEW
            if (!cycle) preview_save(argv[2], page, pixels);
#endif
        }
        lv_aic_manual_page_request(0); lv_aic_manual_page_poll();
        click(indev, touch);
        assert(!strcmp(lv_aic_manual_test_status_text(), "button event received"));
#if LV_USE_FREETYPE
        assert(lv_aic_font_test_create(latin, cjk) == LV_AIC_OK);
        lv_obj_t *open = find_button(lv_layer_top(), "Fonts");
        lv_obj_t *overlay = lv_obj_get_child(lv_layer_top(), top_initial + 2);
        lv_obj_t *panel = lv_obj_get_child(overlay, 0);
        lv_obj_t *close = find_button(panel, "Close");
        assert(open && close);
        lv_obj_update_layout(lv_layer_top());
        lv_obj_get_coords(open, &a); lv_obj_get_coords(next, &b);
        assert(b.x2 < a.x1);
        lv_aic_manual_page_request(1); lv_aic_manual_page_poll();
        for (int i = 0; i < 20; i++) {
            click(indev, open);
            assert(!lv_obj_is_hidden(overlay));
            click(indev, next); check_page(screen, nav, initial, 1);
            click(indev, prev); check_page(screen, nav, initial, 1);
            if (!cycle && !i) {
                lv_refr_now(display);
#ifdef AIC_MANUAL_PREVIEW
                preview_save(argv[2], 3, pixels);
#endif
            }
            click(indev, close);
            assert(lv_obj_is_hidden(overlay));
            check_page(screen, nav, initial, 1);
        }
        lv_aic_font_test_delete();
        assert(lv_obj_get_child_count(lv_layer_top()) == top_initial + 1);
        click(indev, next); check_page(screen, nav, initial, 2);
#else
        assert(lv_obj_get_child_count(lv_layer_top()) == top_initial + 1);
#endif
        lv_aic_manual_page_request(2); /* A pending command is cleared on teardown. */
        lv_aic_manual_test_deinit();
        lv_aic_manual_test_deinit();
        assert(lv_obj_get_child_count(screen) == initial);
        assert(lv_obj_get_child_count(lv_layer_top()) == top_initial);
        assert(lv_aic_manual_page_current() == -1);
    }
    lv_indev_delete(indev);
    lv_display_delete(display);
    lv_deinit();
    puts("PASS manual navigation: real pointer, both directions, wrap, page state and teardown");
    return 0;
}
