/* SPDX-License-Identifier: Apache-2.0 */
#include <assert.h>
#include "lv_swipe_v1.h"
static unsigned changed;
static void advance(void)
{
    for (int i = 0; i < 70; i++) { lv_tick_inc(20); lv_timer_handler(); }
}
static void count_change(lv_event_t *e) { (void)e; changed++; }
static void destroy(lv_event_t *e) { lv_obj_delete(lv_event_get_target_obj(e)); }
static void replace_and_restart(lv_event_t *e)
{
    lv_obj_t *s=lv_event_get_target_obj(e);
    lv_obj_remove_event_cb(s,replace_and_restart);
    lv_obj_delete(lv_swipe_v1_get_child(s,0));
    lv_swipe_v1_child_add_state_src(s,0,(void *)LV_SYMBOL_OK,(void *)LV_SYMBOL_CLOSE);
    lv_swipe_v1_set_next(s,LV_ANIM_ON);
}
static lv_obj_t *create(void)
{
    lv_obj_t *s = lv_swipe_v1_create(lv_screen_active());
    assert(s);
    for (int i = 0; i < 4; i++) {
        lv_swipe_v1_child_add_state_src(s, i * 10, (void *)LV_SYMBOL_OK, (void *)LV_SYMBOL_CLOSE);
        lv_swipe_v1_child_add_state_src(s, i * 10, (void *)LV_SYMBOL_PLUS, (void *)LV_SYMBOL_MINUS);
        lv_obj_t *c = lv_swipe_v1_get_child(s, i * 10);
        assert(c);
        lv_obj_set_pos(c, i * 30, i * 10);
    }
    lv_obj_update_layout(s);
    return s;
}
int main(void)
{
    lv_init();
    lv_display_t *d = lv_display_create(320, 240);
    assert(d);
    for (int cycle = 0; cycle < 20; cycle++) {
        lv_obj_t *s = create();
        lv_obj_add_event_cb(s, count_change, LV_EVENT_VALUE_CHANGED, NULL);
        assert(lv_swipe_v1_get_active_child(s) == 30);
        lv_swipe_v1_child_add_state_src(s, 50, (void *)LV_SYMBOL_OK, (void *)LV_SYMBOL_CLOSE);
        assert(lv_swipe_v1_get_child_count(s) == 4);
        lv_swipe_v1_set_child_active_src(s, 30, 2);
        assert(lv_swipe_v1_get_child_active_src(s, 30) == 0);
        lv_swipe_v1_toggle_child_active(s);
        assert(lv_swipe_v1_get_child_active_src(s, 30) == 1);
        unsigned before = changed;
        lv_swipe_v1_set_next(s, LV_ANIM_OFF);
        assert(changed == before + 1);
        assert(lv_swipe_v1_get_active_child(s) == 20);
        lv_obj_update_layout(s);
        assert(lv_obj_get_x(lv_swipe_v1_get_child(s, 20)) == 90);
        lv_swipe_v1_set_prev(s, LV_ANIM_OFF);
        assert(lv_swipe_v1_get_active_child(s) == 30);
        lv_swipe_v1_set_anim_params(s, 100, NULL);
        lv_swipe_v1_set_next(s, LV_ANIM_ON);
        lv_swipe_v1_set_next(s, LV_ANIM_ON);
        assert(lv_swipe_v1_get_active_child(s) == 20);
        advance();
        assert(changed == before + 3);
        lv_swipe_v1_set_next(s, LV_ANIM_ON);
        lv_obj_delete(lv_swipe_v1_get_child(s, 0));
        assert(lv_swipe_v1_get_child_count(s) == 3);
        advance();
        assert(changed == before + 3);
        lv_obj_delete(s);
        advance();
    }
    lv_event_code_t codes[] = {LV_EVENT_SCROLL_BEGIN, LV_EVENT_SCROLL_END, LV_EVENT_VALUE_CHANGED};
    for (unsigned i = 0; i < 3; i++) {
        for (int animate = 0; animate < 2; animate++) {
            lv_obj_t *s = create();
            lv_obj_add_event_cb(s, destroy, codes[i], NULL);
            lv_swipe_v1_set_next(s, animate ? LV_ANIM_ON : LV_ANIM_OFF);
            advance();
        }
    }
    for(unsigned event=0;event<2;event++) {
        lv_obj_t *widget=create();
        unsigned before=changed;
        lv_obj_add_event_cb(widget,count_change,LV_EVENT_VALUE_CHANGED,NULL);
        lv_obj_add_event_cb(widget,replace_and_restart,
            event?LV_EVENT_SCROLL_END:LV_EVENT_SCROLL_BEGIN,NULL);
        lv_swipe_v1_set_next(widget,LV_ANIM_ON);
        advance();advance();
        assert(changed==before+1); /* Only replacement transition completes. */
        lv_obj_delete(widget);
    }
    lv_obj_t *s = create();
    lv_swipe_v1_set_next(s, LV_ANIM_ON);
    lv_obj_delete(s);
    advance();
    lv_display_delete(d);
    lv_deinit();
    return 0;
}
