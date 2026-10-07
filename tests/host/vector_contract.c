/* SPDX-License-Identifier: Apache-2.0 */
#include "lvgl.h"
#include "lvgl_private.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
static void flush(lv_display_t *d,const lv_area_t *a,uint8_t *pixels)
{ (void)a;(void)pixels;lv_display_flush_ready(d); }
static void scene(lv_obj_t *canvas,unsigned kind)
{
    lv_canvas_fill_bg(canvas,lv_color_black(),LV_OPA_COVER);
    lv_layer_t layer;lv_canvas_init_layer(canvas,&layer);
    if(kind==3) layer._clip_area=(lv_area_t){16,16,47,47};
    lv_draw_vector_dsc_t *d=lv_draw_vector_dsc_create(&layer);assert(d);
    lv_vector_path_t *p=lv_vector_path_create(LV_VECTOR_PATH_QUALITY_HIGH);assert(p);
    lv_draw_vector_dsc_set_fill_color(d,lv_color_hex(0xff0000));
    if(kind==0) {
        lv_fpoint_t points[]={{8,8},{56,8},{8,56}};
        lv_vector_path_move_to(p,&points[0]);lv_vector_path_line_to(p,&points[1]);
        lv_vector_path_line_to(p,&points[2]);lv_vector_path_close(p);
    } else if(kind==1) {
        lv_vector_path_append_rectangle(p,8,8,48,48,0,0);
        lv_vector_path_append_rectangle(p,24,24,16,16,0,0);
        lv_draw_vector_dsc_set_fill_rule(d,LV_VECTOR_FILL_EVENODD);
        lv_draw_vector_dsc_set_fill_opa(d,128);
    } else if(kind==2) {
        lv_vector_path_append_rectangle(p,8,8,48,48,0,0);
        lv_grad_stop_t stops[2]={{.color=lv_color_hex(0xff0000),.opa=255,.frac=0},
                               {.color=lv_color_hex(0x0000ff),.opa=255,.frac=255}};
        lv_draw_vector_dsc_set_fill_linear_gradient(d,8,8,56,8);
        lv_draw_vector_dsc_set_fill_gradient_color_stops(d,stops,2);
    } else {
        lv_vector_path_append_rectangle(p,0,0,48,48,0,0);
        lv_draw_vector_dsc_translate(d,8,8);
        lv_draw_vector_dsc_set_fill_color(d,lv_color_hex(0x00ff00));
    }
    lv_draw_vector_dsc_add_path(d,p);lv_draw_vector(d);
    lv_vector_path_delete(p);lv_draw_vector_dsc_delete(d);
    lv_canvas_finish_layer(canvas,&layer);
    lv_color32_t inside=lv_canvas_get_px(canvas,12,12);
    lv_color32_t center=lv_canvas_get_px(canvas,32,32);
    lv_color32_t outside=lv_canvas_get_px(canvas,60,60);
    assert(outside.red==0 && outside.green==0 && outside.blue==0);
    if(kind==0) {
        /* LVGL opacity multiplication may round full coverage down by one. */
        assert(inside.red>=254 && inside.green==0 && inside.blue==0);
        lv_color32_t empty=lv_canvas_get_px(canvas,48,48);assert(!empty.red);
    } else if(kind==1) {
        assert(inside.red>=126 && inside.red<=129 && !inside.green && !inside.blue);
        assert(!center.red && !center.green && !center.blue);
    } else if(kind==2) {
        lv_color32_t right=lv_canvas_get_px(canvas,52,12);
        assert(inside.red>210 && inside.blue<35 && !inside.green);
        assert(right.red<35 && right.blue>210 && !right.green);
        assert(center.red>110 && center.red<145 && center.blue>110 && center.blue<145);
    } else {
        assert(!inside.red && !inside.green && !inside.blue);
        assert(!center.red && center.green>=254 && !center.blue);
        for(unsigned y=0;y<64;y++) for(unsigned x=0;x<64;x++) {
            lv_color32_t c=lv_canvas_get_px(canvas,x,y);
            if(x<16 || x>47 || y<16 || y>47) assert(!c.red && !c.green && !c.blue);
        }
    }
}
int main(void)
{
    lv_init();
    char formatted[24];
    assert(lv_snprintf(formatted,sizeof formatted,"%.2f %.1f",1.25,-2.5)==9);
    assert(!strcmp(formatted,"1.25 -2.5"));
    lv_display_t *display=lv_display_create(64,64);assert(display);
    uint32_t pixels[64*64];
    lv_display_set_buffers(display,pixels,NULL,sizeof pixels,LV_DISPLAY_RENDER_MODE_DIRECT);
    lv_display_set_flush_cb(display,flush);
    for(unsigned n=0;n<20;n++) {
        lv_obj_t *canvas=lv_canvas_create(lv_screen_active());assert(canvas);
        lv_draw_buf_t *buffer=lv_draw_buf_create(64,64,LV_COLOR_FORMAT_ARGB8888,0);assert(buffer);
        lv_canvas_set_draw_buf(canvas,buffer);
        for(unsigned kind=0;kind<4;kind++) scene(canvas,kind);
        lv_obj_delete(canvas);lv_draw_buf_destroy(buffer);
    }
    lv_display_delete(display);lv_deinit();
    puts("PASS ThorVG vector pixels: paths, fill rules, opacity, gradients, transformed clipping and lifecycle");
    return 0;
}
