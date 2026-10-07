/* SPDX-License-Identifier: Apache-2.0 */
#include "lvgl.h"
#include "lv_aic_lottie.h"
#include "lvgl_private.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Self-contained moving half-opacity red square, 30 fps and two seconds. */
static const char animation[] = "{\"v\":\"5.7.4\",\"fr\":30,\"ip\":0,\"op\":60,\"w\":32,\"h\":32,\"layers\":[{\"ty\":1,\"ind\":1,\"ip\":0,\"op\":60,\"st\":0,\"sw\":8,\"sh\":8,\"sc\":\"#ff0000\",\"ks\":{\"o\":{\"a\":0,\"k\":50},\"r\":{\"a\":0,\"k\":0},\"a\":{\"a\":0,\"k\":[0,0,0]},\"s\":{\"a\":0,\"k\":[100,100,100]},\"p\":{\"a\":1,\"k\":[{\"t\":0,\"s\":[4,4,0],\"e\":[20,4,0],\"o\":{\"x\":0,\"y\":0},\"i\":{\"x\":1,\"y\":1}},{\"t\":30,\"s\":[20,4,0]}]}}}]}";
static bool fail_staging;
lv_draw_buf_t *__real_lv_draw_buf_create(uint32_t w,uint32_t h,lv_color_format_t cf,uint32_t stride);
lv_draw_buf_t *__wrap_lv_draw_buf_create(uint32_t w,uint32_t h,lv_color_format_t cf,uint32_t stride)
{
    if(fail_staging) return NULL;
    return __real_lv_draw_buf_create(w,h,cf,stride);
}
static unsigned file_mode,file_opens,file_closes,file_position;
static void *open_file(lv_fs_drv_t *drv,const char *path,lv_fs_mode_t mode)
{
    (void)drv;
    if(file_mode==1 || strcmp(path,"/animation.json") || mode!=LV_FS_MODE_RD) return NULL;
    file_position=0;file_opens++;return &file_position;
}
static lv_fs_res_t close_file(lv_fs_drv_t *drv,void *file)
{ (void)drv;(void)file;file_closes++;return file_mode==5?LV_FS_RES_FS_ERR:LV_FS_RES_OK; }
static lv_fs_res_t read_file(lv_fs_drv_t *drv,void *file,void *buf,uint32_t requested,uint32_t *read)
{
    (void)drv;(void)file;
    if(file_mode==6) return LV_FS_RES_FS_ERR;
    uint32_t available=(uint32_t)sizeof(animation)-1;
    if(file_mode==3) available/=2;
    *read=file_position<available?available-file_position:0;
    if(*read>requested) *read=requested;
    if(*read>7) *read=7; /* Deliberate valid short reads, separate from EOF. */
    memcpy(buf,animation+file_position,*read);file_position+=*read;return LV_FS_RES_OK;
}
static lv_fs_res_t seek_file(lv_fs_drv_t *drv,void *file,uint32_t pos,lv_fs_whence_t whence)
{
    (void)drv;(void)file;
    if(file_mode==2) return LV_FS_RES_FS_ERR;
    file_position=pos+(whence==LV_FS_SEEK_END?(uint32_t)sizeof(animation)-1:0);
    return LV_FS_RES_OK;
}
static lv_fs_res_t tell_file(lv_fs_drv_t *drv,void *file,uint32_t *pos)
{ (void)drv;(void)file;*pos=file_mode==4?1000000:file_position;return LV_FS_RES_OK; }
static void checked_failures(lv_obj_t *obj,lv_draw_buf_t *buffer)
{
    lv_aic_lottie_limits_t limits={4096,4096};
    uint8_t snapshot[4096];memcpy(snapshot,buffer->data,sizeof snapshot);
    lv_anim_t *anim=lv_lottie_get_anim(obj);int32_t time=anim->act_time;
    assert(lv_aic_lottie_load_data(obj,NULL,3,&limits)==LV_AIC_LOTTIE_INVALID);
    assert(lv_aic_lottie_load_data(obj,animation,SIZE_MAX,&limits)==LV_AIC_LOTTIE_LIMIT);
    assert(lv_aic_lottie_load_data(obj,animation,sizeof animation,&limits)==LV_AIC_LOTTIE_DECODE);
    assert(lv_aic_lottie_load_data(obj,"not-json",8,&limits)==LV_AIC_LOTTIE_DECODE);
    assert(lv_aic_lottie_load_data(obj,"{",1,&limits)==LV_AIC_LOTTIE_DECODE);
    const char zero[]="{\"v\":\"5.7.4\",\"fr\":30,\"ip\":0,\"op\":0,\"w\":32,\"h\":32,\"layers\":[]}";
    assert(lv_aic_lottie_load_data(obj,zero,sizeof(zero)-1,&limits)==LV_AIC_LOTTIE_DECODE);
    lv_aic_lottie_limits_t small={8,4096};
    assert(lv_aic_lottie_load_data(obj,animation,sizeof(animation)-1,&small)==LV_AIC_LOTTIE_LIMIT);
    small=(lv_aic_lottie_limits_t){4096,16};
    assert(lv_aic_lottie_load_data(obj,animation,sizeof(animation)-1,&small)==LV_AIC_LOTTIE_LIMIT);
    fail_staging=true;
    assert(lv_aic_lottie_load_data(obj,animation,sizeof(animation)-1,&limits)==LV_AIC_LOTTIE_NO_MEMORY);
    fail_staging=false;
    assert(!memcmp(snapshot,buffer->data,sizeof snapshot));
    assert(anim->act_time==time && lv_anim_is_paused(anim));
    for(file_mode=1;file_mode<=6;file_mode++) {
        assert(lv_aic_lottie_load_file(obj,"R:/animation.json",&limits)==
               (file_mode==4?LV_AIC_LOTTIE_LIMIT:LV_AIC_LOTTIE_IO));
        assert(!memcmp(snapshot,buffer->data,sizeof snapshot));
        assert(lv_lottie_get_anim(obj)==anim && anim->act_time==time && anim->duration==2000);
        assert(lv_anim_is_paused(anim));
    }
    file_mode=0;
    assert(file_opens==file_closes);
    assert(lv_aic_lottie_load_file(obj,"R:/animation.json",&limits)==LV_AIC_LOTTIE_OK);
    assert(lv_anim_is_paused(anim) && anim->act_time==0 && anim->duration==2000);
    assert(file_opens==file_closes);
    assert(lv_aic_lottie_load_data(obj,animation,sizeof(animation)-1,&limits)==LV_AIC_LOTTIE_OK);
}
static uint32_t screen[64*64];
static unsigned flushes;
static void flush(lv_display_t *d, const lv_area_t *a, uint8_t *data)
{ (void)a; (void)data; flushes++; lv_display_flush_ready(d); }
static void advance(unsigned ms)
{ for(unsigned t=0;t<ms;t+=10) { lv_tick_inc(10); lv_timer_handler(); } }
static uint32_t pixel(lv_draw_buf_t *b, unsigned x, unsigned y)
{ return *(uint32_t *)(b->data+y*b->header.stride+x*4); }
static void check_square(lv_draw_buf_t *b, unsigned x)
{
    uint32_t c=pixel(b,x,7);
    assert((c>>24)>=126 && (c>>24)<=128);
    assert(((c>>16)&255)==(c>>24) && (c&0xffff)==0);
    assert(pixel(b,0,0)==0 && pixel(b,31,31)==0);
}
int main(void)
{
    FILE *file=fopen("lottie-contract.json","wb");assert(file);
    assert(fwrite(animation,1,sizeof(animation)-1,file)==sizeof(animation)-1);
    assert(fclose(file)==0);
    lv_init();
    lv_fs_drv_t fs;lv_fs_drv_init(&fs);fs.letter='R';
    fs.open_cb=open_file;fs.close_cb=close_file;fs.read_cb=read_file;
    fs.seek_cb=seek_file;fs.tell_cb=tell_file;lv_fs_drv_register(&fs);
    lv_display_t *d=lv_display_create(64,64);assert(d);
    lv_display_set_color_format(d,LV_COLOR_FORMAT_ARGB8888);
    lv_display_set_buffers(d,screen,NULL,sizeof screen,LV_DISPLAY_RENDER_MODE_DIRECT);
    lv_display_set_flush_cb(d,flush);
    lv_obj_set_style_bg_color(lv_screen_active(),lv_color_black(),0);
    lv_obj_set_style_bg_opa(lv_screen_active(),LV_OPA_COVER,0);
    unsigned before=lv_anim_count_running();
    uint32_t first[32*32],last[32*32];
    for(unsigned n=0;n<20;n++) {
        lv_draw_buf_t *b=lv_draw_buf_create(32,32,n%2?LV_COLOR_FORMAT_ARGB8888:LV_COLOR_FORMAT_ARGB8888_PREMULTIPLIED,0);assert(b);
        lv_obj_t *obj=lv_lottie_create(lv_screen_active());assert(obj);
        lv_obj_set_pos(obj,8,8);
        if(n&2) lv_lottie_set_buffer(obj,32,32,b->data);
        else lv_lottie_set_draw_buf(obj,b);
        if(n%2) lv_lottie_set_src_file(obj,"lottie-contract.json");
        else {
            char *copy=malloc(sizeof animation);assert(copy);memcpy(copy,animation,sizeof animation);
            lv_lottie_set_src_data(obj,copy,sizeof(animation)-1);
            memset(copy,0,sizeof animation);free(copy); /* Loader promises a copy. */
        }
        lv_anim_t *a=lv_lottie_get_anim(obj);assert(a && a->duration==2000 && a->end_value==60);
        assert(lv_canvas_get_draw_buf(obj)->header.flags&LV_IMAGE_FLAGS_PREMULTIPLIED);
        assert(lv_canvas_get_draw_buf(obj)->header.cf==LV_COLOR_FORMAT_ARGB8888_PREMULTIPLIED);
        assert(b->header.stride==32*4);
        check_square(b,7);assert(pixel(b,23,7)==0);
        if(!n) memcpy(first,b->data,sizeof first);
        else assert(!memcmp(first,b->data,sizeof first));
        lv_refr_now(d);
        uint32_t c=screen[15*64+15];
        assert(((c>>16)&255)>=125 && ((c>>16)&255)<=128 && (c&0xffff)==0);
        advance(1100); /* Real native animation timer, beyond final keyframe. */
        check_square(b,23);assert(pixel(b,7,7)==0);
        if(!n) memcpy(last,b->data,sizeof last);
        else assert(!memcmp(last,b->data,sizeof last));
        lv_anim_pause(a);advance(200);assert(!memcmp(last,b->data,sizeof last));
        checked_failures(obj,b);
        assert(!memcmp(first,b->data,sizeof first));
        lv_anim_resume(a);
        advance(1100);check_square(b,23);
        lv_lottie_set_src_data(obj,animation,sizeof(animation)-1);
        assert(!memcmp(first,b->data,sizeof first));
        lv_obj_delete(obj);
        assert(lv_anim_count_running()==before);
        /* Native widget never frees the caller's draw buffer. */
        memset(b->data,0,b->data_size);lv_draw_buf_destroy(b);
        advance(30);lv_refr_now(d);assert((screen[15*64+15]&0xffffff)==0);
    }
    /* Native callers can remove the animation; checked loading must not use
     * the widget's now stale animation pointer. */
    lv_obj_t *stopped=lv_lottie_create(lv_screen_active());
    lv_draw_buf_t *stopped_buffer=lv_draw_buf_create(32,32,LV_COLOR_FORMAT_ARGB8888_PREMULTIPLIED,0);
    assert(stopped_buffer);lv_lottie_set_draw_buf(stopped,stopped_buffer);
    lv_aic_lottie_limits_t limits={4096,4096};
    const char empty[]="{\"v\":\"5.7.4\",\"fr\":30,\"ip\":0,\"op\":60,\"w\":32,\"h\":32,\"layers\":[]}";
    assert(lv_aic_lottie_load_data(stopped,empty,sizeof(empty)-1,&limits)==LV_AIC_LOTTIE_OK);
    for(unsigned i=0;i<stopped_buffer->data_size;i++) assert(stopped_buffer->data[i]==0);
    lv_anim_delete(stopped,NULL);
    assert(lv_aic_lottie_load_data(stopped,animation,sizeof(animation)-1,&limits)==LV_AIC_LOTTIE_INVALID);
    lv_obj_delete(stopped);lv_draw_buf_destroy(stopped_buffer);
    lv_display_delete(d);lv_deinit();assert(flushes>40);
    assert(remove("lottie-contract.json")==0);
    puts("PASS Lottie FILE/data parity, timing, premultiplied pixels, pause/reset, copied source and caller-owned lifecycle");
    return 0;
}
