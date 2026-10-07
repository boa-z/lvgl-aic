/* SPDX-License-Identifier: Apache-2.0 */
#define AIC_LVGL_USE_PRIVATE_API 1
#include "lv_aic_player.h"
#include "lv_aic_player_control.h"
#include "lv_aic_yuv_image_private.h"
#include "lv_aic_rgb_image_private.h"
#include "lvgl_aic_private.h"
#include <assert.h>
#include <stdlib.h>
#include <string.h>
struct lv_aic_player_playback { lv_aic_playback_status_t status; unsigned readers; bool closing,preserve; };
static lv_aic_player_playback_t *active;
static unsigned created,freed,retained,released,frames;
static bool allow_exit=true,fail_prepare,rgb;
static uint8_t pixel=255;
static char last_uri[128];
typedef struct { lv_aic_player_playback_t *player; uint8_t y[16],uv[16],rgb[48]; } producer_t;
lv_aic_player_playback_t *lv_aic_player_playback_prepare(const char *uri,const lv_aic_playback_options_t *options)
{
    assert(options->cma_budget && options->extra_frames==3);
    if(active || fail_prepare) return NULL;
    strcpy(last_uri,uri); active=calloc(1,sizeof(*active)); assert(active); created++;
    active->status=(lv_aic_playback_status_t){.media_info_valid=true,.media_info={.file_size=6000000000LL,.duration=1000000,.has_video=1,.has_audio=1,.seek_able=1,.video_stream={4,4},.audio_stream={2,16,48000}},.state=LV_AIC_PLAYBACK_OPENING,.volume=-1,.has_video=true,.seekable=true,.duration_us=1000000}; return active;
}
bool lv_aic_player_playback_preserve(lv_aic_player_playback_t *p,bool enabled)
{ if(!p || p->closing) return false;p->preserve=enabled;return true; }
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
    frames--; p->status.frames_received++; p->status.frames_queued++; producer_t *data=calloc(1,sizeof(*data)); assert(data); data->player=p;
    if(rgb) {
        memset(data->rgb,pixel,sizeof(data->rgb));
        lv_aic_rgb_frame_t f={.width=4,.height=4,.stride=12,.format=LV_COLOR_FORMAT_RGB888,
            .data=data->rgb,.capacity=sizeof(data->rgb)};
        out->rgb=lv_aic_rgb_image_create(&f,retain,release,data); assert(out->rgb);
    } else {
        memset(data->y,pixel?235:16,sizeof(data->y)); memset(data->uv,128,sizeof(data->uv));
        lv_aic_yuv_frame_t f={.width=4,.height=4,.format=LV_AIC_YUV_NV16,
            .color_space=LV_AIC_YUV_BT601_LIMITED,.planes={{data->y,4,16},{data->uv,4,16}}};
        out->yuv=lv_aic_yuv_image_create(&f,retain,release,data); assert(out->yuv);
    }
    return true;
}
bool lv_aic_player_playback_seek(lv_aic_player_playback_t *p,uint64_t target)
{
    if(!p || p->closing || p->status.seek_pending || target>1000000) return false;
    p->status.seek_pending=true; p->status.seek_target_us=target; p->status.state=LV_AIC_PLAYBACK_SEEKING; return true;
}
#if defined(AIC_LVGL_USE_APNG) && AIC_LVGL_USE_APNG
struct lv_aic_apng_playback { lv_aic_apng_playback_status_t status; unsigned readers; bool closing,preserve; };
static lv_aic_apng_playback_t *png;
static unsigned png_created,png_freed,png_frames;
typedef struct { lv_aic_apng_playback_t *p; uint32_t pixels[16]; } png_frame_t;
lv_aic_apng_playback_t *lv_aic_apng_playback_prepare(const char *path,const lv_aic_apng_playback_options_t *o)
{
    assert(!png && o->snapshots==2); strcpy(last_uri,path);
    png=calloc(1,sizeof(*png)); assert(png); png_created++;
    png->status=(lv_aic_apng_playback_status_t){.state=LV_AIC_APNG_OPENING,.width=4,.height=4,
        .file_bytes=1234,.rate_num=1,.rate_den=1}; return png;
}
bool lv_aic_apng_playback_preserve(lv_aic_apng_playback_t *p,bool enabled)
{ if(!p || p->closing) return false;p->preserve=enabled;return true; }
bool lv_aic_apng_playback_start(lv_aic_apng_playback_t *p)
{ if(!p || p->closing) return false; p->status.state=LV_AIC_APNG_PLAYING; return true; }
bool lv_aic_apng_playback_pause(lv_aic_apng_playback_t *p,bool paused)
{ if(!p || p->closing) return false; p->status.state=paused?LV_AIC_APNG_PLAYBACK_PAUSED:LV_AIC_APNG_PLAYING;return true; }
bool lv_aic_apng_playback_rate(lv_aic_apng_playback_t *p,uint32_t n,uint32_t d)
{
    if(!p || p->closing || !n || !d || (uint64_t)n*10<d || n>(uint64_t)d*10) return false;
    p->status.rate_num=n;p->status.rate_den=d;return true;
}
bool lv_aic_apng_playback_restart(lv_aic_apng_playback_t *p)
{
    if(!p || p->closing || p->status.restart_pending || p->status.state==LV_AIC_APNG_OPENING) return false;
    p->status.restart_pending=true;return true;
}
lv_aic_apng_playback_status_t lv_aic_apng_playback_status(lv_aic_apng_playback_t *p)
{ return p?p->status:(lv_aic_apng_playback_status_t){.state=LV_AIC_APNG_CLOSED}; }
void lv_aic_apng_playback_close(lv_aic_apng_playback_t *p)
{ if(p) { p->closing=true;p->status.state=LV_AIC_APNG_CLOSING; } }
bool lv_aic_apng_playback_destroy(lv_aic_apng_playback_t *p)
{
    if(!p) return true;
    if(!p->closing || p->readers || !allow_exit) return false;
    assert(p==png); png=NULL; free(p); png_freed++;return true;
}
static bool png_retain(void *ctx) { png_frame_t *f=ctx;f->p->readers++;return true; }
static void png_release(void *ctx) { png_frame_t *f=ctx;assert(f->p->readers);f->p->readers--;free(f); }
bool lv_aic_apng_playback_poll(lv_aic_apng_playback_t *p,lv_aic_rgb_image_t **out,uint64_t *seq)
{
    if(!png_frames || p->closing || p->status.restart_pending) return false;
    png_frames--;p->status.composed++;p->status.published++;
    png_frame_t *f=calloc(1,sizeof(*f));assert(f);f->p=p;
    for(unsigned i=0;i<16;i++) f->pixels[i]=0xff00ff00;
    lv_aic_rgb_frame_t desc={.width=4,.height=4,.stride=16,.format=LV_COLOR_FORMAT_ARGB8888,
        .data=(const uint8_t *)f->pixels,.capacity=sizeof(f->pixels)};
    *out=lv_aic_rgb_image_create(&desc,png_retain,png_release,f);assert(*out);
    *seq=p->status.published;return true;
}
#endif
const lv_image_dsc_t *lv_aic_player_image_source(const lv_aic_player_image_t *p)
{ return p->rgb?lv_aic_rgb_image_source(p->rgb):lv_aic_yuv_image_source(p->yuv); }
void lv_aic_player_image_destroy(lv_aic_player_image_t *p)
{ if(p->rgb) lv_aic_rgb_image_destroy(p->rgb); if(p->yuv) lv_aic_yuv_image_destroy(p->yuv); *p=(lv_aic_player_image_t){0}; }
#if defined(AIC_LVGL_USE_VIDEO_PLANE) && AIC_LVGL_USE_VIDEO_PLANE
#include "lv_aic_video_plane.h"
#include <stdio.h>
struct lv_aic_video_plane { unsigned unused; };
static struct lv_aic_video_plane plane_mock;
static bool plane_live,plane_fail_present,plane_fail_close;
static lv_aic_rgb_image_t *plane_rgb[2];
static lv_aic_yuv_image_t *plane_yuv[2];
static unsigned plane_presents,plane_closes,plane_degrees;
static size_t plane_budget;
static int32_t plane_x,plane_y;static uint32_t plane_w,plane_h;
lv_aic_video_plane_t *lv_aic_video_plane_open(void)
{ if(plane_live) return NULL;plane_live=true;return &plane_mock; }
static unsigned alpha_enables;
bool lv_aic_video_plane_enable_ui_alpha(lv_aic_video_plane_t *p)
{ assert(p==&plane_mock && plane_live);alpha_enables++;return true; }
bool lv_aic_video_plane_hide(lv_aic_video_plane_t *p)
{
    assert(p==&plane_mock && plane_live);if(plane_fail_close) return false;
    for(unsigned i=0;i<2;i++) {
        if(plane_rgb[i]) lv_aic_rgb_image_release_lease(plane_rgb[i]);
        if(plane_yuv[i]) lv_aic_yuv_image_release_lease(plane_yuv[i]);
        plane_rgb[i]=NULL;plane_yuv[i]=NULL;
    }
    return true;
}
bool lv_aic_video_plane_present(lv_aic_video_plane_t *p,const void *src,int32_t x,int32_t y,uint32_t w,uint32_t h)
{
    assert(p==&plane_mock && plane_live && !plane_rgb[1] && !plane_yuv[1]);
    const lv_aic_rgb_frame_t *r;const lv_aic_yuv_frame_t *v;
    plane_rgb[1]=lv_aic_rgb_image_acquire(src,&r);
    if(!plane_rgb[1]) plane_yuv[1]=lv_aic_yuv_image_acquire(src,&v);
    assert(plane_rgb[1] || plane_yuv[1]);plane_presents++;
    plane_x=x;plane_y=y;plane_w=w;plane_h=h;
    if(plane_fail_present) return false;
    if(plane_rgb[0]) lv_aic_rgb_image_release_lease(plane_rgb[0]);
    if(plane_yuv[0]) lv_aic_yuv_image_release_lease(plane_yuv[0]);
    plane_rgb[0]=plane_rgb[1];plane_yuv[0]=plane_yuv[1];plane_rgb[1]=NULL;plane_yuv[1]=NULL;
    return true;
}
bool lv_aic_video_plane_present_rotated(lv_aic_video_plane_t *p,const void *src,
    int32_t x,int32_t y,uint32_t w,uint32_t h,unsigned degrees,size_t budget)
{
    plane_degrees=degrees;plane_budget=budget;
    return lv_aic_video_plane_present(p,src,x,y,w,h);
}
bool lv_aic_video_plane_close(lv_aic_video_plane_t *p)
{ if(!lv_aic_video_plane_hide(p)) return false;plane_live=false;plane_closes++;return true; }
bool lv_aic_video_plane_faulted(const lv_aic_video_plane_t *p) { (void)p;return plane_fail_present; }
static void *window_open(lv_fs_drv_t *d,const char *path,lv_fs_mode_t mode)
{ (void)path;(void)mode;return d; }
static lv_fs_res_t window_close(lv_fs_drv_t *d,void *f)
{ (void)d;(void)f;return LV_FS_RES_OK; }
static lv_fs_res_t window_read(lv_fs_drv_t *d,void *f,void *b,uint32_t n,uint32_t *r)
{ (void)d;(void)f;(void)b;(void)n;*r=0;return LV_FS_RES_OK; }
static lv_fs_res_t window_seek(lv_fs_drv_t *d,void *f,uint32_t n,lv_fs_whence_t w)
{ (void)d;(void)f;(void)n;(void)w;return LV_FS_RES_OK; }
static lv_fs_res_t window_tell(lv_fs_drv_t *d,void *f,uint32_t *p)
{ (void)d;(void)f;*p=0;return LV_FS_RES_OK; }
static lv_result_t window_decode(lv_image_decoder_t *d,lv_image_decoder_dsc_t *s)
{ (void)d;(void)s;return LV_RESULT_INVALID; } /* Metadata-only stub; no pixel evidence. */
static lv_result_t window_info(lv_image_decoder_t *dec,lv_image_decoder_dsc_t *dsc,lv_image_header_t *h)
{
    (void)dec;unsigned w,v;
    if(dsc->src_type!=LV_IMAGE_SRC_FILE || sscanf(dsc->src,"L:/%ux%u_0_00000000.fake",&w,&v)!=2) return LV_RESULT_INVALID;
    *h=(lv_image_header_t){.magic=LV_IMAGE_HEADER_MAGIC,.cf=LV_COLOR_FORMAT_ARGB8888,.w=w,.h=v,.stride=w*4};return LV_RESULT_OK;
}
#endif
static void tick(void) { lv_tick_inc(25); lv_timer_handler(); }
static void flush(lv_display_t *d,const lv_area_t *a,uint8_t *p) { (void)a;(void)p;lv_display_flush_ready(d); }
static void delete_on_event(lv_event_t *e) { lv_obj_delete(lv_event_get_target_obj(e)); }
static void disable_repeat_on_terminal(lv_event_t *e)
{
    lv_obj_t *o=lv_event_get_target_obj(e);
    if(lv_aic_player_get_state(o)==LV_AIC_PLAYER_TERMINAL)
        assert(lv_aic_player_set_auto_restart(o,false)==LV_RESULT_OK);
}
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
    static uint8_t pixels[16*16*4]; lv_display_t *d=lv_display_create(16,16);
    lv_display_set_color_format(d,LV_COLOR_FORMAT_RGB888);
    lv_display_set_buffers(d,pixels,NULL,sizeof(pixels),LV_DISPLAY_RENDER_MODE_DIRECT);
    lv_display_set_flush_cb(d,flush); lv_timer_pause(lv_display_get_refr_timer(d));
    lv_obj_t *screen=lv_screen_active();
    lv_obj_set_style_bg_color(screen,lv_color_black(),0);
    lv_obj_set_style_bg_opa(screen,LV_OPA_COVER,0);
    /* SDK draw-layer compatibility: NONE advances the producer without
     * publishing an image, while the UI layers use the normal image path. */
    lv_obj_t *none=make();
    lv_aic_player_set_draw_layer(none,LV_AIC_PLAYER_LAYER_NONE);
    assert(lv_aic_player_set_src(none,"none.mp4")==LV_RESULT_OK);
    assert(lv_aic_player_start(none)==LV_RESULT_OK);frames=1;tick();
    assert(!lv_image_get_src(none) && retained==released);
    assert(lv_aic_player_close(none)==LV_RESULT_OK);tick();tick();
    assert(!active && !lv_aic_player_pending_cleanup());
    lv_obj_t *o=make();
    lv_aic_player_set_draw_layer(o,LV_AIC_PLAYER_LAYER_UI_DOUBLE_BUF);
    /* Disabled or unconfigured PNG must not reach the SDK video backend. */
    assert(lv_aic_player_set_src(o,"unsupported.png")==LV_RESULT_INVALID && !active);
    fail_prepare=true;
    assert(lv_aic_player_set_src(o,"one.mp4")==LV_RESULT_INVALID); tick();
    assert(lv_aic_player_get_state(o)==LV_AIC_PLAYER_FAULT); fail_prepare=false;
    assert(lv_aic_player_set_volume(o,37)==LV_RESULT_OK);
    assert(lv_aic_player_set_src(o,"one.mp4")==LV_RESULT_OK);
    assert(active->status.volume==37); active->status.state=LV_AIC_PLAYBACK_READY; tick();
    assert(lv_aic_player_get_state(o)==LV_AIC_PLAYER_READY);
    int32_t queried_volume=-99;
    assert(lv_aic_player_control(o,LV_AIC_PLAYER_CMD_GET_VOLUME,&queried_volume)==LV_RESULT_OK && queried_volume==37);
    float rate=2;assert(lv_aic_player_control(o,LV_AIC_PLAYER_CMD_SET_PLAYBACK_RATE,&rate)==LV_RESULT_INVALID);
    assert(lv_aic_player_control(o,LV_AIC_PLAYER_CMD_GET_PLAYBACK_RATE,&rate)==LV_RESULT_OK && rate==1);
    uint64_t position=99;
    assert(lv_aic_player_control(o,LV_AIC_PLAYER_CMD_GET_PLAY_TIME,&position)==LV_RESULT_INVALID && position==99);
    active->status.position_valid=true;active->status.position_us=-1;tick();
    assert(lv_aic_player_control(o,LV_AIC_PLAYER_CMD_GET_PLAY_TIME,&position)==LV_RESULT_INVALID && position==99);
    active->status.position_us=1234;tick();
    assert(lv_aic_player_control(o,LV_AIC_PLAYER_CMD_GET_PLAY_TIME,&position)==LV_RESULT_OK && position==1234);
    lv_aic_media_info_t info;
    assert(lv_aic_player_control(o,LV_AIC_PLAYER_CMD_GET_MEDIA_INFO,&info)==LV_RESULT_OK);
    /* SDK UI_SINGLE_BUF blocks publication behind the displayed frame;
     * UI_DOUBLE_BUF keeps the latest-wins mailbox policy. */
    assert(lv_aic_player_close(o)==LV_RESULT_OK);tick();tick();
    assert(!active && !lv_aic_player_pending_cleanup());
    lv_obj_delete(o);tick();
    lv_obj_t *layer=make();
    lv_aic_player_set_draw_layer(layer,LV_AIC_PLAYER_LAYER_UI_SINGLE_BUF);
    assert(lv_aic_player_set_src(layer,"single-buffer.mp4")==LV_RESULT_OK);
    assert(lv_aic_player_start(layer)==LV_RESULT_OK);
    assert(active && active->preserve);
    assert(lv_aic_player_close(layer)==LV_RESULT_OK);tick();tick();
    assert(!active && !lv_aic_player_pending_cleanup());
    lv_obj_delete(layer);tick();
    layer=make();
    lv_aic_player_set_draw_layer(layer,LV_AIC_PLAYER_LAYER_UI_DOUBLE_BUF);
    assert(lv_aic_player_set_src(layer,"double-buffer.mp4")==LV_RESULT_OK);
    assert(lv_aic_player_start(layer)==LV_RESULT_OK);
    assert(active && !active->preserve);
    assert(lv_aic_player_close(layer)==LV_RESULT_OK);tick();tick();
    assert(!active && !lv_aic_player_pending_cleanup());
    lv_obj_delete(layer);tick();
    o=make();
    assert(lv_aic_player_set_volume(o,37)==LV_RESULT_OK);
    assert(lv_aic_player_set_src(o,"one.mp4")==LV_RESULT_OK);
    assert(lv_aic_player_start(o)==LV_RESULT_OK);
    active->status.state=LV_AIC_PLAYBACK_READY;tick();
    /* SDK-shaped void command wrappers must preserve the checked adapter's
     * playback and query semantics while remaining safe for invalid objects. */
    queried_volume=37;
    lv_aic_player_set_cmd(o,LV_AIC_PLAYER_CMD_SET_VOLUME,&queried_volume);
    queried_volume=0;
    lv_aic_player_set_cmd(o,LV_AIC_PLAYER_CMD_GET_VOLUME,&queried_volume);
    assert(queried_volume==37);
    lv_aic_player_set_cmd(NULL,LV_AIC_PLAYER_CMD_START,NULL);
    assert(info.file_size==6000000000LL && info.duration==1000000 && info.has_audio && info.seek_able);
    assert(info.video_stream.width==4 && info.audio_stream.sample_rate==48000 && info.audio_stream.nb_channel==2);
    active->status.media_info_valid=false;lv_aic_media_info_t saved=info;
    assert(lv_aic_player_get_media_info(o,&info)==LV_RESULT_INVALID && !memcmp(&info,&saved,sizeof(info)));
    active->status.media_info_valid=true;
    assert(lv_aic_player_control(o,LV_AIC_PLAYER_CMD_ATTACH_SLAVE,NULL)==LV_RESULT_INVALID);
    assert(lv_aic_player_control(o,LV_AIC_PLAYER_CMD_START,NULL)==LV_RESULT_OK); lv_obj_set_pos(o,0,0);
    for(unsigned i=0;i<4;i++) {
        rgb=(i&1)!=0; frames=1; tick(); lv_refr_now(d);
        for(unsigned y=0;y<4;y++) for(unsigned x=0;x<12;x++) assert(pixels[y*48+x]==255);
        assert(retained-released==1);
    }
    assert(lv_aic_player_seek(o,1000001)==LV_RESULT_INVALID);
    position=500000;assert(lv_aic_player_control(o,LV_AIC_PLAYER_CMD_SET_PLAY_TIME,&position)==LV_RESULT_OK);
    assert(lv_aic_player_seek(o,400000)==LV_RESULT_INVALID);
    tick(); assert(!lv_image_get_src(o) && retained==released);
    assert(lv_aic_player_get_state(o)==LV_AIC_PLAYER_SEEKING);
    active->status.seek_pending=false; active->status.seeks_completed++;
    active->status.state=LV_AIC_PLAYBACK_PLAYING; tick();
    assert(lv_aic_player_pause(o)==LV_RESULT_OK); tick(); assert(lv_aic_player_get_state(o)==LV_AIC_PLAYER_PAUSED);
    assert(lv_aic_player_resume(o)==LV_RESULT_OK); rgb=false; frames=1; tick();
    const lv_aic_yuv_frame_t *view;
    lv_aic_yuv_image_t *reader=lv_aic_yuv_image_acquire(lv_image_get_src(o),&view); assert(reader);
    assert(lv_aic_player_set_src(o,"two.mp4")==LV_RESULT_OK);
    assert(lv_aic_player_get_media_info(o,&info)==LV_RESULT_INVALID && !memcmp(&info,&saved,sizeof(info)));
    queried_volume=77;position=99;
    assert(lv_aic_player_control(o,LV_AIC_PLAYER_CMD_GET_VOLUME,&queried_volume)==LV_RESULT_INVALID && queried_volume==77);
    assert(lv_aic_player_control(o,LV_AIC_PLAYER_CMD_GET_PLAY_TIME,&position)==LV_RESULT_INVALID && position==99);
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
    /* Slaves share one immutable source/producer across independent transforms. */
    o=make(); assert(lv_aic_player_set_src(o,"shared.mp4")==LV_RESULT_OK);
    assert(lv_aic_player_control(o,LV_AIC_PLAYER_CMD_START,NULL)==LV_RESULT_OK); lv_obj_set_pos(o,0,0);
    lv_obj_t *s1=lv_aic_slave_player_create(screen),*s2=lv_aic_slave_player_create(screen);
    assert(s1 && s2 && !lv_aic_slave_player_get_master(s1));
    assert(lv_aic_player_control(o,LV_AIC_PLAYER_CMD_ATTACH_SLAVE,s1)==LV_RESULT_OK);
    assert(lv_aic_slave_player_set_master(s2,o)==LV_RESULT_OK);
    assert(lv_aic_slave_player_get_master(s1)==o);
    lv_obj_set_pos(s1,5,0); lv_obj_set_pos(s2,10,0);
    lv_image_set_pivot(s2,0,0); lv_image_set_antialias(s2,false);
    lv_image_set_scale(s2,128); /* Native transforms remain independent. */
    for(unsigned i=0;i<4;i++) {
        rgb=(i&1)!=0; pixel=(i&2)?255:0; frames=1; tick(); tick(); lv_refr_now(d);
        assert(lv_image_get_src(o)==lv_image_get_src(s1) && lv_image_get_src(o)==lv_image_get_src(s2));
        for(unsigned y=0;y<4;y++) for(unsigned x=0;x<12;x++) {
            assert(pixels[y*48+x]==pixel && pixels[y*48+15+x]==pixel);
            if(y<2 && x<6) assert(pixels[y*48+30+x]==pixel);
        }
        assert(retained-released==1); /* No extra decoder buffers for slaves. */
    }
    /* Detach during queued draw retains the source until a safe timer pass. */
    d->layer_head->draw_task_head=&pending;
    assert(lv_aic_slave_player_set_master(s1,NULL)==LV_RESULT_OK);
    tick(); assert(lv_image_get_src(s1));
    d->layer_head->draw_task_head=NULL; tick(); assert(!lv_image_get_src(s1));
    assert(lv_aic_slave_player_set_master(s1,o)==LV_RESULT_OK); tick();
    /* Last owner deletion cannot free a slave decoder's native reader. */
    const lv_aic_rgb_frame_t *rgb_view;
    lv_aic_rgb_image_t *rgb_reader=lv_aic_rgb_image_acquire(lv_image_get_src(s2),&rgb_view); assert(rgb_reader);
    lv_obj_delete(o); assert(!lv_aic_slave_player_get_master(s1) && !lv_aic_slave_player_get_master(s2));
    tick(); tick(); assert(active && !lv_image_get_src(s1) && !lv_image_get_src(s2));
    assert(rgb_view->data[0]==255 && retained-released==1);
    lv_aic_rgb_image_release_lease(rgb_reader); tick(); assert(!active);
    /* Rebind survives old-master deletion, and slave-first deletion unlinks. */
    o=make(); lv_obj_t *other=make();
    assert(lv_aic_player_set_src(o,"rebind.mp4")==LV_RESULT_OK);
    assert(lv_aic_player_start(o)==LV_RESULT_OK);
    assert(lv_aic_slave_player_set_master(s1,o)==LV_RESULT_OK);
    assert(lv_aic_slave_player_set_master(s2,o)==LV_RESULT_OK); rgb=false; frames=1; tick();
    assert(lv_aic_slave_player_set_master(s1,other)==LV_RESULT_OK);
    lv_obj_delete(s2); lv_obj_delete(o); tick(); tick();
    assert(lv_aic_slave_player_get_master(s1)==other && !lv_image_get_src(s1));
    lv_obj_delete(other); tick(); assert(!lv_aic_slave_player_get_master(s1));
    lv_obj_delete(s1); tick();
    /* Master seek/stop clears attached slaves; native readers still defer exit. */
    o=make(); s1=lv_aic_slave_player_create(screen);
    assert(lv_aic_slave_player_set_master(s1,o)==LV_RESULT_OK);
    assert(lv_aic_player_set_src(o,"seek.mp4")==LV_RESULT_OK);
    assert(lv_aic_player_start(o)==LV_RESULT_OK); frames=1; tick(); tick();
    assert(lv_image_get_src(s1)); assert(lv_aic_player_seek(o,1)==LV_RESULT_OK); tick();
    assert(!lv_image_get_src(s1));
    lv_obj_delete(o); lv_obj_delete(s1); tick(); tick();
    /* Repeat publishes terminal first, then uses normal asynchronous seek. */
    o=make(); assert(!lv_aic_player_get_auto_restart(o));
    assert(lv_aic_player_set_auto_restart(o,true)==LV_RESULT_OK);
    assert(lv_aic_player_set_src(o,"loop.mp4")==LV_RESULT_OK);
    assert(lv_aic_player_start(o)==LV_RESULT_OK);
    for(unsigned loop=0;loop<3;loop++) {
        rgb=false; frames=1; active->status.video_eos=true; active->status.state=LV_AIC_PLAYBACK_TERMINAL;
        tick(); assert(lv_aic_player_get_state(o)==LV_AIC_PLAYER_TERMINAL);
        assert(lv_aic_player_get_auto_restart_count(o)==loop);
        reader=lv_aic_yuv_image_acquire(lv_image_get_src(o),&view); assert(reader);
        tick(); assert(lv_aic_player_get_state(o)==LV_AIC_PLAYER_SEEKING && !lv_image_get_src(o));
        assert(lv_aic_player_get_auto_restart_count(o)==loop+1 && active->status.seek_target_us==0);
        assert(view->planes[0].data[0]==235); lv_aic_yuv_image_release_lease(reader);
        active->status.seek_pending=false; active->status.seeks_completed++; active->status.video_eos=false;
        active->status.state=LV_AIC_PLAYBACK_PLAYING; tick();
    }
    /* Neither a terminal without EOS nor one without fresh frames can loop. */
    active->status.state=LV_AIC_PLAYBACK_TERMINAL; tick(); tick();
    assert(lv_aic_player_get_auto_restart_count(o)==3);
    active->status.video_eos=true; tick(); assert(lv_aic_player_get_auto_restart_count(o)==3);
    active->status.seekable=false; frames=1; tick(); tick();
    assert(lv_aic_player_get_auto_restart_count(o)==3); active->status.seekable=true;
    /* Applications can disable repeat in the terminal event before restart. */
    active->status.state=LV_AIC_PLAYBACK_PLAYING; tick();
    lv_obj_add_event_cb(o,disable_repeat_on_terminal,LV_EVENT_VALUE_CHANGED,NULL);
    active->status.state=LV_AIC_PLAYBACK_TERMINAL; frames=1; tick(); tick();
    assert(!lv_aic_player_get_auto_restart(o) && lv_aic_player_get_auto_restart_count(o)==3);
    lv_obj_delete(o); tick();
    /* Audio-only terminal needs a timestamp; default/no-progress never loops. */
    o=make(); assert(lv_aic_player_set_src(o,"audio.mp3")==LV_RESULT_OK);
    assert(lv_aic_player_set_auto_restart(o,true)==LV_RESULT_OK);
    assert(lv_aic_player_start(o)==LV_RESULT_OK);
    active->status.has_video=false; active->status.has_audio=true;
    active->status.state=LV_AIC_PLAYBACK_TERMINAL; tick(); tick();
    assert(!lv_aic_player_get_auto_restart_count(o)); active->status.position_valid=true;
    tick(); assert(lv_aic_player_get_auto_restart_count(o)==1);
    /* Disabling does not mutate the already accepted seek transaction. */
    assert(lv_aic_player_set_auto_restart(o,false)==LV_RESULT_OK && active->status.seek_pending);
    lv_obj_delete(o); tick();
    /* Terminal callback deletion remains safe even with repeat enabled. */
    o=make(); assert(lv_aic_player_set_src(o,"delete.mp4")==LV_RESULT_OK);
    assert(lv_aic_player_set_auto_restart(o,true)==LV_RESULT_OK);
    active->status.state=LV_AIC_PLAYBACK_TERMINAL; active->status.video_eos=true; frames=1;
    lv_obj_add_event_cb(o,delete_on_event,LV_EVENT_VALUE_CHANGED,NULL); tick(); tick();
    assert(created==freed && retained==released && !lv_aic_player_pending_cleanup());
