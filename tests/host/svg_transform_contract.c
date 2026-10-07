/* SPDX-License-Identifier: Apache-2.0 */
#include "lvgl.h"
#include "lvgl_private.h"
#include <assert.h>
#include <math.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
static const char svg[]="<svg width='32' height='24' xmlns='http://www.w3.org/2000/svg'>"
    "<rect width='32' height='24' fill='#ff0000'/>"
    "<circle cx='12' cy='10' r='5' fill='#0000ff'/></svg>";
static void flush(lv_display_t *d,const lv_area_t *a,uint8_t *p)
{ (void)a;(void)p;lv_display_flush_ready(d); }
int main(void)
{
    lv_init();
    lv_display_t *display=lv_display_create(96,96);assert(display);
    lv_display_set_flush_cb(display,flush);
    static uint32_t display_pixels[96*96];
    lv_display_set_color_format(display,LV_COLOR_FORMAT_ARGB8888);
    lv_display_set_buffers(display,display_pixels,NULL,sizeof display_pixels,LV_DISPLAY_RENDER_MODE_DIRECT);
    lv_obj_set_style_bg_color(lv_screen_active(),lv_color_black(),0);
    lv_obj_set_style_bg_opa(lv_screen_active(),LV_OPA_COVER,0);
    lv_obj_t *canvas=lv_canvas_create(lv_screen_active());
    lv_draw_buf_t *buf=lv_draw_buf_create(96,96,LV_COLOR_FORMAT_ARGB8888,0);assert(buf);
    lv_canvas_set_draw_buf(canvas,buf);
    lv_image_dsc_t src={.header={.magic=LV_IMAGE_HEADER_MAGIC,.cf=LV_COLOR_FORMAT_RAW,.w=32,.h=24},
        .data=(const uint8_t *)svg,.data_size=sizeof(svg)-1};
    static const struct {int angle,sx,sy,px,py,x,y;} cases[]={
        {0,256,256,0,0,20,20}, {0,512,128,0,0,12,20},
        {900,256,256,16,12,30,30}, {1800,384,256,3,17,40,45},
        {2700,128,512,16,12,30,25}, {370,384,192,7,3,24,25},
        {-230,256,256,-5,30,25,25}, {0,512,512,16,12,-25,15}
    };
    for(unsigned mode=0;mode<6;mode++)
    for(unsigned n=0;n<sizeof(cases)/sizeof(cases[0]);n++) {
        lv_canvas_fill_bg(canvas,lv_color_black(),LV_OPA_COVER);
        lv_layer_t layer;lv_canvas_init_layer(canvas,&layer);
        layer._clip_area=(lv_area_t){10,8,83,78};
        lv_layer_t parent;lv_layer_init(&parent);
        if(mode>=2) {
            layer.parent=&parent;
            lv_area_move(&layer.buf_area,11,7);
            lv_area_move(&layer._clip_area,11,7);
            lv_area_move(&layer.phy_clip_area,11,7);
        }
        lv_area_t saved=layer._clip_area;
        lv_draw_image_dsc_t d;lv_draw_image_dsc_init(&d);d.src=&src;
        d.base.obj=(mode&1)?NULL:canvas;
        d.rotation=cases[n].angle;d.scale_x=cases[n].sx;d.scale_y=cases[n].sy;
        d.pivot=(lv_point_t){cases[n].px,cases[n].py};
        lv_area_t coords={cases[n].x,cases[n].y,cases[n].x+31,cases[n].y+23};
        if(mode>=2) lv_area_move(&coords,11,7);
        lv_draw_image(&layer,&d,&coords);
        assert(!memcmp(&saved,&layer._clip_area,sizeof saved));
        lv_canvas_finish_layer(canvas,&layer);
        lv_obj_t *widget_parent=NULL;
        if(mode>=4) {
            lv_obj_set_hidden(canvas,true);
            widget_parent=lv_obj_create(lv_screen_active());lv_obj_remove_style_all(widget_parent);
            lv_obj_set_pos(widget_parent,10,8);lv_obj_set_size(widget_parent,74,71);
            lv_obj_set_scrollable(widget_parent,false);
            if(mode==5) lv_obj_set_style_opa_layered(widget_parent,128,0);
            lv_obj_t *image=lv_image_create(widget_parent);lv_image_set_src(image,&src);
            lv_obj_set_pos(image,cases[n].x-10,cases[n].y-8);
            lv_image_set_pivot(image,cases[n].px,cases[n].py);
            lv_image_set_rotation(image,cases[n].angle);
            lv_image_set_scale_x(image,cases[n].sx);lv_image_set_scale_y(image,cases[n].sy);
            lv_refr_now(display);
        }
        unsigned checked=0,failed=0;
        double rad=cases[n].angle*3.14159265358979323846/1800.0;
        double c=cos(rad),s=sin(rad);
        for(int y=0;y<96;y++) for(int x=0;x<96;x++) {
            lv_color32_t p=lv_canvas_get_px(canvas,x,y);
            if(mode>=4) memcpy(&p,&display_pixels[y*96+x],sizeof p);
            if(x<10 || x>83 || y<8 || y>78) {
                assert(!p.red && !p.green && !p.blue);continue;
            }
            double dx=x+0.5-cases[n].x-cases[n].px,dy=y+0.5-cases[n].y-cases[n].py;
            double u=(c*dx+s*dy)*256/cases[n].sx+cases[n].px;
            double v=(-s*dx+c*dy)*256/cases[n].sy+cases[n].py;
            double radius=hypot(u-12,v-10);
            /* Skip only the antialias boundary; all interior/exterior pixels
             * use an independent inverse transform, not FILE/VARIABLE parity. */
            if(fabs(u)<2 || fabs(u-32)<2 || fabs(v)<2 || fabs(v-24)<2 ||
               fabs(radius-5)<2) continue;
            bool inside=u>0 && u<32 && v>0 && v<24,blue=inside && radius<5;
            unsigned opacity=mode==5?128:255;
            unsigned r=inside && !blue?opacity:0,b=blue?opacity:0;
            if(abs((int)p.red-(int)r)>2 || p.green>2 || abs((int)p.blue-(int)b)>2) failed++;
            checked++;
        }
        if(failed) fprintf(stderr,"mode %u scene %u: %u/%u incorrect interior pixels\n",mode,n,failed,checked);
        assert(checked>1000 && !failed);
        if(widget_parent) {lv_obj_delete(widget_parent);lv_refr_now(display);}
    }
    lv_image_cache_drop(&src);lv_obj_delete(canvas);lv_draw_buf_destroy(buf);
    lv_display_delete(display);lv_deinit();
    puts("PASS SVG transforms, independent interior pixels and clip guards");
    return 0;
}
