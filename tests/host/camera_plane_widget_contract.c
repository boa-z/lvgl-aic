/* SPDX-License-Identifier: Apache-2.0 */
#define main camera_baseline_main
#include "camera_widget_contract.c"
#undef main
#include "lv_aic_video_plane.h"
#include <stdio.h>
struct lv_aic_video_plane { lv_aic_yuv_image_t *reader; } plane;
static bool hold_close,fail_present;
static unsigned presents,hides;
lv_aic_video_plane_t *lv_aic_video_plane_open(void) { return &plane; }
bool lv_aic_video_plane_enable_ui_alpha(lv_aic_video_plane_t *p) { assert(p==&plane);return true; }
bool lv_aic_video_plane_present_rotated(lv_aic_video_plane_t *p,const void *source,
    int32_t x,int32_t y,uint32_t w,uint32_t h,unsigned degrees,size_t budget)
{
    assert(p==&plane && x>=0 && y>=0 && w==4 && h==4 && !degrees && !budget);
    const lv_aic_yuv_frame_t *frame;
    lv_aic_yuv_image_t *next=lv_aic_yuv_image_acquire(source,&frame);assert(next);
    if(p->reader) lv_aic_yuv_image_release_lease(p->reader);
    p->reader=next;presents++;return !fail_present;
}
bool lv_aic_video_plane_hide(lv_aic_video_plane_t *p)
{
    assert(p==&plane);if(hold_close) return false;
    if(p->reader) lv_aic_yuv_image_release_lease(p->reader);
    p->reader=NULL;hides++;return true;
}
bool lv_aic_video_plane_close(lv_aic_video_plane_t *p) { return lv_aic_video_plane_hide(p); }
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
int main(void)
{
    assert(camera_baseline_main()==0);
    lv_init();assert(lv_aic_yuv_image_decoder_init());
    static lv_fs_drv_t window_fs;lv_fs_drv_init(&window_fs);window_fs.letter='L';
    window_fs.open_cb=window_open;window_fs.close_cb=window_close;window_fs.read_cb=window_read;
    window_fs.seek_cb=window_seek;window_fs.tell_cb=window_tell;lv_fs_drv_register(&window_fs);
    lv_image_decoder_t *window_decoder=lv_image_decoder_create();assert(window_decoder);
    lv_image_decoder_set_info_cb(window_decoder,window_info);lv_image_decoder_set_open_cb(window_decoder,window_decode);
    lv_display_t *d=lv_display_create(16,16);lv_display_set_color_format(d,LV_COLOR_FORMAT_ARGB8888);
    lv_timer_pause(lv_display_get_refr_timer(d));
    lv_obj_t *obj=make(lv_screen_active());lv_obj_set_pos(obj,0,0);
    assert(lv_aic_camera_set_video_plane(obj,true,0)==LV_RESULT_OK);
    assert(lv_aic_camera_open(obj)==LV_RESULT_OK && lv_aic_camera_start(obj)==LV_RESULT_OK);
    assert(lv_aic_camera_set_video_plane(obj,false,0)==LV_RESULT_INVALID);
    for(unsigned i=0;i<4;i++) {
        frames=1;tick();assert(presents==i+1 && active->readers==1 && plane.reader);
        assert(strstr(lv_image_get_src(obj),"_0_00000000.fake"));
    }
    assert(lv_aic_camera_pause(obj)==LV_RESULT_OK);tick();
    lv_obj_set_hidden(obj,true);tick();assert(!plane.reader && hides);
    lv_obj_set_hidden(obj,false);tick();assert(plane.reader);
    hold_close=true;lv_obj_delete(obj);tick();
    assert(lv_aic_camera_pending_cleanup()==1 && active && plane.reader);
    hold_close=false;tick();assert(!active && !plane.reader && !lv_aic_camera_pending_cleanup());
    obj=make(lv_screen_active());lv_obj_set_pos(obj,0,0);
    assert(lv_aic_camera_set_video_plane(obj,true,0)==LV_RESULT_OK);
    assert(lv_aic_camera_open(obj)==LV_RESULT_OK && lv_aic_camera_start(obj)==LV_RESULT_OK);
    fail_present=true;frames=1;tick();assert(lv_aic_camera_get_state(obj)==LV_AIC_CAMERA_FAULT);
    assert(active->closing && plane.reader);
    hold_close=true;tick();assert(plane.reader && active->readers);
    hold_close=false;fail_present=false;assert(lv_aic_camera_close(obj)==LV_RESULT_OK);tick();
    assert(!plane.reader && !active);lv_obj_delete(obj);tick();
    assert(created==freed && retained==released);
    lv_image_decoder_delete(window_decoder);
    lv_display_delete(d);assert(lv_aic_yuv_image_decoder_deinit());lv_deinit();return 0;
}