#if defined(AIC_LVGL_USE_APNG) && AIC_LVGL_USE_APNG
    /* One image object and slave survive video -> PNG -> video, with readers
     * deliberately held across both hand-offs and pending replacement updates. */
    o=make(); s1=lv_aic_slave_player_create(screen);
    lv_aic_apng_playback_options_t po={.limits={4096,4096,64,8},.stream_budget=8192,
        .snapshot_budget=4096,.cma_budget=4096,.packet_limit=4096,.snapshots=2,.minimum_delay_us=1000};
    assert(lv_aic_player_configure_apng(o,&po)==LV_RESULT_OK);
    assert(lv_aic_slave_player_set_master(s1,o)==LV_RESULT_OK);
    lv_image_set_scale(o,128);lv_image_set_rotation(s1,900);
    assert(lv_aic_player_set_src(o,"switch.mp4")==LV_RESULT_OK);
    assert(lv_aic_player_start(o)==LV_RESULT_OK);rgb=false;frames=1;tick();
    reader=lv_aic_yuv_image_acquire(lv_image_get_src(o),&view);assert(reader);
    assert(lv_aic_player_set_src(o,"first.png")==LV_RESULT_OK);
    assert(lv_aic_player_set_src(o,"latest.apng")==LV_RESULT_OK);
    assert(lv_aic_player_start(o)==LV_RESULT_OK);tick();
    assert(active && !png && !lv_image_get_src(s1));
    lv_aic_yuv_image_release_lease(reader);tick();tick();
    assert(!active && png && !strcmp(last_uri,"latest.apng"));
    assert(lv_aic_slave_player_get_master(s1)==o);
    assert(lv_image_get_scale_x(o)==128 && lv_image_get_rotation(s1)==900);
    png_frames=1;tick();tick();
    assert(lv_image_get_src(o) && lv_image_get_src(o)==lv_image_get_src(s1));
    assert(lv_aic_player_get_media_info(o,&info)==LV_RESULT_OK);
    assert(info.file_size==1234 && info.has_video && !info.has_audio && !info.seek_able && info.video_stream.width==4);
    assert(lv_aic_player_set_volume(o,50)==LV_RESULT_INVALID);
    queried_volume=77;
    assert(lv_aic_player_control(o,LV_AIC_PLAYER_CMD_GET_VOLUME,&queried_volume)==LV_RESULT_INVALID && queried_volume==77);
    rate=2.5f;
    assert(lv_aic_player_control(o,LV_AIC_PLAYER_CMD_SET_PLAYBACK_RATE,&rate)==LV_RESULT_OK);
    rate=0;assert(lv_aic_player_control(o,LV_AIC_PLAYER_CMD_GET_PLAYBACK_RATE,&rate)==LV_RESULT_OK && rate==2.5f);
    assert(lv_aic_player_set_rate(o,0,1)==LV_RESULT_INVALID);
    assert(lv_aic_player_seek(o,1)==LV_RESULT_INVALID);
    assert(lv_aic_player_pause(o)==LV_RESULT_OK);tick();
    assert(lv_aic_player_seek(o,0)==LV_RESULT_OK);tick();
    assert(lv_aic_player_get_state(o)==LV_AIC_PLAYER_SEEKING && !lv_image_get_src(s1));
    assert(lv_aic_player_get_media_info(o,&info)==LV_RESULT_INVALID);
    png->status.restart_pending=false;png->status.restarts++;tick();
    assert(lv_aic_player_get_state(o)==LV_AIC_PLAYER_PAUSED);
    assert(lv_aic_player_resume(o)==LV_RESULT_OK);png_frames=1;tick();
    assert(lv_aic_player_set_auto_restart(o,true)==LV_RESULT_OK);
    png->status.state=LV_AIC_APNG_TERMINAL;tick();tick();
    assert(png->status.restart_pending && lv_aic_player_get_auto_restart_count(o)==1);
    png->status.restart_pending=false;png->status.restarts++;png->status.state=LV_AIC_APNG_PLAYING;
    png_frames=1;tick();tick();
    rgb_reader=lv_aic_rgb_image_acquire(lv_image_get_src(s1),&rgb_view);assert(rgb_reader);
    assert(lv_aic_player_set_src(o,"return.mp4")==LV_RESULT_OK);
    rate=9;assert(lv_aic_player_control(o,LV_AIC_PLAYER_CMD_GET_PLAYBACK_RATE,&rate)==LV_RESULT_INVALID && rate==9);
    assert(lv_aic_player_start(o)==LV_RESULT_OK);tick();
    assert(png && !active && rgb_view->data[1]==255);
    lv_aic_rgb_image_release_lease(rgb_reader);tick();tick();
    assert(active && !png && !strcmp(last_uri,"return.mp4"));
    rgb=true;frames=1;tick();tick();assert(lv_image_get_src(o)==lv_image_get_src(s1));
    lv_obj_delete(o);lv_obj_delete(s1);tick();tick();
    /* SDK-style group: independent video and PNG producers share a publication
     * barrier, not frame pixels. The faster producer cannot race the slower. */
    lv_obj_t *g=lv_aic_player_group_create(screen),*g2=lv_aic_player_group_create(screen);
    assert(g && g2);size_t accepted=99;
    assert(lv_aic_player_group_control(g,LV_AIC_PLAYER_CMD_START,NULL,&accepted)==LV_RESULT_INVALID && !accepted);
    o=make();other=lv_aic_player_create(screen);
    assert(lv_aic_player_configure_apng(other,&po)==LV_RESULT_OK);
    assert(lv_aic_player_group_add(g,o)==LV_RESULT_OK);
    assert(lv_aic_player_group_add(g,o)==LV_RESULT_OK && lv_aic_player_group_get_count(g)==1);
    assert(lv_aic_player_control(other,LV_AIC_PLAYER_CMD_ATTACH_GROUP,g)==LV_RESULT_OK);
    assert(lv_aic_player_get_group(other)==g && lv_aic_player_group_get_count(g)==2);
    assert(lv_aic_player_set_src(o,"group.mp4")==LV_RESULT_OK);
    assert(lv_aic_player_set_src(other,"group.png")==LV_RESULT_OK);
    assert(lv_aic_player_group_control(g,LV_AIC_PLAYER_CMD_START,NULL,&accepted)==LV_RESULT_OK && accepted==2);
    assert(active->preserve && png->preserve);
    lv_aic_player_group_set_cmd(g,LV_AIC_PLAYER_CMD_PAUSE,NULL);
    /* The SDK-shaped wrapper keeps the void ABI, while control completion is
     * observed on the owner timer just like the checked adapter. */
    tick();
    assert(lv_aic_player_get_state(o)==LV_AIC_PLAYER_PAUSED &&
           lv_aic_player_get_state(other)==LV_AIC_PLAYER_PAUSED);
    lv_aic_player_group_set_cmd(NULL,LV_AIC_PLAYER_CMD_START,NULL);
    lv_aic_player_group_set_cmd(g,LV_AIC_PLAYER_CMD_RESUME,NULL);
    frames=20;png_frames=0;rgb=false;
    tick();tick();tick();
    assert(active->status.frames_queued==1 && !png->status.published);
    /* Automatic repeat may not let a fast terminal member bypass a slow one. */
    assert(lv_aic_player_set_auto_restart(o,true)==LV_RESULT_OK);
    active->status.state=LV_AIC_PLAYBACK_TERMINAL;active->status.video_eos=true;
    tick();tick();assert(!lv_aic_player_get_auto_restart_count(o) && !active->status.seek_pending);
    assert(lv_aic_player_set_auto_restart(o,false)==LV_RESULT_OK);
    active->status.state=LV_AIC_PLAYBACK_PLAYING;active->status.video_eos=false;
    png_frames=1;tick();tick();tick();
    assert(active->status.frames_queued==2 && png->status.published==1);
    assert(lv_image_get_src(o)!=lv_image_get_src(other)); /* Independent source ownership. */
    rate=2;
    assert(lv_aic_player_group_control(g,LV_AIC_PLAYER_CMD_SET_PLAYBACK_RATE,&rate,&accepted)==LV_RESULT_INVALID && accepted==1);
    assert(png->status.rate_num==2);
    queried_volume=123;
    assert(lv_aic_player_group_control(g,LV_AIC_PLAYER_CMD_GET_VOLUME,&queried_volume,&accepted)==LV_RESULT_INVALID);
    assert(!accepted && queried_volume==123);
    position=1;
    assert(lv_aic_player_group_control(g,LV_AIC_PLAYER_CMD_SET_PLAY_TIME,&position,&accepted)==LV_RESULT_INVALID && !accepted);
    assert(lv_aic_player_seek(o,1)==LV_RESULT_INVALID);
    assert(lv_aic_player_group_control(g,LV_AIC_PLAYER_CMD_PAUSE,NULL,&accepted)==LV_RESULT_OK && accepted==2);
    tick();assert(lv_aic_player_get_state(o)==LV_AIC_PLAYER_PAUSED && lv_aic_player_get_state(other)==LV_AIC_PLAYER_PAUSED);
    position=0;
    assert(lv_aic_player_group_control(g,LV_AIC_PLAYER_CMD_SET_PLAY_TIME,&position,&accepted)==LV_RESULT_OK && accepted==2);
    tick();assert(!lv_image_get_src(o) && !lv_image_get_src(other));
    active->status.seek_pending=false;active->status.seeks_completed++;active->status.state=LV_AIC_PLAYBACK_PAUSED;
    png->status.restart_pending=false;png->status.restarts++;tick();
    assert(lv_aic_player_group_control(g,LV_AIC_PLAYER_CMD_RESUME,NULL,&accepted)==LV_RESULT_OK && accepted==2);
    tick();tick();assert(active->status.frames_queued==3);
    /* Reassignment removes old membership and releases its stalled barrier. */
    assert(lv_aic_player_group_add(g2,other)==LV_RESULT_OK);
    assert(lv_aic_player_group_get_count(g)==1 && lv_aic_player_group_get_count(g2)==1);
    unsigned before=active->status.frames_queued;tick();tick();assert(active->status.frames_queued>before);
    assert(lv_aic_player_group_remove(g,other)==LV_RESULT_INVALID);
    assert(lv_aic_player_control(other,LV_AIC_PLAYER_CMD_ATTACH_GROUP,NULL)==LV_RESULT_OK);
    assert(!lv_aic_player_get_group(other) && !lv_aic_player_group_get_count(g2));
    assert(lv_aic_player_group_add(g,other)==LV_RESULT_OK);
    /* Deleting a group only detaches, preserving its independent producers. */
    lv_obj_delete(g);
    assert(!lv_aic_player_get_group(o) && !lv_aic_player_get_group(other) && active && png);
    assert(!active->preserve && !png->preserve);
    assert(lv_aic_player_group_add(g2,o)==LV_RESULT_OK);
    assert(lv_aic_player_group_add(g2,other)==LV_RESULT_OK);
    lv_obj_delete(other);assert(lv_aic_player_group_get_count(g2)==1);tick();
    lv_obj_delete(o);assert(!lv_aic_player_group_get_count(g2));tick();lv_obj_delete(g2);
    frames=0;
    assert(!active && !png && !lv_aic_player_pending_cleanup());
    /* Parent group deletion invokes member and group destructors safely. */
    g=lv_aic_player_group_create(screen);o=lv_aic_player_create(g);
    assert(lv_aic_player_group_add(g,o)==LV_RESULT_OK);lv_obj_delete(g);tick();
    /* APNG can be configured without media budgets; deletion waits native readers. */
    o=lv_aic_player_create(screen);assert(lv_aic_player_configure_apng(o,&po)==LV_RESULT_OK);
    assert(lv_aic_player_set_src(o,"static.png")==LV_RESULT_OK);
    assert(lv_aic_player_start(o)==LV_RESULT_OK);png_frames=1;tick();
    rgb_reader=lv_aic_rgb_image_acquire(lv_image_get_src(o),&rgb_view);assert(rgb_reader);
    lv_obj_delete(o);tick();assert(png && lv_aic_player_pending_cleanup());
    lv_aic_rgb_image_release_lease(rgb_reader);tick();
    assert(png_created==png_freed && created==freed && retained==released && !lv_aic_player_pending_cleanup());
