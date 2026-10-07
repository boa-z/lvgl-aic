/* SPDX-License-Identifier: Apache-2.0 */
/* Host contract for the application-owned meter cluster demo.
 * Proves widget creation, timer-driven state (needle sweep, speed digit)
 * and varied software rendering on virtual time. Asset files, GE
 * acceleration and board timing are explicitly out of scope here. */
#include "lvgl.h"
#include "demos/meter_demo/meter_ui.h"

#include <assert.h>
#include <stdio.h>
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

static int fps_sixty(void)
{
    return 60;
}

int main(void)
{
    meter_ui_config_t config;
    int32_t angle_before;
    int speed_before;
    uint32_t fires_before;
    bool varied = false;
    size_t i;

    memset(&config, 0, sizeof(config));
    config.asset_root = NULL; /* built-in fallback: no filesystem on host */
    config.fps = fps_sixty;

    lv_init();
    {
        lv_display_t *display = lv_display_create(1024, 600);
        assert(display);
        lv_display_set_color_format(display, LV_COLOR_FORMAT_ARGB8888);
        lv_display_set_buffers(display, pixels, NULL, sizeof(pixels),
                               LV_DISPLAY_RENDER_MODE_DIRECT);
        lv_display_set_flush_cb(display, flush);
    }

    meter_ui_configure(&config);
    meter_ui_init();
    assert(meter_ui_timer_fires() == 0);
    angle_before = meter_ui_get_needle_angle();
    speed_before = meter_ui_get_speed_step();
    fires_before = meter_ui_timer_fires();

    /* 2.5 s of virtual time: point, speed and fps timers must all fire;
     * the needle sweep and speed digit must move. 25 steps avoids an exact
     * multiple of the 10-step speed cycle. */
    for (i = 0; i < 25; i++) {
        lv_tick_inc(100);
        lv_timer_handler();
    }
    assert(meter_ui_timer_fires() > fires_before);
    assert(meter_ui_get_needle_angle() != angle_before);
    assert(meter_ui_get_speed_step() != speed_before);
    assert(flushes > 0);
    for (i = 1; i < 1024u * 600u; i++) {
        if (pixels[i] != pixels[0]) {
            varied = true;
            break;
        }
    }
    assert(varied);

    /* Re-entry is a no-op while created; destroy tears everything down. */
    meter_ui_init();
    meter_ui_destroy();
    assert(meter_ui_get_speed_step() == -1);
    for (i = 0; i < 4; i++) {
        lv_tick_inc(100);
        lv_timer_handler();
    }
    lv_deinit();
    puts("PASS meter demo lifecycle/timer/render (software, virtual time)");
    return 0;
}
