/* SPDX-License-Identifier: Apache-2.0 */
#include "lvgl.h"
#include "lvgl_private.h"
#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static const char svg[]="<svg width='32' height='32' xmlns='http://www.w3.org/2000/svg'>"
    "<rect width='24' height='32' fill='#ff0000'/>"
    "<rect x='8' width='16' height='32' fill='#0000ff'/>"
    "<rect x='24' width='8' height='32' fill='#00ff00' fill-opacity='0.5'/></svg>";
static bool fail_buffer;
lv_draw_buf_t *__real_lv_draw_buf_create(uint32_t w,uint32_t h,lv_color_format_t cf,uint32_t stride);
lv_draw_buf_t *__wrap_lv_draw_buf_create(uint32_t w,uint32_t h,lv_color_format_t cf,uint32_t stride)
{
    if(fail_buffer) {fail_buffer=false;return NULL;}
    return __real_lv_draw_buf_create(w,h,cf,stride);
}
static void flush(lv_display_t *d,const lv_area_t *a,uint8_t *p)
{(void)a;(void)p;lv_display_flush_ready(d);}
static void expect(lv_obj_t *canvas,int x,int y,unsigned r,unsigned g,unsigned b)
{
    lv_color32_t p=lv_canvas_get_px(canvas,x,y);
    if(abs((int)p.red-(int)r)>4 || abs((int)p.green-(int)g)>4 || abs((int)p.blue-(int)b)>4)
        fprintf(stderr,"pixel %d,%d expected %u,%u,%u got %u,%u,%u\n",x,y,r,g,b,p.red,p.green,p.blue);
    assert(abs((int)p.red-(int)r)<=4 && abs((int)p.green-(int)g)<=4 && abs((int)p.blue-(int)b)<=4);
}
int main(void)
{
    lv_init();lv_display_t *display=lv_display_create(128,128);assert(display);
    lv_display_set_flush_cb(display,flush);
    static uint32_t display_pixels[128*128];
    lv_display_set_color_format(display,LV_COLOR_FORMAT_ARGB8888);
    lv_display_set_buffers(display,display_pixels,NULL,sizeof display_pixels,LV_DISPLAY_RENDER_MODE_DIRECT);
    lv_obj_set_style_bg_color(lv_screen_active(),lv_color_hex(0x102030),0);
    lv_obj_set_style_bg_opa(lv_screen_active(),LV_OPA_COVER,0);
    lv_obj_t *canvas=lv_canvas_create(lv_screen_active());
    lv_draw_buf_t *buf=lv_draw_buf_create(128,128,LV_COLOR_FORMAT_ARGB8888,0);assert(buf);
    lv_canvas_set_draw_buf(canvas,buf);
    lv_image_dsc_t src={.header={.magic=LV_IMAGE_HEADER_MAGIC,.cf=LV_COLOR_FORMAT_RAW,.w=32,.h=32},
        .data=(const uint8_t *)svg,.data_size=sizeof(svg)-1};
    uint8_t mask_pixels[32*32];memset(mask_pixels,128,sizeof mask_pixels);memset(mask_pixels,0,8*32);
    lv_image_dsc_t mask={.header={.magic=LV_IMAGE_HEADER_MAGIC,.cf=LV_COLOR_FORMAT_A8,.w=32,.h=32,.stride=32},
        .data=mask_pixels,.data_size=sizeof mask_pixels};
    /* Key rasterized pixels; native vector alpha rounding can leave red=1. */
    lv_image_colorkey_t key={.low=lv_color_hex(0x0000fd),.high=lv_color_hex(0x0202ff)};
    for(unsigned cycle=0;cycle<20;cycle++) for(unsigned scene=0;scene<9;scene++) {
        lv_canvas_fill_bg(canvas,lv_color_hex(0x102030),LV_OPA_COVER);
        lv_layer_t *head=display->layer_head;
        uint32_t memory=LV_GLOBAL_DEFAULT()->draw_info.used_memory_for_layers;
        lv_layer_t layer;lv_canvas_init_layer(canvas,&layer);
        lv_draw_image_dsc_t d;lv_draw_image_dsc_init(&d);d.src=&src;
        d.opa=(scene==1 || scene==8)?255:scene==2?192:128;
        d.recolor=lv_color_hex(scene==2?0x0000ff:0x00ff00);
        d.recolor_opa=scene==1?128:scene==2?255:0;
        if(scene==3) d.clip_radius=8;
        if(scene==4) d.bitmap_mask_src=&mask;
        if(scene==5) d.colorkey=&key;
        if(scene==6) d.tile=true;
        if(scene==7) {d.rotation=900;d.scale_x=512;d.scale_y=128;}
        lv_area_t coords={20,20,scene==6?83:51,scene==6?83:51};
        lv_draw_image(&layer,&d,&coords);lv_canvas_finish_layer(canvas,&layer);
        for(unsigned sample=0;sample<3;sample++) {
            unsigned u=sample==0?4:sample==1?12:28;
            unsigned col[3]={sample==0?255:0,sample==2?255:0,sample==1?255:0};
            unsigned tint[3]={0,scene==2?0:255,scene==2?255:0};
            double alpha=(sample==2?0.5:1.0)*d.opa/255.0;
            if(scene==4) alpha*=128.0/255.0;
            if(scene==5 && sample==1) alpha=0;
            unsigned expected[3],bg[3]={16,32,48};
            for(unsigned k=0;k<3;k++) {
                double channel=col[k]*(255-d.recolor_opa)/255.0+tint[k]*d.recolor_opa/255.0;
                expected[k]=(unsigned)lround(channel*alpha+bg[k]*(1-alpha));
            }
            int x=20+u,y=36;
            if(scene==7) {x=11;y=20+2*u;}
            expect(canvas,x,y,expected[0],expected[1],expected[2]);
            if(scene==6) expect(canvas,x+32,y+32,expected[0],expected[1],expected[2]);
        }
        expect(canvas,100,100,16,32,48);
        if(scene==3) expect(canvas,20,20,16,32,48);
        if(scene==4) expect(canvas,24,24,16,32,48);
        assert(display->layer_head==head && LV_GLOBAL_DEFAULT()->draw_info.used_memory_for_layers==memory);
    }
    lv_canvas_fill_bg(canvas,lv_color_hex(0x102030),LV_OPA_COVER);
    lv_layer_t layer;lv_canvas_init_layer(canvas,&layer);
    lv_layer_t *head=display->layer_head;
    uint32_t memory=LV_GLOBAL_DEFAULT()->draw_info.used_memory_for_layers;
    lv_draw_image_dsc_t d;lv_draw_image_dsc_init(&d);d.src=&src;d.opa=128;
    lv_area_t area={20,20,51,51};fail_buffer=true;
    lv_draw_image(&layer,&d,&area);lv_canvas_finish_layer(canvas,&layer);
    assert(!fail_buffer && display->layer_head==head);
    assert(LV_GLOBAL_DEFAULT()->draw_info.used_memory_for_layers==memory);
    expect(canvas,24,36,16,32,48);
    /* Straight-alpha destinations must retain straight colour, even after
     * composing a translucent SVG over earlier canvas content. */
    lv_canvas_fill_bg(canvas,lv_color_hex(0x102030),128);
    lv_canvas_init_layer(canvas,&layer);d.opa=255;
    lv_draw_image(&layer,&d,&area);lv_canvas_finish_layer(canvas,&layer);
    lv_color32_t translucent=lv_canvas_get_px(canvas,48,36);
    assert(translucent.alpha>=189 && translucent.alpha<=193);
    expect(canvas,48,36,5,181,16);

    /* Real image style opacity is already combined by LVGL with inherited
     * opacity. An extra parent opacity layer must apply once, after rendering. */
    lv_obj_set_hidden(canvas,true);
    for(unsigned partial=0;partial<2;partial++) {
        lv_obj_t *parent=lv_obj_create(lv_screen_active());lv_obj_remove_style_all(parent);
        lv_obj_set_pos(parent,10,8);lv_obj_set_size(parent,80,80);lv_obj_set_scrollable(parent,false);
        lv_obj_set_style_opa_layered(parent,128,0);
        lv_obj_t *image=lv_image_create(parent);lv_image_set_src(image,&src);lv_obj_set_pos(image,10,12);
        lv_obj_set_style_image_opa(image,partial?128:255,0);
        lv_refr_now(display);
        for(unsigned sample=0;sample<3;sample++) {
            unsigned u=sample==0?4:sample==1?12:28;
            double alpha=(sample==2?0.5:1.0)*(128.0/255.0)*(partial?128.0/255.0:1.0);
            unsigned col[3]={sample==0?255:0,sample==2?255:0,sample==1?255:0},bg[3]={16,32,48};
            lv_color32_t actual;memcpy(&actual,&display_pixels[36*128+20+u],sizeof actual);
            unsigned channels[3]={actual.red,actual.green,actual.blue};
            for(unsigned k=0;k<3;k++)
                assert(abs((int)channels[k]-(int)lround(col[k]*alpha+bg[k]*(1-alpha)))<=4);
        }
        lv_obj_delete(parent);lv_refr_now(display);
    }
    lv_image_cache_drop(&src);lv_image_cache_drop(&mask);
    lv_obj_delete(canvas);lv_draw_buf_destroy(buf);lv_display_delete(display);lv_deinit();
    puts("PASS SVG composed opacity, recolor, radius, masks, key, tiling, transforms and allocation cleanup");
    return 0;
}