#endif
#if defined(AIC_LVGL_USE_VIDEO_PLANE) && AIC_LVGL_USE_VIDEO_PLANE
    static lv_fs_drv_t window_fs;lv_fs_drv_init(&window_fs);window_fs.letter='L';
    window_fs.open_cb=window_open;window_fs.close_cb=window_close;window_fs.read_cb=window_read;
    window_fs.seek_cb=window_seek;window_fs.tell_cb=window_tell;lv_fs_drv_register(&window_fs);
    lv_image_decoder_t *window_decoder=lv_image_decoder_create();assert(window_decoder);
    lv_image_decoder_set_info_cb(window_decoder,window_info);lv_image_decoder_set_open_cb(window_decoder,window_decode);
    o=make();assert(lv_aic_player_set_video_plane(o,true)==LV_RESULT_INVALID);lv_obj_delete(o);tick();
    lv_display_set_color_format(d,LV_COLOR_FORMAT_ARGB8888);
    lv_display_set_buffers(d,pixels,NULL,sizeof(pixels),LV_DISPLAY_RENDER_MODE_DIRECT);
    lv_timer_pause(lv_display_get_refr_timer(d));
    o=make();lv_obj_set_size(o,4,4);lv_obj_set_pos(o,1,2);
    lv_aic_player_set_draw_layer(o,LV_AIC_PLAYER_LAYER_VIDEO);
    assert(lv_aic_player_set_video_plane_rotation_budget(o,8192)==LV_RESULT_OK);
    assert(lv_aic_player_set_src(o,"plane.mp4")==LV_RESULT_OK);
    assert(lv_aic_player_set_video_plane(o,false)==LV_RESULT_INVALID);
    assert(lv_aic_player_start(o)==LV_RESULT_OK);rgb=true;frames=1;tick();
    assert(plane_live && plane_presents && plane_x==1 && plane_y==2 && plane_w==4 && plane_h==4);
    assert(lv_image_src_get_type(lv_image_get_src(o))==LV_IMAGE_SRC_FILE);
    s1=lv_aic_slave_player_create(screen);assert(lv_aic_slave_player_set_master(s1,o)==LV_RESULT_OK);tick();
    assert(lv_image_src_get_type(lv_image_get_src(s1))==LV_IMAGE_SRC_VARIABLE);
    unsigned presented=plane_presents;tick();assert(plane_presents==presented);
    lv_obj_set_pos(o,3,4);tick();assert(plane_x==3 && plane_y==4 && plane_presents==presented+1);
    lv_obj_set_hidden(o,true);tick();assert(!plane_rgb[0]);
    lv_obj_set_hidden(o,false);tick();assert(plane_rgb[0]);
    assert(lv_aic_player_pause(o)==LV_RESULT_OK);tick();lv_obj_set_size(o,6,5);tick();
    assert(plane_w==6 && plane_h==5 && lv_aic_player_get_state(o)==LV_AIC_PLAYER_PAUSED);
    assert(lv_aic_player_set_video_plane_rotation_budget(o,16384)==LV_RESULT_INVALID);
    /* Pixel pivot (2,1), non-square 6x5 image at (3,4): clockwise bounds. */
    lv_image_set_pivot(o,2,1);
    static const int32_t image_bounds[3][4]={{2,3,5,6},{2,2,6,5},{4,2,5,6}};
    for(unsigned angle=1;angle<=3;angle++) {
        lv_image_set_rotation(o,angle*900);tick();
        assert(plane_degrees==angle*90);
        assert(plane_x==image_bounds[angle-1][0] && plane_y==image_bounds[angle-1][1]);
        assert(plane_w==(uint32_t)image_bounds[angle-1][2] && plane_h==(uint32_t)image_bounds[angle-1][3]);
        assert(lv_aic_player_get_state(o)==LV_AIC_PLAYER_PAUSED);
    }
    /* Negative right-angle values use LVGL's signed 0.1-degree API and must
     * normalize to the same clockwise DE rotations as positive values. */
    for(int angle=-1;angle>=-3;angle--) {
        lv_image_set_rotation(o,angle*900);tick();
        assert(plane_degrees==(unsigned)((360+angle*90)%360));
        assert(lv_aic_player_get_state(o)==LV_AIC_PLAYER_PAUSED);
    }
    lv_image_set_pivot(o,LV_PCT(50),LV_PCT(50));lv_image_set_rotation(o,900);tick();
    assert(plane_x==3 && plane_y==4 && plane_w==5 && plane_h==6);
    lv_obj_set_size(o,8,5);tick();
    assert(plane_x==4 && plane_y==3 && plane_w==5 && plane_h==8);
    lv_obj_set_size(o,6,5);lv_image_set_pivot(o,2,1);tick();
    lv_image_set_rotation(o,900);lv_image_set_offset_x(o,2);lv_image_set_offset_y(o,1);tick();
    assert(plane_x==4 && plane_y==4 && plane_w==5 && plane_h==6);
    lv_image_set_offset_x(o,0);lv_image_set_offset_y(o,0);tick();
    lv_image_set_rotation(o,900);lv_display_set_rotation(d,LV_DISPLAY_ROTATION_90);
    lv_timer_pause(lv_display_get_refr_timer(d));tick();
    assert(plane_degrees==0 && plane_x==3 && plane_y==9 && plane_w==6 && plane_h==5);
    lv_display_set_rotation(d,LV_DISPLAY_ROTATION_0);lv_timer_pause(lv_display_get_refr_timer(d));
    lv_image_set_rotation(o,0);tick();
    lv_display_set_resolution(d,16,12);lv_timer_pause(lv_display_get_refr_timer(d));
    for(unsigned rotation=1;rotation<=3;rotation++) {
        lv_display_set_rotation(d,(lv_display_rotation_t)rotation);lv_timer_pause(lv_display_get_refr_timer(d));tick();
        assert(plane_degrees==360-rotation*90 && plane_budget==8192);
        assert(plane_w==(rotation==2?6:5) && plane_h==(rotation==2?5:6));
        assert(plane_x==(rotation==1?4:rotation==2?7:7));
        assert(plane_y==(rotation==1?3:rotation==2?3:3));
        assert(lv_aic_player_get_state(o)==LV_AIC_PLAYER_PAUSED);
    }
    lv_display_set_rotation(d,LV_DISPLAY_ROTATION_0);tick();assert(plane_degrees==0);
    lv_display_set_resolution(d,16,16);lv_timer_pause(lv_display_get_refr_timer(d));
    lv_obj_delete(s1);tick();
    /* Seek cannot retire the old epoch until scanout has been disabled. */
    plane_fail_close=true;assert(lv_aic_player_seek(o,100)==LV_RESULT_OK);tick();assert(plane_live && active->readers);
    plane_fail_close=false;tick();assert(!plane_live && !active->readers);
    active->status.seek_pending=false;active->status.state=LV_AIC_PLAYBACK_PLAYING;frames=1;tick();assert(plane_live);
    /* Failed close holds the backend and orphan until DMA is no longer live. */
    plane_fail_close=true;lv_obj_delete(o);tick();
    assert(active && lv_aic_player_pending_cleanup() && plane_live);
    plane_fail_close=false;tick();assert(!active && !plane_live && !lv_aic_player_pending_cleanup());
    o=make();assert(lv_aic_player_set_video_plane(o,true)==LV_RESULT_OK);
    assert(lv_aic_player_set_src(o,"plane.mp4")==LV_RESULT_OK);assert(lv_aic_player_start(o)==LV_RESULT_OK);
    frames=1;tick();assert(plane_live);
    plane_fail_close=true;assert(lv_aic_player_set_src(o,"replacement.mp4")==LV_RESULT_OK);tick();
    assert(!strcmp(last_uri,"plane.mp4"));plane_fail_close=false;tick();assert(!strcmp(last_uri,"replacement.mp4"));
    assert(lv_aic_player_start(o)==LV_RESULT_OK);frames=1;tick();assert(plane_live);
    lv_image_set_rotation(o,100);tick();assert(lv_aic_player_get_state(o)==LV_AIC_PLAYER_FAULT && !plane_live);
    lv_obj_delete(o);tick();assert(!active);
    o=make();assert(lv_aic_player_set_video_plane(o,true)==LV_RESULT_OK);
    assert(lv_aic_player_set_src(o,"fault.mp4")==LV_RESULT_OK);assert(lv_aic_player_start(o)==LV_RESULT_OK);
    plane_fail_present=plane_fail_close=true;frames=1;tick();
    assert(lv_aic_player_get_state(o)==LV_AIC_PLAYER_FAULT && plane_live);
    lv_obj_delete(o);tick();assert(active && lv_aic_player_pending_cleanup());
    plane_fail_present=plane_fail_close=false;tick();assert(!active && !plane_live);
    assert(created==freed && retained==released && !lv_aic_player_pending_cleanup());
    o=make();assert(lv_aic_player_set_video_plane(o,true)==LV_RESULT_OK);
    assert(lv_aic_player_set_src(o,"offset.mp4")==LV_RESULT_OK);assert(lv_aic_player_start(o)==LV_RESULT_OK);
    lv_display_set_offset(d,1,0);frames=1;tick();
    assert(lv_aic_player_get_state(o)==LV_AIC_PLAYER_FAULT && !plane_live);
    lv_obj_delete(o);tick();lv_display_set_offset(d,0,0);
    assert(created==freed && retained==released && !lv_aic_player_pending_cleanup());
    /* Every display orientation rejects real offsets before acquiring UI alpha.
     * Missing rotation budget also rejects without changing alpha/scanout. */
    for(unsigned rotation=1;rotation<=3;rotation++) {
        lv_display_set_rotation(d,(lv_display_rotation_t)rotation);
        lv_timer_pause(lv_display_get_refr_timer(d));
        for(unsigned scenario=0;scenario<3;scenario++) {
            o=make();assert(lv_aic_player_set_video_plane(o,true)==LV_RESULT_OK);
            if(scenario) assert(lv_aic_player_set_video_plane_rotation_budget(o,8192)==LV_RESULT_OK);
            lv_display_set_offset(d,scenario==1,scenario==2);
            assert(lv_aic_player_set_src(o,"reject.mp4")==LV_RESULT_OK);
            assert(lv_aic_player_start(o)==LV_RESULT_OK);
            unsigned before=alpha_enables;frames=1;tick();
            assert(lv_aic_player_get_state(o)==LV_AIC_PLAYER_FAULT && !plane_live && alpha_enables==before);
            lv_obj_delete(o);tick();lv_display_set_offset(d,0,0);
            assert(!active && created==freed && retained==released);
        }
    }
    lv_display_set_rotation(d,LV_DISPLAY_ROTATION_0);lv_timer_pause(lv_display_get_refr_timer(d));
    /* Original object fits; rotated window crosses the root clip, or pivot
     * exceeds the fake draw path limit. Neither may acquire UI alpha. */
    for(unsigned scenario=0;scenario<2;scenario++) {
        o=make();lv_obj_set_size(o,6,5);lv_obj_set_pos(o,1,1);
        lv_image_set_pivot(o,scenario?4097:0,0);lv_image_set_rotation(o,900);
        assert(lv_aic_player_set_video_plane(o,true)==LV_RESULT_OK);
        assert(lv_aic_player_set_video_plane_rotation_budget(o,8192)==LV_RESULT_OK);
        assert(lv_aic_player_set_src(o,"clip.mp4")==LV_RESULT_OK);
        assert(lv_aic_player_start(o)==LV_RESULT_OK);
        unsigned before=alpha_enables,submitted=plane_presents;frames=1;tick();
        assert(lv_aic_player_get_state(o)==LV_AIC_PLAYER_FAULT);
        assert(!plane_live && alpha_enables==before && plane_presents==submitted);
        lv_obj_delete(o);tick();assert(!active && created==freed && retained==released);
    }
    /* A single DE rectangle cannot reproduce a bitmap mask. Tile+offset also
     * has LVGL clipping semantics outside this plane profile. */
    static const uint8_t mask_pixels[24]={0};
    static const lv_image_dsc_t mask={.header={.magic=LV_IMAGE_HEADER_MAGIC,
        .cf=LV_COLOR_FORMAT_A8,.w=6,.h=4,.stride=6},.data_size=24,.data=mask_pixels};
    for(unsigned scenario=0;scenario<2;scenario++) {
        o=make();lv_obj_set_size(o,6,4);lv_obj_set_pos(o,2,2);
        if(scenario) { lv_image_set_inner_align(o,LV_IMAGE_ALIGN_TILE);lv_image_set_offset_x(o,1); }
        else lv_image_set_bitmap_map_src(o,&mask);
        assert(lv_aic_player_set_video_plane(o,true)==LV_RESULT_OK);
        assert(lv_aic_player_set_src(o,"unsupported-geometry.mp4")==LV_RESULT_OK);
        assert(lv_aic_player_start(o)==LV_RESULT_OK);
        unsigned before=alpha_enables,submitted=plane_presents;frames=1;tick();
        assert(lv_aic_player_get_state(o)==LV_AIC_PLAYER_FAULT);
        assert(!plane_live && alpha_enables==before && plane_presents==submitted);
        lv_obj_delete(o);tick();assert(!active && created==freed && retained==released);
    }
    /* Native scanout displays a fake window whose size is not the decoded
     * frame size. Deferred sizing must use frame metadata, not that window. */
    o=make();lv_obj_set_pos(o,0,0);lv_obj_set_size(o,6,5);
    assert(lv_aic_player_set_video_plane(o,true)==LV_RESULT_OK);
    assert(lv_aic_player_set_src(o,"window-sized.mp4")==LV_RESULT_OK);
    assert(lv_aic_player_start(o)==LV_RESULT_OK);rgb=true;frames=1;tick();
    assert(plane_live && plane_w==6 && plane_h==5);
    assert(lv_aic_player_set_width(o,4)==LV_RESULT_OK);
    assert(lv_aic_player_set_height(o,4)==LV_RESULT_OK);
    assert(lv_image_get_scale_x(o)==256 && lv_image_get_scale_y(o)==256);
    tick();assert(plane_live && plane_w==4 && plane_h==4);
    assert(lv_aic_player_pause(o)==LV_RESULT_OK);tick();
    assert(lv_aic_player_set_width(o,8)==LV_RESULT_OK);
    assert(lv_aic_player_set_height(o,6)==LV_RESULT_OK);
    tick();assert(plane_live && plane_w==8 && plane_h==6);
    assert(lv_aic_player_get_state(o)!=LV_AIC_PLAYER_FAULT);
    assert(lv_image_get_scale_x(o)==256 && lv_image_get_scale_y(o)==256);
    lv_obj_delete(o);tick();assert(!plane_live && !active);
    /* Pre-open destination requests use DE scaling on the first frame. */
    o=make();lv_obj_set_pos(o,0,0);
    assert(lv_aic_player_set_video_plane(o,true)==LV_RESULT_OK);
    assert(lv_aic_player_set_width(o,8)==LV_RESULT_OK);
    assert(lv_aic_player_set_height(o,6)==LV_RESULT_OK);
    assert(lv_aic_player_set_src(o,"plane-sized.mp4")==LV_RESULT_OK);
    assert(lv_aic_player_start(o)==LV_RESULT_OK);frames=1;tick();
    assert(plane_live && plane_w==8 && plane_h==6);
    assert(lv_image_get_scale_x(o)==256 && lv_image_get_scale_y(o)==256);
    lv_obj_delete(o);tick();assert(!plane_live && !active);
    /* Top/system/bottom layers are visible roots, unlike inactive screens. */
    lv_obj_t *roots[]={lv_display_get_layer_top(d),lv_display_get_layer_sys(d),lv_display_get_layer_bottom(d)};
    for(unsigned i=0;i<3;i++) {
        o=make();lv_obj_set_parent(o,roots[i]);
        assert(lv_aic_player_set_video_plane(o,true)==LV_RESULT_OK);
        assert(lv_aic_player_set_src(o,"overlay.mp4")==LV_RESULT_OK);assert(lv_aic_player_start(o)==LV_RESULT_OK);
        unsigned before=alpha_enables;frames=1;tick();assert(plane_live && alpha_enables>before);
        lv_obj_set_hidden(roots[i],true);tick();assert(!plane_rgb[0]);
        lv_obj_set_hidden(roots[i],false);tick();assert(plane_rgb[0]);
        lv_obj_delete(o);tick();assert(!plane_live && !active);
    }
    lv_obj_t *inactive=lv_obj_create(NULL);o=make();lv_obj_set_parent(o,inactive);
    assert(lv_aic_player_set_video_plane(o,true)==LV_RESULT_OK);
    assert(lv_aic_player_set_src(o,"inactive.mp4")==LV_RESULT_OK);assert(lv_aic_player_start(o)==LV_RESULT_OK);
    frames=1;tick();assert(!plane_live);lv_obj_delete(inactive);tick();assert(!active);
    assert(created==freed && retained==released && !lv_aic_player_pending_cleanup());
    lv_image_decoder_delete(window_decoder);
