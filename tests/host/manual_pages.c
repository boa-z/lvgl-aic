/* SPDX-License-Identifier: Apache-2.0 */
#include "lv_aic_manual_test.h"
#include <assert.h>
#include <string.h>

static lv_obj_t *find_button(lv_obj_t *root, const char *text)
{
    for (uint32_t i = 0; i < lv_obj_get_child_count(root); i++) {
        lv_obj_t *child = lv_obj_get_child(root, i);
        if (!lv_obj_check_type(child, &lv_button_class)) continue;
        lv_obj_t *label = lv_obj_get_child(child, 0);
        if (label && strcmp(lv_label_get_text(label), text) == 0) return child;
    }
    return NULL;
}

int main(void)
{
    lv_init();
    lv_display_t *display = lv_display_create(800, 480);
    assert(display);
    lv_obj_t *screen = lv_screen_active();
    uint32_t initial = lv_obj_get_child_count(screen);
    for (int cycle = 0; cycle < 3; cycle++) {
        assert(lv_aic_manual_test_create() == LV_AIC_OK);
        assert(lv_obj_get_child_count(screen) == initial + 3);
        lv_obj_t *baseline = lv_obj_get_child(screen, initial);
        lv_obj_t *rotation = lv_obj_get_child(screen, initial + 1);
        lv_obj_t *combo = lv_obj_get_child(screen, initial + 2);
        lv_obj_t *next = find_button(baseline, "Next >  (1/3)");
        lv_obj_t *prev = find_button(rotation, "< Prev  (2/3)");
        lv_obj_t *combo_prev = find_button(combo, "< Prev  (3/3)");
        assert(next && prev && combo_prev);
        lv_obj_update_layout(screen);
        lv_area_t area;
        lv_obj_get_coords(next, &area);
        assert(area.x1 >= 590 && area.x2 < 800 && area.y1 >= 0 && area.y2 < 58);
        assert(!lv_obj_is_hidden(baseline));
        assert(lv_obj_is_hidden(rotation));
        lv_obj_send_event(next, LV_EVENT_CLICKED, NULL);
        assert(lv_obj_is_hidden(baseline));
        assert(!lv_obj_is_hidden(rotation));
        lv_obj_send_event(prev, LV_EVENT_CLICKED, NULL);
        assert(!lv_obj_is_hidden(baseline));
        assert(lv_obj_is_hidden(rotation));
        lv_aic_manual_page_request(1);
        lv_aic_manual_page_poll();
        assert(!lv_obj_is_hidden(rotation));
        lv_aic_manual_page_request(99);
        lv_aic_manual_page_poll();
        assert(!lv_obj_is_hidden(rotation));
        lv_aic_manual_test_deinit();
        assert(lv_obj_get_child_count(screen) == initial);
    }
    lv_display_delete(display);
    lv_deinit();
    return 0;
}
