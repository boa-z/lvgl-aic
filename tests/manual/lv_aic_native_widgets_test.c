/* SPDX-License-Identifier: Apache-2.0
 * Product-independent native widget page. Resources are immutable ROM arrays;
 * animations run only while this page is visible. No device or filesystem I/O. */
#include "lv_aic_native_widgets_test.h"
#if LV_AIC_NATIVE_WIDGET_TEST
#include "lvgl/widgets/lv_animimage.h"
#include "lvgl/widgets/lv_imagebutton.h"
#include "lvgl/widgets/lv_span.h"
#include "lvgl/widgets/lv_scale.h"
#include "lvgl/widgets/lv_spinner.h"
#include "lvgl/widgets/lv_win.h"

/* lv_win is retained solely to exercise SDK-era application compatibility. */
#if defined(__GNUC__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wdeprecated-declarations"
#endif
#define ROW(c) c,c,c,c,c,c,c,c,c,c,c,c,c,c,c,c
#define TILE(c) ROW(c),ROW(c),ROW(c),ROW(c),ROW(c),ROW(c),ROW(c),ROW(c), \
                ROW(c),ROW(c),ROW(c),ROW(c),ROW(c),ROW(c),ROW(c),ROW(c)
static const uint16_t pixels[3][256] = {{TILE(0x249f)}, {TILE(0xfd20)}, {TILE(0x36c8)}};
#define FRAME(i) {.header={.magic=LV_IMAGE_HEADER_MAGIC,.cf=LV_COLOR_FORMAT_RGB565, \
    .w=16,.h=16,.stride=32},.data_size=sizeof(pixels[i]),.data=(const uint8_t *)pixels[i]}
static const lv_image_dsc_t images[3]={FRAME(0),FRAME(1),FRAME(2)};
static const void *frames[]={&images[0],&images[1],&images[2]};
typedef struct {
    lv_obj_t *animation, *spinner, *scale, *needle, *status, *span, *window;
    unsigned value;
    bool visible, paused;
} native_page_t;