#endif
    /* Size requests made before open apply on the first decoded frame and
     * persist across source replacement. Manual scaling cancels both axes. */
    o=make();
    assert(lv_aic_player_set_width(o,8)==LV_RESULT_OK);
    assert(lv_aic_player_set_height(o,12)==LV_RESULT_OK);
    lv_obj_update_layout(o);
    assert(lv_obj_get_width(o)==8 && lv_obj_get_height(o)==12);
    assert(lv_image_get_scale_x(o)==LV_SCALE_NONE && lv_image_get_scale_y(o)==LV_SCALE_NONE);
    assert(lv_aic_player_set_width(o,0)==LV_RESULT_INVALID);
    assert(lv_aic_player_set_height(o,4097)==LV_RESULT_INVALID);
    assert(lv_aic_player_set_src(o,"sized.mp4")==LV_RESULT_OK);
    assert(lv_aic_player_start(o)==LV_RESULT_OK);frames=1;tick();
    lv_obj_update_layout(o);
    assert(lv_obj_get_width(o)==8 && lv_obj_get_height(o)==12);
    assert(lv_image_get_scale_x(o)==512 && lv_image_get_scale_y(o)==768);
    assert(lv_aic_player_set_width(o,6)==LV_RESULT_OK);lv_obj_update_layout(o);
    assert(lv_obj_get_width(o)==6 && lv_image_get_scale_x(o)==384);
    assert(lv_aic_player_set_src(o,"resized.mp4")==LV_RESULT_OK);
    assert(lv_aic_player_start(o)==LV_RESULT_OK);tick();frames=1;tick();
    assert(lv_image_get_scale_x(o)==384 && lv_image_get_scale_y(o)==768);
    lv_aic_player_set_scale_x(o,256);lv_image_set_scale_y(o,64);frames=1;tick();
    assert(lv_image_get_scale_x(o)==256 && lv_image_get_scale_y(o)==64);
    lv_obj_delete(o);tick();assert(!active);
