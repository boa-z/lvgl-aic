/* SPDX-License-Identifier: Apache-2.0 */
#include "lvgl.h"
#include "lv_demos.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

static uint32_t pixels[320*240];
static unsigned flushes;
static bool finished;
static void flush(lv_display_t *display,const lv_area_t *area,uint8_t *data)
{
    (void)area;(void)data;flushes++;lv_display_flush_ready(display);
}
static void benchmark_done(const lv_demo_benchmark_summary_t *summary)
{
    assert(summary && summary->scenes);
    unsigned scenes=0,measured=0;
    while(summary->scenes[scenes].create_cb) {
        if(summary->scenes[scenes].measurement_cnt) measured++;
        scenes++;
    }
    assert(scenes==16 && measured==16);
    finished=true;
}
int main(int argc,char **argv)
{
    assert(argc==2);
    lv_init();
    lv_display_t *display=lv_display_create(320,240);assert(display);
    lv_display_set_color_format(display,LV_COLOR_FORMAT_ARGB8888);
    lv_display_set_buffers(display,pixels,NULL,sizeof(pixels),LV_DISPLAY_RENDER_MODE_DIRECT);
    lv_display_set_flush_cb(display,flush);
    if(!strcmp(argv[1],"widgets")) {
        lv_demo_widgets();
        for(unsigned i=0;i<12;i++) { lv_tick_inc(100);lv_timer_handler(); }
        assert(lv_obj_get_child_count(lv_screen_active())>0 && flushes>0);
        bool varied=false;
        for(unsigned i=1;i<320*240;i++) if(pixels[i]!=pixels[0]) { varied=true;break; }
        assert(varied);
        lv_obj_clean(lv_screen_active());
        for(unsigned i=0;i<4;i++) { lv_tick_inc(100);lv_timer_handler(); }
    }
    else {
        assert(!strcmp(argv[1],"benchmark"));
        lv_demo_benchmark_set_end_cb(benchmark_done);
        lv_demo_benchmark();
        /* Advance virtual time through every real upstream scene. This proves
         * rendering/lifecycle integration, never hardware speed or FPS. */
        for(unsigned i=0;i<400 && !finished;i++) { lv_tick_inc(250);lv_timer_handler(); }
        assert(finished && flushes>20);
    }
    lv_deinit();
    puts("PASS upstream demo render/scene lifecycle (software, virtual time)");
    return 0;
}
