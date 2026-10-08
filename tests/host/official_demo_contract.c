/* SPDX-License-Identifier: Apache-2.0 */
/* Host contract for the official SDK demo runner: every registered demo,
 * compiled from the unmodified SDK source, starts on its own screen at the
 * 1024x600 design size, keeps running on virtual time, and closes cleanly:
 * exactly its timers are removed, the previous screen and the hidden
 * top-layer overlay come back, and it can be shown again. Asset files are
 * absent on host (image sources fail to open), so pixels are not checked. */
#include "lvgl.h"
#include "lvgl_private.h" /* lv_display_t screen_cnt: screen leak check */
#include "demos/official/lv_aic_official_demo.h"

#include <assert.h>
#include <string.h>

static uint32_t pixels[1024 * 600];
static unsigned flushes;

static void flush(lv_display_t *display, const lv_area_t *area, uint8_t *data)
{
    (void)area;
    (void)data;
    flushes++;
    lv_display_flush_ready(display);
}

static uint32_t timer_total(void)
{
    uint32_t n = 0;
    for (lv_timer_t *t = lv_timer_get_next(NULL); t != NULL; t = lv_timer_get_next(t)) {
        n++;
    }
    return n;
}

static void run_ms(uint32_t ms)
{
    for (uint32_t i = 0; i < ms / 10U; i++) {
        lv_tick_inc(10);
        lv_timer_handler();
    }
}

int main(void)
{
    lv_display_t *display;
    lv_obj_t *home, *nav;
    uint32_t baseline, screens, anims;

    lv_init();
    display = lv_display_create(1024, 600);
    assert(display);
    lv_display_set_color_format(display, LV_COLOR_FORMAT_ARGB8888);
    lv_display_set_buffers(display, pixels, NULL, sizeof(pixels), LV_DISPLAY_RENDER_MODE_DIRECT);
    lv_display_set_flush_cb(display, flush);

    home = lv_screen_active();
    nav = lv_obj_create(lv_layer_top()); /* stands in for the smoke nav bar */
    run_ms(50);
    baseline = timer_total();
    anims = lv_anim_count_running();

    screens = display->screen_cnt;
    assert(lv_aic_official_demo_count() == 6);
    assert(lv_aic_official_demo_find("meter") && lv_aic_official_demo_find("dashboard") &&
           lv_aic_official_demo_find("slide") && lv_aic_official_demo_find("multi_lang") &&
           lv_aic_official_demo_find("demo_hub") && lv_aic_official_demo_find("image"));
    assert(lv_aic_official_demo_find("nope") == NULL);
    assert(lv_aic_official_demo_show("nope") == -1);
    assert(lv_aic_official_demo_active() == NULL);
    lv_aic_official_demo_close(); /* no-op when idle */

    for (size_t i = 0; i < lv_aic_official_demo_count(); i++) {
        const char *name = lv_aic_official_demo_get(i)->name;
        for (int round = 0; round < 2; round++) {
            unsigned before = flushes;
            assert(lv_aic_official_demo_show(name) == 0);
            assert(strcmp(lv_aic_official_demo_active(), name) == 0);
            assert(lv_screen_active() != home);
            assert(lv_obj_is_hidden(nav));
            /* The animated clusters own timers; slide and multi_lang none. */
            if (strcmp(name, "meter") == 0 || strcmp(name, "dashboard") == 0) {
                assert(lv_aic_official_demo_timer_count() > 0);
            } else if (strcmp(name, "slide") == 0 || strcmp(name, "multi_lang") == 0 ||
                       strcmp(name, "image") == 0) {
                assert(lv_aic_official_demo_timer_count() == 0);
            }
            assert(timer_total() == baseline + lv_aic_official_demo_timer_count());
            run_ms(3000);
            assert(flushes > before);
            if (strcmp(name, "image") == 0) {
                /* The lyric canvases chain animations from deleted_cb. */
                assert(lv_anim_count_running() > anims);
            }

            lv_aic_official_demo_close();
            assert(lv_aic_official_demo_active() == NULL);
            assert(lv_screen_active() == home);
            assert(!lv_obj_is_hidden(nav));
            assert(timer_total() == baseline);
            /* Runner screen and any screen the demo loaded itself are gone. */
            assert(display->screen_cnt == screens);
            /* No animation survives on a deleted object, including ones a
             * demo would restart from its deleted_cb during teardown. */
            assert(lv_anim_count_running() == anims);
            run_ms(100); /* no demo callback may fire after close */
        }
    }

    /* Switching directly closes the previous demo first. */
    assert(lv_aic_official_demo_show("meter") == 0);
    assert(lv_aic_official_demo_show("dashboard") == 0);
    assert(strcmp(lv_aic_official_demo_active(), "dashboard") == 0);
    assert(timer_total() == baseline + lv_aic_official_demo_timer_count());
    lv_aic_official_demo_close();
    assert(timer_total() == baseline && lv_screen_active() == home);
    return 0;
}
