/* SPDX-License-Identifier: Apache-2.0 */
#include "lv_aic_native_widgets_test.h"
#include "lvgl/widgets/lv_animimage.h"
#include "lvgl/widgets/lv_imagebutton.h"
#include "lvgl/widgets/lv_span.h"
#include "lvgl/widgets/lv_scale.h"
#include "lvgl/widgets/lv_spinner.h"
#include "lvgl/widgets/lv_win.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#if defined(__GNUC__)
#pragma GCC diagnostic ignored "-Wdeprecated-declarations"
#endif
static uint8_t pixels[800*480*2];
static int width,height;
static lv_point_t point;
static bool down;
static void flush(lv_display_t *d,const lv_area_t *a,uint8_t *p)
{ (void)a;(void)p;lv_display_flush_ready(d); }
static void read_pointer(lv_indev_t *i,lv_indev_data_t *data)
{ (void)i;data->point=point;data->state=down?LV_INDEV_STATE_PRESSED:LV_INDEV_STATE_RELEASED; }
static void advance(unsigned milliseconds)
{ for(unsigned i=0;i<milliseconds;i+=20) { lv_tick_inc(20);lv_timer_handler(); } }
static void click(lv_indev_t *input,lv_obj_t *obj)
{
    lv_obj_update_layout(lv_screen_active());lv_area_t a;lv_obj_get_coords(obj,&a);
    assert(a.x1>=0 && a.y1>=0 && a.x2<width && a.y2<height);
    point=(lv_point_t){(a.x1+a.x2)/2,(a.y1+a.y2)/2};
    down=true;lv_tick_inc(40);lv_indev_read(input);
    down=false;lv_tick_inc(40);lv_indev_read(input);advance(120);
}
static lv_obj_t *type(lv_obj_t *parent,const lv_obj_class_t *class)
{
    for(unsigned i=0;i<lv_obj_get_child_count(parent);i++) {
        lv_obj_t *child=lv_obj_get_child(parent,i);
        if(lv_obj_check_type(child,class)) return child;
    }
    return NULL;
}
static lv_obj_t *button(lv_obj_t *parent,const char *text)
{
    for(unsigned i=0;i<lv_obj_get_child_count(parent);i++) {
        lv_obj_t *obj=lv_obj_get_child(parent,i);
        if(!lv_obj_check_type(obj,&lv_button_class)) continue;
        lv_obj_t *label=lv_obj_get_child(obj,0);
        if(label && !strcmp(lv_label_get_text(label),text)) return obj;
    }
    return NULL;
}
static uint32_t region_hash(lv_display_t *display,lv_obj_t *object)
{
    lv_refr_now(display);lv_area_t a;lv_obj_get_coords(object,&a);uint32_t hash=2166136261U;
    for(int y=a.y1;y<=a.y2;y++) for(int x=a.x1;x<=a.x2;x++) {
        assert(x>=0 && y>=0 && x<width && y<height);
        for(unsigned b=0;b<2;b++) hash=(hash^pixels[(y*width+x)*2+b])*16777619U;
    }
    return hash;
}
static void snapshot(lv_display_t *d,unsigned cycle)
{
    const char *prefix=getenv("AIC_NATIVE_PPM_PREFIX");if(!prefix || cycle) return;
    lv_refr_now(d);char path[512];snprintf(path,sizeof path,"%s-%dx%d.ppm",prefix,width,height);
    FILE *f=fopen(path,"wb");assert(f);fprintf(f,"P6\n%d %d\n255\n",width,height);
    for(int i=0;i<width*height;i++) {
        uint16_t p=(uint16_t)pixels[2*i]|((uint16_t)pixels[2*i+1]<<8);
        uint8_t rgb[3]={((p>>11)&31)*255/31,((p>>5)&63)*255/63,(p&31)*255/31};
        assert(fwrite(rgb,1,3,f)==3);
    }
    assert(!fclose(f));
}
int main(void)
{
    for(unsigned portrait=0;portrait<2;portrait++) for(unsigned cycle=0;cycle<3;cycle++) {
        width=portrait?480:800;height=portrait?800:480;
        lv_init();lv_display_t *display=lv_display_create(width,height);assert(display);
        lv_display_set_color_format(display,LV_COLOR_FORMAT_RGB565);
        lv_display_set_buffers(display,pixels,NULL,sizeof pixels,LV_DISPLAY_RENDER_MODE_FULL);
        lv_display_set_flush_cb(display,flush);
        lv_indev_t *input=lv_indev_create();assert(input);
        lv_indev_set_type(input,LV_INDEV_TYPE_POINTER);lv_indev_set_read_cb(input,read_pointer);
        lv_obj_t *screen=lv_screen_active();unsigned children=lv_obj_get_child_count(screen);
        uint32_t animations=lv_anim_count_running();
        assert(!lv_aic_native_widgets_create(screen,320,200));
        lv_obj_t *page=lv_aic_native_widgets_create(screen,width,height-64);assert(page);
        lv_obj_set_pos(page,0,64);assert(lv_obj_is_hidden(page));
        assert(lv_anim_count_running()==animations);
        lv_obj_t *anim=type(page,&lv_animimg_class),*spinner=type(page,&lv_spinner_class);
        lv_obj_t *scale=type(page,&lv_scale_class),*span=type(page,&lv_spangroup_class);
        lv_obj_t *imagebutton=type(page,&lv_imagebutton_class);
        lv_obj_t *pause=button(page,"Pause / resume"),*open=button(page,"Open window");
        assert(anim && spinner && scale && span && imagebutton && pause && open);
        assert(lv_spangroup_get_span_count(span)==3);
        lv_aic_native_widgets_show(page,true);assert(!lv_obj_is_hidden(page));
        lv_obj_update_layout(screen);
        for(unsigned i=0;i<lv_obj_get_child_count(page);i++) {
            lv_area_t a;lv_obj_get_coords(lv_obj_get_child(page,i),&a);
            assert(a.x1>=0 && a.y1>=64 && a.x2<width && a.y2<height);
        }
        const void *first=lv_image_get_src(anim);bool changed=false;
        uint32_t initial_spinner=region_hash(display,spinner);bool spun=false;
        uint32_t initial_frame=region_hash(display,anim);bool animated_pixels=false;
        unsigned frames_seen=0;
        for(unsigned i=0;i<12;i++) {
            advance(100);changed|=first!=lv_image_get_src(anim);
            spun|=initial_spinner!=region_hash(display,spinner);
            animated_pixels|=initial_frame!=region_hash(display,anim);
            for(unsigned frame=0;frame<lv_animimg_get_src_count(anim);frame++)
                if(lv_image_get_src(anim)==lv_animimg_get_src(anim)[frame]) frames_seen|=1U<<frame;
        }
        assert(changed && spun && animated_pixels && frames_seen==7);
        click(input,pause);advance(200);
        first=lv_image_get_src(anim);uint32_t frozen=region_hash(display,page);
        advance(1400);assert(first==lv_image_get_src(anim) && region_hash(display,page)==frozen);
        assert(lv_anim_count_running()==animations);
        lv_obj_t *needle=type(scale,&lv_line_class);assert(needle);
        uint32_t needle_before=region_hash(display,scale);
        assert(!lv_obj_has_state(imagebutton,LV_STATE_CHECKED));
        click(input,imagebutton);assert(lv_obj_has_state(imagebutton,LV_STATE_CHECKED));
        assert(!strcmp(lv_span_get_text(lv_spangroup_get_child(span,1)),"25"));
        assert(region_hash(display,scale)!=needle_before);
        uint32_t checked=region_hash(display,imagebutton);
        click(input,imagebutton);assert(!lv_obj_has_state(imagebutton,LV_STATE_CHECKED));
        assert(region_hash(display,imagebutton)!=checked);
        assert(!strcmp(lv_span_get_text(lv_spangroup_get_child(span,1)),"50"));
        snapshot(display,cycle);
        unsigned page_children=lv_obj_get_child_count(page);
        for(unsigned repeat=0;repeat<20;repeat++) {
            click(input,open);lv_obj_t *win=type(page,&lv_win_class);assert(win);
            assert(lv_obj_get_child_count(page)==page_children+1);
            /* The window must intercept a click over the underlying control. */
            click(input,imagebutton);
            assert(!strcmp(lv_span_get_text(lv_spangroup_get_child(span,1)),"50"));
            lv_obj_t *content=lv_win_get_content(win);lv_obj_update_layout(content);
            lv_area_t a;lv_obj_get_coords(content,&a);
            point=(lv_point_t){(a.x1+a.x2)/2,a.y2-30};down=true;lv_tick_inc(20);lv_indev_read(input);
            for(unsigned i=0;i<7;i++) { point.y-=18;lv_tick_inc(20);lv_indev_read(input);lv_timer_handler(); }
            down=false;lv_tick_inc(20);lv_indev_read(input);advance(400);
            assert(lv_obj_get_scroll_y(content)>0);
            click(input,type(lv_win_get_header(win),&lv_button_class));
            assert(!type(page,&lv_win_class) && lv_obj_get_child_count(page)==page_children);
        }
        click(input,open);assert(type(page,&lv_win_class));
        lv_aic_native_widgets_show(page,false);assert(!type(page,&lv_win_class));
        advance(1000);assert(lv_anim_count_running()==animations);
        lv_aic_native_widgets_show(page,true);advance(300);
        assert(first==lv_image_get_src(anim)); /* Paused state survives page hide. */
        click(input,pause);assert(lv_anim_count_running()>animations);
        lv_obj_delete(page);advance(1200);
        assert(lv_anim_count_running()==animations && lv_obj_get_child_count(screen)==children);
        lv_indev_delete(input);lv_display_delete(display);lv_deinit();
    }
    puts("PASS native widgets: two orientations, six lifecycles, rendered animation/pause/needle/state, 120 pointer-scrolled windows and teardown");
    return 0;
}