#if defined(AIC_LVGL_USE_APNG) && AIC_LVGL_USE_APNG
    o=make();assert(lv_aic_player_configure_apng(o,&po)==LV_RESULT_OK);
    assert(lv_aic_player_set_width(o,12)==LV_RESULT_OK);
    assert(lv_aic_player_set_height(o,8)==LV_RESULT_OK);
    assert(lv_aic_player_set_src(o,"sized.apng")==LV_RESULT_OK);
    assert(lv_aic_player_start(o)==LV_RESULT_OK);png_frames=1;tick();tick();
    lv_obj_update_layout(o);
    assert(lv_obj_get_width(o)==12 && lv_obj_get_height(o)==8);
    assert(lv_image_get_scale_x(o)==768 && lv_image_get_scale_y(o)==512);
    lv_obj_delete(o);tick();assert(!png);
#endif
    /* SDK-named transforms use the same native image state on both classes. */
    static const uint8_t transform_pixels[32*16*4]={0};
    lv_image_dsc_t transform_image={.header={.magic=LV_IMAGE_HEADER_MAGIC,
        .cf=LV_COLOR_FORMAT_ARGB8888,.w=32,.h=16,.stride=128},
        .data_size=sizeof(transform_pixels),.data=transform_pixels};
    for(unsigned slave=0;slave<2;slave++) {
        lv_obj_t *image=slave?lv_aic_slave_player_create(screen):lv_aic_player_create(screen);
        assert(image);lv_image_set_src(image,&transform_image);
        lv_aic_player_set_pivot(image,7,5);lv_point_t pivot;
        lv_aic_player_get_pivot(image,&pivot);assert(pivot.x==7 && pivot.y==5);
        lv_aic_player_set_rotation(image,900);assert(lv_aic_player_get_rotation(image)==900);
        lv_aic_player_set_scale(image,384);
        assert(lv_aic_player_get_scale_x(image)==384 && lv_aic_player_get_scale_y(image)==384);
        lv_aic_player_set_scale_x(image,512);lv_aic_player_set_scale_y(image,128);
        assert(lv_aic_player_get_scale(image)==512 && lv_image_get_scale_x(image)==512);
        assert(lv_aic_player_get_scale_y(image)==128);
        lv_aic_player_set_inner_align(image,LV_IMAGE_ALIGN_TILE);
        assert(lv_aic_player_get_inner_align(image)==LV_IMAGE_ALIGN_TILE);
        lv_aic_player_set_offset_x(image,3);lv_aic_player_set_offset_y(image,4);
        assert(lv_aic_player_get_offset_x(image)==3 && lv_aic_player_get_offset_y(image)==4);
        lv_obj_delete(image);tick();
    }
    assert(!lv_aic_player_pending_cleanup());
    lv_display_delete(d); assert(lv_aic_yuv_image_decoder_deinit()); assert(lv_aic_rgb_image_decoder_deinit()); lv_deinit();
    return 0;
}
