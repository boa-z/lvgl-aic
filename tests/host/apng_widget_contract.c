/* SPDX-License-Identifier: Apache-2.0 */
#define AIC_LVGL_USE_PRIVATE_API 1
#include "lv_aic_apng_widget.h"
#include "lv_aic_player_control.h"
#include <math.h>
#include "../../tests/manual/lv_aic_apng_test.h"
#include "lv_aic_rgb_image_private.h"
#include "lvgl_aic_private.h"
#include <assert.h>
#include <stdlib.h>
#include <string.h>
struct lv_aic_apng_playback { lv_aic_apng_playback_status_t status;unsigned readers;bool closing; };
static lv_aic_apng_playback_t *active;
static bool allow_exit=true,fail_prepare;
static unsigned frames,created,freed,retained,released;
static char last_path[128];
typedef struct { lv_aic_apng_playback_t *p;uint32_t pixel; } owner_t;
lv_aic_apng_playback_t *lv_aic_apng_playback_prepare(const char *path,const lv_aic_apng_playback_options_t *o)
{
    assert(o->snapshots==2 || o->snapshots==3);if(active || fail_prepare) return NULL;
    active=calloc(1,sizeof(*active));assert(active);created++;strcpy(last_path,path);
    active->status.state=LV_AIC_APNG_READY;return active;
}
bool lv_aic_apng_playback_start(lv_aic_apng_playback_t *p)
{ if(!p || p->closing) return false;p->status.state=LV_AIC_APNG_PLAYING;return true; }
bool lv_aic_apng_playback_pause(lv_aic_apng_playback_t *p,bool pause)
{ if(!p || p->closing) return false;if(pause) p->status.state=LV_AIC_APNG_PLAYBACK_PAUSED;return true; }
bool lv_aic_apng_playback_rate(lv_aic_apng_playback_t *p,uint32_t n,uint32_t d)
{ if(!p || p->closing) return false;p->status.rate_num=n;p->status.rate_den=d;return true; }
bool lv_aic_apng_playback_restart(lv_aic_apng_playback_t *p)
{ if(!p || p->closing) return false;p->status.restarts++;return true; }
lv_aic_apng_playback_status_t lv_aic_apng_playback_status(lv_aic_apng_playback_t *p)
{ return p?p->status:(lv_aic_apng_playback_status_t){.state=LV_AIC_APNG_CLOSED,.finished=true}; }
void lv_aic_apng_playback_close(lv_aic_apng_playback_t *p)
{ if(p) { p->closing=true;p->status.state=LV_AIC_APNG_CLOSING; } }
bool lv_aic_apng_playback_destroy(lv_aic_apng_playback_t *p)
{
    if(!p) return true;
    if(!p->closing || p->readers || !allow_exit) return false;
    assert(p==active);active=NULL;free(p);freed++;return true;
}
static bool retain(void *v) { owner_t *o=v;o->p->readers++;retained++;return true; }
static void release(void *v) { owner_t *o=v;o->p->readers--;released++;free(o); }
bool lv_aic_apng_playback_poll(lv_aic_apng_playback_t *p,lv_aic_rgb_image_t **image,uint64_t *seq)
{
    if(!p || p->closing || !frames) return false;
    frames--;owner_t *o=calloc(1,sizeof(*o));assert(o);o->p=p;o->pixel=0xff123456;
    lv_aic_rgb_frame_t f={LV_COLOR_FORMAT_ARGB8888,1,1,4,(const uint8_t *)&o->pixel,4};
    *image=lv_aic_rgb_image_create(&f,retain,release,o);assert(*image);*seq=retained;return true;
}
static void tick(void) { lv_tick_inc(25);lv_timer_handler(); }
static void flush(lv_display_t *d,const lv_area_t *a,uint8_t *pixels)
{ (void)a;(void)pixels;lv_display_flush_ready(d); }
static void delete_event(lv_event_t *e) { lv_obj_delete(lv_event_get_target_obj(e)); }
static lv_obj_t *make(lv_obj_t *parent)
{
    lv_obj_t *o=lv_aic_apng_create(parent);assert(o);
    assert(lv_aic_apng_set_src(o,"one.png")==LV_RESULT_INVALID);
    lv_aic_apng_playback_options_t options={.limits={1024,1024,16,4},.stream_budget=4096,
        .snapshot_budget=4096,.cma_budget=4096,.packet_limit=1024,.snapshots=2,.minimum_delay_us=1000};
    assert(lv_aic_apng_configure(o,&options)==LV_RESULT_OK);return o;
}
int main(void)
{
    lv_init();assert(lv_aic_rgb_image_decoder_init());
    lv_display_t *d=lv_display_create(16,16);assert(d);static uint8_t pixels[16*16*4];
    lv_display_set_color_format(d,LV_COLOR_FORMAT_ARGB8888);
    lv_display_set_buffers(d,pixels,NULL,sizeof(pixels),LV_DISPLAY_RENDER_MODE_DIRECT);
    lv_display_set_flush_cb(d,flush);lv_timer_pause(lv_display_get_refr_timer(d));lv_obj_t *screen=lv_display_get_screen_active(d);
    lv_obj_t *obj=make(screen);assert(lv_aic_apng_set_rate(obj,2,1)==LV_RESULT_OK);
    assert(lv_aic_apng_set_src(obj,"one.png")==LV_RESULT_OK);tick();assert(active);
    assert(lv_aic_apng_get_status(obj).state==LV_AIC_APNG_READY);
    float rate=NAN;assert(lv_aic_player_control(obj,LV_AIC_PLAYER_CMD_SET_PLAYBACK_RATE,&rate)==LV_RESULT_INVALID);
    rate=INFINITY;assert(lv_aic_player_control(obj,LV_AIC_PLAYER_CMD_SET_PLAYBACK_RATE,&rate)==LV_RESULT_INVALID);
    rate=0.5f;assert(lv_aic_player_control(obj,LV_AIC_PLAYER_CMD_SET_PLAYBACK_RATE,&rate)==LV_RESULT_OK);tick();
    rate=99;assert(lv_aic_player_control(obj,LV_AIC_PLAYER_CMD_GET_PLAYBACK_RATE,&rate)==LV_RESULT_OK && rate==0.5f);
    rate=2;assert(lv_aic_player_control(obj,LV_AIC_PLAYER_CMD_SET_PLAYBACK_RATE,&rate)==LV_RESULT_OK);tick();
    uint64_t target=1;assert(lv_aic_player_control(obj,LV_AIC_PLAYER_CMD_SET_PLAY_TIME,&target)==LV_RESULT_INVALID);
    target=99;assert(lv_aic_player_control(obj,LV_AIC_PLAYER_CMD_GET_PLAY_TIME,&target)==LV_RESULT_INVALID && target==99);
    int32_t volume=77;assert(lv_aic_player_control(obj,LV_AIC_PLAYER_CMD_GET_VOLUME,&volume)==LV_RESULT_INVALID && volume==77);
    assert(lv_aic_player_control(obj,LV_AIC_PLAYER_CMD_PLAY_END,NULL)==LV_RESULT_INVALID);
    assert(lv_aic_player_control(screen,LV_AIC_PLAYER_CMD_START,NULL)==LV_RESULT_INVALID);
    assert(lv_aic_player_control(obj,(lv_aic_player_cmd_t)99,NULL)==LV_RESULT_INVALID);
    assert(lv_aic_apng_start(obj)==LV_RESULT_OK);frames=1;tick();
    const lv_aic_rgb_frame_t *f;
    lv_aic_rgb_image_t *reader=lv_aic_rgb_image_acquire(lv_image_get_src(obj),&f);assert(reader);
    assert(lv_aic_apng_set_src(obj,"two.png")==LV_RESULT_OK);
    assert(lv_aic_apng_set_src(obj,"three.png")==LV_RESULT_OK);
    assert(lv_aic_apng_start(obj)==LV_RESULT_OK);tick();assert(!strcmp(last_path,"one.png"));
    assert(((const uint32_t *)f->data)[0]==0xff123456);
    lv_aic_rgb_image_release_lease(reader);tick();assert(!strcmp(last_path,"three.png"));
    assert(lv_aic_apng_get_status(obj).state==LV_AIC_APNG_PLAYING && active->status.rate_num==2);
    active->status.state=LV_AIC_APNG_TERMINAL;tick();assert(lv_aic_apng_start(obj)==LV_RESULT_OK);
    assert(active->status.restarts==1);target=0;assert(lv_aic_player_control(obj,LV_AIC_PLAYER_CMD_SET_PLAY_TIME,&target)==LV_RESULT_OK);assert(active->status.restarts==2);assert(lv_aic_apng_pause(obj,true)==LV_RESULT_OK);tick();
    assert(lv_aic_apng_get_status(obj).state==LV_AIC_APNG_PLAYBACK_PAUSED);
    frames=1;tick();lv_draw_task_t pending={0};d->layer_head->draw_task_head=&pending;
    lv_obj_delete(obj);lv_timer_pause(lv_display_get_refr_timer(d));tick();
    assert(active && retained>released && lv_aic_apng_pending_cleanup()==1);
    d->layer_head->draw_task_head=NULL;allow_exit=false;tick();assert(active && retained==released);
    allow_exit=true;tick();assert(!active && !lv_aic_apng_pending_cleanup());
    obj=make(screen);assert(lv_aic_apng_set_src(obj,"event.png")==LV_RESULT_OK);
    lv_obj_add_event_cb(obj,delete_event,LV_EVENT_VALUE_CHANGED,NULL);tick();
    assert(lv_aic_apng_pending_cleanup()==1);tick();assert(!active);
    obj=make(screen);fail_prepare=true;assert(lv_aic_apng_set_src(obj,"fail.png")==LV_RESULT_OK);tick();
    assert(lv_aic_apng_get_status(obj).state==LV_AIC_APNG_FAULT);
    assert(lv_aic_apng_start(obj)==LV_RESULT_INVALID);fail_prepare=false;
    assert(lv_aic_apng_set_src(obj,"retry.png")==LV_RESULT_OK);tick();assert(active);
    assert(lv_aic_apng_close(obj)==LV_RESULT_OK);tick();assert(!active);
    assert(lv_aic_apng_start(obj)==LV_RESULT_OK);tick();assert(active);
    active->status.state=LV_AIC_APNG_FAULT;active->closing=true;frames=0;tick();
    assert(lv_aic_apng_get_status(obj).state==LV_AIC_APNG_FAULT);
    assert(lv_aic_apng_close(obj)==LV_RESULT_OK);tick();lv_obj_delete(obj);tick();
    assert(!active && !lv_aic_apng_pending_cleanup() && created==freed && retained==released);
    assert(lv_aic_rgb_image_decoder_deinit());
    lv_display_set_resolution(d,800,480);
    static uint8_t panel_pixels[800*480*4];
    lv_display_set_buffers(d,panel_pixels,NULL,sizeof(panel_pixels),LV_DISPLAY_RENDER_MODE_DIRECT);
    assert(lv_aic_apng_test_show()==LV_AIC_OK);tick();assert(active);
    assert(lv_aic_apng_test_show()==LV_AIC_ERR_INVALID_STATE);
    lv_aic_apng_test_poll();
    lv_obj_t *panel=lv_obj_get_child(lv_layer_top(),-1);assert(panel);
    lv_obj_send_event(lv_obj_get_child(panel,8),LV_EVENT_CLICKED,NULL);tick();
    assert(!strcmp(last_path,"/data/mpp_test/apng-loop.png"));
    lv_obj_send_event(lv_obj_get_child(panel,2),LV_EVENT_CLICKED,NULL);tick();
    assert(active->status.state==LV_AIC_APNG_PLAYBACK_PAUSED);
    lv_obj_send_event(lv_obj_get_child(panel,6),LV_EVENT_CLICKED,NULL);tick();
    assert(active->status.rate_num==2);
    lv_obj_send_event(lv_obj_get_child(panel,9),LV_EVENT_CLICKED,NULL);tick();assert(!active);
    assert(lv_aic_apng_test_show()==LV_AIC_OK);tick();lv_aic_apng_test_deinit();tick();
    assert(!active && !lv_aic_apng_pending_cleanup());
    lv_display_delete(d);assert(lv_aic_rgb_image_decoder_deinit());lv_deinit();return 0;
}
