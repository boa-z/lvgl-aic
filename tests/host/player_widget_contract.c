/* SPDX-License-Identifier: Apache-2.0 */
#define AIC_LVGL_USE_PRIVATE_API 1
#include "lv_aic_player.h"
#include "lv_aic_yuv_image_private.h"
#include "lvgl_aic_private.h"
#include <assert.h>
#include <stdlib.h>
#include <string.h>
struct lv_aic_player_playback { lv_aic_playback_status_t status; unsigned readers; bool closing; };
static lv_aic_player_playback_t *active;
static unsigned created,freed,retained,released,frames;
static bool allow_exit=true,fail_prepare,rgb;
static char last_uri[128];
typedef struct { lv_aic_player_playback_t *player; uint8_t y[16],uv[16],rgb[48]; } producer_t;
lv_aic_player_playback_t *lv_aic_player_playback_prepare(const char *uri,const lv_aic_playback_options_t *options)
{
    assert(options->cma_budget && options->extra_frames==3);
    if(active || fail_prepare) return NULL;
    strcpy(last_uri,uri); active=calloc(1,sizeof(*active)); assert(active); created++;
    active->status=(lv_aic_playback_status_t){.state=LV_AIC_PLAYBACK_OPENING,.volume=-1}; return active;
}
bool lv_aic_player_playback_start(lv_aic_player_playback_t *p)
{ if(!p || p->closing) return false; p->status.state=LV_AIC_PLAYBACK_PLAYING; return true; }
bool lv_aic_player_playback_pause(lv_aic_player_playback_t *p,bool paused)
{ if(!p || p->closing) return false; p->status.state=paused?LV_AIC_PLAYBACK_PAUSED:LV_AIC_PLAYBACK_PLAYING; return true; }
bool lv_aic_player_playback_volume(lv_aic_player_playback_t *p,int volume)
{ if(!p || p->closing) return false; p->status.volume=volume; return true; }
lv_aic_playback_status_t lv_aic_player_playback_status(lv_aic_player_playback_t *p)
{ return p?p->status:(lv_aic_playback_status_t){.state=LV_AIC_PLAYBACK_CLOSED,.volume=-1}; }
void lv_aic_player_playback_close(lv_aic_player_playback_t *p)
{ if(p) { p->closing=true; p->status.state=LV_AIC_PLAYBACK_CLOSING; } }
bool lv_aic_player_playback_destroy(lv_aic_player_playback_t *p)
{
    if(!p) return true;
    if(!p->closing || p->readers || !allow_exit) return false;
    assert(p==active); active=NULL; free(p); freed++; return true;
}
static bool retain(void *ctx) { producer_t *p=ctx; p->player->readers++; retained++; return true; }
static void release(void *ctx) { producer_t *p=ctx; assert(p->player->readers); p->player->readers--; released++; free(p); }
bool lv_aic_player_playback_poll(lv_aic_player_playback_t *p,lv_aic_player_image_t *out)
{
    if(!frames) return false;
    frames--; producer_t *data=calloc(1,sizeof(*data)); assert(data); data->player=p;
    if(rgb) {
        memset(data->rgb,255,sizeof(data->rgb));
        lv_aic_rgb_frame_t f={.width=4,.height=4,.stride=12,.format=LV_COLOR_FORMAT_RGB888,
            .data=data->rgb,.capacity=sizeof(data->rgb)};
        out->rgb=lv_aic_rgb_image_create(&f,retain,release,data); assert(out->rgb);
    } else {
        memset(data->y,235,sizeof(data->y)); memset(data->uv,128,sizeof(data->uv));
        lv_aic_yuv_frame_t f={.width=4,.height=4,.format=LV_AIC_YUV_NV16,
            .color_space=LV_AIC_YUV_BT601_LIMITED,.planes={{data->y,4,16},{data->uv,4,16}}};
        out->yuv=lv_aic_yuv_image_create(&f,retain,release,data); assert(out->yuv);
    }
    return true;
}
const lv_image_dsc_t *lv_aic_player_image_source(const lv_aic_player_image_t *p)
{ return p->rgb?lv_aic_rgb_image_source(p->rgb):lv_aic_yuv_image_source(p->yuv); }
void lv_aic_player_image_destroy(lv_aic_player_image_t *p)
{ if(p->rgb) lv_aic_rgb_image_destroy(p->rgb); if(p->yuv) lv_aic_yuv_image_destroy(p->yuv); *p=(lv_aic_player_image_t){0}; }
static void tick(void) { lv_tick_inc(25); lv_timer_handler(); }
static void flush(lv_display_t *d,const lv_area_t *a,uint8_t *p) { (void)a;(void)p;lv_display_flush_ready(d); }
static void delete_on_event(lv_event_t *e) { lv_obj_delete(lv_event_get_target_obj(e)); }
static void replace_on_event(lv_event_t *e)
{
    lv_obj_t *o=lv_event_get_target_obj(e);
    lv_obj_remove_event_cb(o,replace_on_event);
    assert(lv_aic_player_set_src(o,"callback.mp4")==LV_RESULT_OK);
    assert(lv_aic_player_start(o)==LV_RESULT_OK);
}
static lv_obj_t *make(void)
{
    lv_obj_t *o=lv_aic_player_create(lv_screen_active()); assert(o);
    assert(lv_aic_player_set_src(o,"one.mp4")==LV_RESULT_INVALID);
    lv_aic_playback_options_t options={.cma_budget=4096,.extra_frames=3,.color_space=LV_AIC_YUV_BT601_LIMITED};
    assert(lv_aic_player_configure(o,&options)==LV_RESULT_OK); return o;
}
int main(void)
{
    lv_init(); assert(lv_aic_yuv_image_decoder_init()); assert(lv_aic_rgb_image_decoder_init());
    static uint8_t pixels[16*16*3]; lv_display_t *d=lv_display_create(16,16);
    lv_display_set_color_format(d,LV_COLOR_FORMAT_RGB888);
    lv_display_set_buffers(d,pixels,NULL,sizeof(pixels),LV_DISPLAY_RENDER_MODE_DIRECT);
    lv_display_set_flush_cb(d,flush); lv_timer_pause(lv_display_get_refr_timer(d));
    lv_obj_t *o=make(); fail_prepare=true;
    assert(lv_aic_player_set_src(o,"one.mp4")==LV_RESULT_INVALID); tick();
    assert(lv_aic_player_get_state(o)==LV_AIC_PLAYER_FAULT); fail_prepare=false;
    assert(lv_aic_player_set_volume(o,37)==LV_RESULT_OK);
    assert(lv_aic_player_set_src(o,"one.mp4")==LV_RESULT_OK);
    assert(active->status.volume==37); active->status.state=LV_AIC_PLAYBACK_READY; tick();
    assert(lv_aic_player_get_state(o)==LV_AIC_PLAYER_READY);
    assert(lv_aic_player_start(o)==LV_RESULT_OK); lv_obj_set_pos(o,0,0);
    for(unsigned i=0;i<4;i++) {
        rgb=(i&1)!=0; frames=1; tick(); lv_refr_now(d);
        for(unsigned y=0;y<4;y++) for(unsigned x=0;x<12;x++) assert(pixels[y*48+x]==255);
        assert(retained-released==1);
    }
    assert(lv_aic_player_pause(o)==LV_RESULT_OK); tick(); assert(lv_aic_player_get_state(o)==LV_AIC_PLAYER_PAUSED);
    assert(lv_aic_player_resume(o)==LV_RESULT_OK); rgb=false; frames=1; tick();
    const lv_aic_yuv_frame_t *view;
    lv_aic_yuv_image_t *reader=lv_aic_yuv_image_acquire(lv_image_get_src(o),&view); assert(reader);
    assert(lv_aic_player_set_src(o,"two.mp4")==LV_RESULT_OK);
    assert(lv_aic_player_set_src(o,"three.mp4")==LV_RESULT_OK);
    assert(lv_aic_player_start(o)==LV_RESULT_OK); tick();
    assert(!strcmp(last_uri,"one.mp4") && view->planes[0].data[0]==235);
    lv_aic_yuv_image_release_lease(reader); tick(); tick();
    assert(!strcmp(last_uri,"three.mp4") && active->status.volume==37);
    assert(lv_aic_player_get_state(o)==LV_AIC_PLAYER_PLAYING);
    active->status.state=LV_AIC_PLAYBACK_TERMINAL; frames=1; tick();
    assert(lv_aic_player_get_state(o)==LV_AIC_PLAYER_TERMINAL && lv_image_get_src(o));
    unsigned count=created; assert(lv_aic_player_start(o)==LV_RESULT_OK); tick(); tick(); assert(created==count+1);
    assert(lv_aic_player_stop(o)==LV_RESULT_OK); tick();
    assert(!active && lv_aic_player_get_state(o)==LV_AIC_PLAYER_STOPPED);
    assert(lv_aic_player_start(o)==LV_RESULT_OK); tick(); frames=1; tick();
    lv_draw_task_t pending={0}; d->layer_head->draw_task_head=&pending;
    lv_obj_delete(o); lv_timer_pause(lv_display_get_refr_timer(d)); tick();
    assert(active && retained-released==1 && lv_aic_player_pending_cleanup()==1);
    d->layer_head->draw_task_head=NULL; allow_exit=false; tick(); assert(active && retained==released);
    allow_exit=true; tick(); assert(!active && !lv_aic_player_pending_cleanup());
    o=make(); assert(lv_aic_player_set_src(o,"one.mp4")==LV_RESULT_OK);
    lv_obj_add_event_cb(o,replace_on_event,LV_EVENT_VALUE_CHANGED,NULL); tick(); tick(); tick();
    assert(!strcmp(last_uri,"callback.mp4") && lv_aic_player_get_state(o)==LV_AIC_PLAYER_PLAYING);
    lv_obj_add_event_cb(o,delete_on_event,LV_EVENT_VALUE_CHANGED,NULL);
    assert(lv_aic_player_set_volume(o,44)==LV_RESULT_OK); tick(); tick(); assert(!active);
    o=make(); assert(lv_aic_player_set_src(o,"fault.mp4")==LV_RESULT_OK);
    assert(lv_aic_player_start(o)==LV_RESULT_OK); frames=1; tick();
    active->status.state=LV_AIC_PLAYBACK_FAULT; active->closing=true; tick();
    assert(lv_aic_player_get_state(o)==LV_AIC_PLAYER_FAULT && !lv_image_get_src(o));
    assert(lv_aic_player_start(o)==LV_RESULT_INVALID);
    assert(lv_aic_player_close(o)==LV_RESULT_OK); tick(); lv_obj_delete(o); tick();
    o=make(); lv_obj_delete(o); tick();
    assert(created==freed && retained==released && !lv_aic_player_pending_cleanup());
    lv_display_delete(d); assert(lv_aic_yuv_image_decoder_deinit()); assert(lv_aic_rgb_image_decoder_deinit()); lv_deinit();
    return 0;
}
