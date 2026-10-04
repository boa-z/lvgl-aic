/* SPDX-License-Identifier: Apache-2.0 */
#include "lvgl.h"
#include "lvgl_private.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
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
    /* A centered layer mask must scale premultiplied RGB as well as alpha.
     * Compare both encodings against straight alpha and an independent blend. */
    uint8_t masks[8*8];
    static const uint8_t coverage[4]={0,64,128,255};
    for(unsigned i=0;i<sizeof masks;i++) masks[i]=coverage[i%4];
    lv_image_dsc_t mask={.header={.magic=LV_IMAGE_HEADER_MAGIC,.w=8,.h=8,.stride=8,.cf=LV_COLOR_FORMAT_A8},
        .data=masks,.data_size=sizeof masks};
    for(unsigned encoding=0;encoding<3;encoding++) {
        lv_canvas_fill_bg(canvas,lv_color_hex(0x103050),LV_OPA_COVER);
        lv_layer_t parent;lv_canvas_init_layer(canvas,&parent);
        lv_area_t area={20,20,35,35};
        lv_layer_t *child=lv_draw_layer_create(&parent,
            encoding==1?LV_COLOR_FORMAT_ARGB8888_PREMULTIPLIED:LV_COLOR_FORMAT_ARGB8888,&area);
        assert(child && lv_draw_layer_alloc_buf(child));
        if(encoding==2) child->draw_buf->header.flags|=LV_IMAGE_FLAGS_PREMULTIPLIED;
        for(unsigned y=0;y<16;y++) {
            uint32_t *row=(uint32_t *)(child->draw_buf->data+y*child->draw_buf->header.stride);
            for(unsigned x=0;x<16;x++) row[x]=encoding?0x80800000:0x80ff0000;
        }
        lv_draw_image_dsc_t d;lv_draw_image_dsc_init(&d);d.src=child;
        d.bitmap_mask_src=&mask;d.image_area=area;
        lv_draw_layer(&parent,&d,&area);lv_canvas_finish_layer(canvas,&parent);
        for(unsigned y=20;y<36;y++) for(unsigned x=20;x<36;x++) {
            unsigned a=x>=24 && x<32 && y>=24 && y<32?coverage[(x-24)%4]*128/255:0;
            unsigned r=(255*a+16*(255-a))/255,g=48*(255-a)/255,b=80*(255-a)/255;
            lv_color32_t px=lv_canvas_get_px(canvas,x,y);
            assert(abs((int)px.red-(int)r)<=2 && abs((int)px.green-(int)g)<=2 && abs((int)px.blue-(int)b)<=2);
        }
    }
    lv_image_cache_drop(&mask);
    lv_obj_delete(canvas);lv_draw_buf_destroy(buf);lv_display_delete(disp);lv_deinit();
    puts("PASS premultiplied format/flag parity: native, opacity, rounded clip, recolor and transformed pixels");
    return 0;
}
