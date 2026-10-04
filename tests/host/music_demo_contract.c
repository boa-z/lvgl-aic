/* SPDX-License-Identifier: Apache-2.0 */
#include "lvgl.h"
#include "lv_demos.h"
#include "music/lv_demo_music_main.h"
#include <assert.h>
#include <stdio.h>

static uint32_t pixels[320*480];
static unsigned flushes;
static void flush(lv_display_t *display,const lv_area_t *area,uint8_t *data)
{ (void)area;(void)data;flushes++;lv_display_flush_ready(display); }
static void advance(unsigned milliseconds)
{
    for(unsigned elapsed=0;elapsed<milliseconds;elapsed+=50) {
        lv_tick_inc(50);lv_timer_handler();
    }
}
static uint32_t checksum(void)
{
    uint32_t hash=2166136261U;
    for(unsigned i=0;i<320*480;i++) hash=(hash^pixels[i])*16777619U;
    return hash;
}
int main(void)
{
    lv_init();
    lv_display_t *display=lv_display_create(320,480);assert(display);
    lv_display_set_color_format(display,LV_COLOR_FORMAT_ARGB8888);
    lv_display_set_buffers(display,pixels,NULL,sizeof(pixels),LV_DISPLAY_RENDER_MODE_DIRECT);
    lv_display_set_flush_cb(display,flush);
    lv_demo_music();advance(4000);
    assert(lv_obj_get_child_count(lv_screen_active())>0 && flushes>0);
    bool varied=false;
    for(unsigned i=1;i<320*480;i++) if(pixels[i]!=pixels[0]) { varied=true;break; }
    assert(varied);
    uint32_t idle=checksum();unsigned before=flushes;
    lv_demo_music_play(1);advance(1600);
    assert(flushes>before && checksum()!=idle);
    lv_demo_music_pause();advance(300);
    uint32_t paused=checksum();
    lv_demo_music_album_next(true);advance(800);
    assert(checksum()!=paused);
    lv_demo_music_resume();advance(700);lv_demo_music_pause();
    lv_obj_clean(lv_screen_active());advance(1000);
    assert(lv_obj_get_child_count(lv_screen_active())==0);
    lv_deinit();
    puts("PASS music UI render, track animation, pause/resume and cleanup; no audio");
    return 0;
}