static lv_obj_t *label(lv_obj_t *parent,const char *text,int x,int y,int width)
{
    lv_obj_t *obj=lv_label_create(parent);
    if(obj) { lv_label_set_text(obj,text);lv_obj_set_pos(obj,x,y);lv_obj_set_width(obj,width); }
    return obj;
}
static void animations(native_page_t *state)
{
    lv_anim_delete(state->animation,NULL);lv_anim_delete(state->spinner,NULL);
    if(state->visible && !state->paused) {
        lv_animimg_start(state->animation);lv_spinner_set_anim_params(state->spinner,1000,90);
    }
}
static void update(native_page_t *state)
{
    lv_scale_set_line_needle_value(state->scale,state->needle,55,(int32_t)state->value);
    lv_label_set_text_fmt(state->status,"Value %u / %s",state->value,state->paused?"paused":"running");
    lv_spangroup_set_span_text_fmt(state->span,lv_spangroup_get_child(state->span,1),"%u",state->value);
}
static void page_delete(lv_event_t *e)
{
    /* Child callbacks do not use this state during deletion. LVGL owns their
     * animations and all internal line/span allocations. */
    lv_free(lv_event_get_user_data(e));
}
static void close_window(lv_event_t *e)
{
    native_page_t *state=lv_event_get_user_data(e);
    lv_obj_t *win=state->window;state->window=NULL;
    if(win) lv_obj_delete(win);
}
static void action(lv_event_t *e)
{
    lv_obj_t *button=lv_event_get_target_obj(e);
    lv_obj_t *page=lv_obj_get_parent(button);
    native_page_t *state=lv_obj_get_user_data(page);
    unsigned kind=(unsigned)(uintptr_t)lv_event_get_user_data(e);
    if(kind==0) { state->paused=!state->paused;animations(state); }
    else if(kind==1) state->value=(state->value+25)%125;
    else if(!state->window) {
        lv_obj_t *win=lv_win_create(page);
        if(!win) return;
        state->window=win;
        lv_obj_set_pos(win,12,12);lv_obj_set_size(win,lv_obj_get_width(page)-24,lv_obj_get_height(page)-24);
        lv_obj_t *title=lv_win_add_title(win,"SDK window compatibility");
        lv_obj_t *close=lv_win_add_button(win,LV_SYMBOL_CLOSE,52);
        if(!title || !close) { state->window=NULL;lv_obj_delete(win);return; }
        lv_obj_add_event_cb(close,close_window,LV_EVENT_CLICKED,state);
        lv_obj_t *content=lv_win_get_content(win);
        if(!label(content,"Scroll this window, then close it.\nThe page value must survive.",0,0,lv_obj_get_width(page)-80)) {
            state->window=NULL;lv_obj_delete(win);return;
        }
        for(unsigned row=0;row<24;row++) {
            lv_obj_t *text=label(content,"Native window content",0,80+row*40,lv_obj_get_width(page)-80);
            if(!text) { state->window=NULL;lv_obj_delete(win);return; }
        }
    }
    update(state);
}
lv_obj_t *lv_aic_native_widgets_create(lv_obj_t *parent,int32_t width,int32_t height)
{
    if(width<440 || height<340) return NULL;
    native_page_t *state=lv_malloc_zeroed(sizeof(*state));if(!state) return NULL;
    lv_obj_t *page=lv_obj_create(parent);if(!page) { lv_free(state);return NULL; }
    lv_obj_set_user_data(page,state);lv_obj_add_event_cb(page,page_delete,LV_EVENT_DELETE,state);
    lv_obj_set_size(page,width,height);lv_obj_set_style_pad_all(page,0,0);
    lv_obj_set_style_border_width(page,0,0);lv_obj_set_style_radius(page,0,0);
    lv_obj_set_scrollable(page,false);lv_obj_set_style_bg_color(page,lv_color_hex(0x182430),0);
    lv_obj_set_style_text_color(page,lv_color_white(),0);
    int32_t right=width/2+12;
    if(!label(page,"Native animation and controls",16,12,width-32) ||
       !label(page,"Frames: blue / amber / green",16,44,width/2-24) ||
       !label(page,"Tap color to change value",16,154,width/2-24)) goto fail;
    state->animation=lv_animimg_create(page);state->spinner=lv_spinner_create(page);
    state->scale=lv_scale_create(page);state->span=lv_spangroup_create(page);
    if(!state->animation || !state->spinner || !state->scale || !state->span) goto fail;
    lv_obj_set_pos(state->animation,16,82);lv_image_set_scale(state->animation,512);
    lv_animimg_set_src(state->animation,frames,3);lv_animimg_set_duration(state->animation,900);
    lv_animimg_set_repeat_count(state->animation,LV_ANIM_REPEAT_INFINITE);
    lv_image_set_src(state->animation,frames[0]);
    lv_obj_set_pos(state->spinner,100,76);lv_obj_set_size(state->spinner,56,56);
    lv_obj_set_pos(state->scale,right+22,44);lv_obj_set_size(state->scale,152,152);
    lv_scale_set_mode(state->scale,LV_SCALE_MODE_ROUND_OUTER);lv_scale_set_range(state->scale,0,100);
    lv_scale_set_total_tick_count(state->scale,21);lv_scale_set_major_tick_every(state->scale,5);
    lv_scale_set_angle_range(state->scale,240);lv_scale_set_rotation(state->scale,150);
    lv_obj_set_style_text_color(state->scale,lv_color_white(),LV_PART_INDICATOR);
    lv_obj_set_style_arc_color(state->scale,lv_color_hex(0x9db7c8),LV_PART_MAIN);
    lv_obj_set_style_line_color(state->scale,lv_color_hex(0x9db7c8),LV_PART_ITEMS);
    lv_obj_set_style_line_color(state->scale,lv_color_white(),LV_PART_INDICATOR);
    state->needle=lv_line_create(state->scale);if(!state->needle) goto fail;
    lv_obj_set_style_line_color(state->needle,lv_color_hex(0xffc04d),0);lv_obj_set_style_line_width(state->needle,3,0);
    lv_obj_set_pos(state->span,right,216);lv_obj_set_size(state->span,width-right-20,76);
    lv_spangroup_set_mode(state->span,LV_SPAN_MODE_FIXED);lv_spangroup_set_overflow(state->span,LV_SPAN_OVERFLOW_CLIP);
    const char *words[]={"Current value: ","0",". Rich text wraps inside its own column."};
    for(unsigned i=0;i<3;i++) {
        lv_span_t *span=lv_spangroup_add_span(state->span);if(!span) goto fail;
        lv_span_set_text_static(span,words[i]);
        lv_style_set_text_color(lv_span_get_style(span),lv_color_hex(i==1?0xffc04d:0xffffff));
    }
    lv_spangroup_refresh(state->span);
    lv_obj_t *imagebutton=lv_imagebutton_create(page);if(!imagebutton) goto fail;
    lv_obj_set_pos(imagebutton,16,190);lv_obj_set_size(imagebutton,160,32);lv_obj_set_checkable(imagebutton,true);
    const unsigned state_colors[]={0,1,1,2,1,1};
    for(unsigned i=0;i<6;i++) lv_imagebutton_set_src(imagebutton,(lv_imagebutton_state_t)i,NULL,&images[state_colors[i]],NULL);
    lv_obj_add_event_cb(imagebutton,action,LV_EVENT_CLICKED,(void *)(uintptr_t)1);
    for(unsigned i=0;i<2;i++) {
        lv_obj_t *button=lv_button_create(page);if(!button) goto fail;
        lv_obj_set_pos(button,16+(int32_t)i*(width/2),height-60);lv_obj_set_size(button,width/2-32,44);
        lv_obj_add_event_cb(button,action,LV_EVENT_CLICKED,(void *)(uintptr_t)(i?2:0));
        lv_obj_t *text=lv_label_create(button);if(!text) goto fail;
        lv_label_set_text(text,i?"Open window":"Pause / resume");lv_obj_center(text);
    }
    state->status=label(page,"",16,250,width/2-24);if(!state->status) goto fail;
    update(state);animations(state);lv_obj_set_hidden(page,true);return page;
fail:
    lv_obj_delete(page);return NULL;
}
void lv_aic_native_widgets_show(lv_obj_t *page,bool visible)
{
    if(!page) return;
    native_page_t *state=lv_obj_get_user_data(page);
    if(state->visible!=visible) {
        state->visible=visible;animations(state);
        if(!visible && state->window) {
            lv_obj_delete(state->window);state->window=NULL;
        }
    }
    lv_obj_set_hidden(page,!visible);
}
#if defined(__GNUC__)
#pragma GCC diagnostic pop
#endif
#endif
