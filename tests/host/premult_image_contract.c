/* SPDX-License-Identifier: Apache-2.0 */
#include "lvgl.h"
#include "lvgl_private.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
static uint32_t pixels[16*16], reference[64*64];
static void flush(lv_display_t *d,const lv_area_t *a,uint8_t *p)
{ (void)a;(void)p;lv_display_flush_ready(d); }
int main(void)
{
    lv_init();lv_display_t *disp=lv_display_create(64,64);assert(disp);
    lv_display_set_flush_cb(disp,flush);
    lv_obj_t *canvas=lv_canvas_create(lv_screen_active());
    lv_draw_buf_t *buf=lv_draw_buf_create(64,64,LV_COLOR_FORMAT_ARGB8888,0);assert(buf);
    lv_canvas_set_draw_buf(canvas,buf);assert(buf->header.stride==64*4);
    for(unsigned y=0;y<16;y++) for(unsigned x=0;x<16;x++) {
        unsigned a=(x<8?128:200),r=a,g=y*a/30;
        pixels[y*16+x]=(a<<24)|(r<<16)|(g<<8);
    }
    pixels[0]=0; /* Transparent storage must not contribute colour. */
    for(unsigned scene=0;scene<9;scene++) for(unsigned flags=0;flags<2;flags++) {
        lv_image_dsc_t src={.header={.magic=LV_IMAGE_HEADER_MAGIC,.w=16,.h=16,.stride=64,
            .cf=flags?LV_COLOR_FORMAT_ARGB8888:LV_COLOR_FORMAT_ARGB8888_PREMULTIPLIED,
            .flags=flags?LV_IMAGE_FLAGS_PREMULTIPLIED:0},.data=(const uint8_t *)pixels,.data_size=sizeof pixels};
        lv_canvas_fill_bg(canvas,lv_color_hex(0x103050),LV_OPA_COVER);
        lv_layer_t layer;lv_canvas_init_layer(canvas,&layer);
        lv_draw_image_dsc_t d;lv_draw_image_dsc_init(&d);d.src=&src;
        d.opa=scene&1?128:255;d.pivot=(lv_point_t){8,8};
        if(scene==2 || scene==3) d.clip_radius=5;
        if(scene==4 || scene==5 || scene==7) { d.recolor=lv_color_hex(0x2050e0);d.recolor_opa=100; }
        lv_image_colorkey_t key={.low=lv_color_hex(0xff0000),.high=lv_color_hex(0xff0000)};
        if(scene==8) d.colorkey=&key;
        if(scene==6 || scene==7) { d.rotation=175;d.scale_x=384;d.scale_y=512; }
        lv_area_t coords={20,20,35,35};
        lv_draw_image(&layer,&d,&coords);lv_canvas_finish_layer(canvas,&layer);
        if(!flags) memcpy(reference,buf->data,sizeof reference);
        else assert(!memcmp(reference,buf->data,sizeof reference));
        lv_color32_t outside=lv_canvas_get_px(canvas,0,0);
        assert(outside.red==16 && outside.green==48 && outside.blue==80);
        if(scene==0) {
            lv_color32_t inside=lv_canvas_get_px(canvas,24,24);
            assert(inside.red>=135 && inside.red<=137); /* 128 + 16 * (1 - 128/255) */
        }
        lv_image_cache_drop(&src);
    }
    lv_obj_delete(canvas);lv_draw_buf_destroy(buf);lv_display_delete(disp);lv_deinit();
    puts("PASS premultiplied format/flag parity: native, opacity, rounded clip, recolor and transformed pixels");
    return 0;
}
