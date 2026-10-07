/* SPDX-License-Identifier: Apache-2.0 */
#define AIC_LVGL_USE_PRIVATE_API 1
#define LOG_TAG "lvgl.video.window"
#define LOG_LVL LOG_LVL_INFO
#include "lv_aic_manual_test.h"
#if AIC_LVGL_USE_VIDEO_WINDOW && AIC_LVGL_USE_GE2D && AIC_LVGL_BSP_RTTHREAD
#include "lvgl_aic_private.h"
#include "lv_aic_video_window.h"
#include "lv_draw_aic_ge2d.h"
#include "lv_aic_test_log.h"
#include "../common/lv_aic_video_window_cases.h"
static void probe_flush(lv_display_t *display,const lv_area_t *area,uint8_t *pixels)
{(void)area;(void)pixels;lv_display_flush_ready(display);}
static bool probe_pixels(lv_display_t *display,const lv_draw_buf_t *buffer,
                         const lv_area_t *hole,unsigned scene)
{
    lv_refr_now(display);
    for(int y=0;y<64;y++) for(int x=0;x<64;x++) {
        uint32_t value;lv_memcpy(&value,buffer->data+y*buffer->header.stride+x*4,4);
        uint32_t expected=hole && x>=hole->x1 && x<=hole->x2 && y>=hole->y1 && y<=hole->y2?
            0x00123456:0xff204060;
        if(value!=expected) {
            AIC_TEST_E("FAIL window scene=%u x=%d y=%d got=%08x expected=%08x",scene,x,y,(unsigned)value,(unsigned)expected);
            return false;
        }
    }
    AIC_TEST_I("PASS window scene=%u pixels=4096",scene);return true;
}
/* UI-owner only, outside a refresh. The temporary display never drives a panel
 * or changes DE/alpha ownership; it renders through the normal draw units. */
int lv_aic_video_window_test_run(void)
{
    lv_display_t *previous=lv_display_get_default(),*refreshing=lv_refr_get_disp_refreshing();
    lv_draw_buf_t *buffer=lv_draw_buf_create(64,64,LV_COLOR_FORMAT_ARGB8888,256);
    if(!buffer) return -1;
    lv_display_t *display=lv_display_create(64,64);
    if(!display) {lv_draw_buf_destroy(buffer);return -1;}
    int result=-1;lv_display_set_default(display);
    lv_display_set_color_format(display,LV_COLOR_FORMAT_ARGB8888);
    lv_display_set_draw_buffers(display,buffer,NULL);
    lv_display_set_render_mode(display,LV_DISPLAY_RENDER_MODE_DIRECT);
    lv_display_set_flush_cb(display,probe_flush);
    lv_obj_t *screen=lv_display_get_screen_active(display);lv_obj_remove_style_all(screen);
    lv_obj_set_style_bg_color(screen,lv_color_hex(0x204060),0);lv_obj_set_style_bg_opa(screen,255,0);
    lv_obj_t *window=lv_aic_video_window_create(screen);
    if(!window) goto done;
    lv_aic_video_window_set_size(window,16,8);lv_aic_video_window_set_color(window,lv_color_hex(0x123456));
    lv_obj_set_pos(window,32,32);lv_image_set_pivot(window,0,0);
    uint32_t before=lv_draw_aic_ge2d_stats()->image_completed-lv_draw_aic_ge2d_stats()->image_sw_fallback;
    for(unsigned angle=0;angle<4;angle++) {
        lv_image_set_rotation(window,angle*900);
        if(!probe_pixels(display,buffer,&video_window_rotated_bounds[angle],angle)) goto done;
    }
    lv_aic_video_window_set_size(window,8,16);
    for(unsigned angle=0;angle<4;angle++) {
        lv_image_set_rotation(window,angle*900);
        if(!probe_pixels(display,buffer,&video_window_portrait_bounds[angle],angle+4)) goto done;
    }
    lv_aic_video_window_set_size(window,16,8);
    lv_image_set_rotation(window,0);lv_obj_set_pos(window,-4,-2);
    if(!probe_pixels(display,buffer,&(lv_area_t){0,0,11,5},8)) goto done;
    lv_obj_set_hidden(window,true);if(!probe_pixels(display,buffer,NULL,9)) goto done;
    lv_obj_set_hidden(window,false);lv_obj_set_pos(window,20,24);
    if(!probe_pixels(display,buffer,&(lv_area_t){20,24,35,31},10)) goto done;
    lv_obj_delete(window);if(!probe_pixels(display,buffer,NULL,11)) goto done;
    if(lv_draw_aic_ge2d_stats()->image_completed-lv_draw_aic_ge2d_stats()->image_sw_fallback==before) {
        AIC_TEST_E("FAIL window no GE image execution");goto done;
    }
    AIC_TEST_I("PASS 12 video window widget probes; physical scanout remains separate");result=0;
done:
    lv_display_delete(display);lv_draw_buf_destroy(buffer);
    lv_display_set_default(previous);lv_refr_set_disp_refreshing(refreshing);
    return result;
}
#endif
