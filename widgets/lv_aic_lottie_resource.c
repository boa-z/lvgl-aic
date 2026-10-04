/* SPDX-License-Identifier: Apache-2.0 */
#include "../include/lv_aic_lottie.h"
#if LV_USE_LOTTIE
#include "../../lvgl/src/widgets/lottie/lv_lottie_private.h"
#include "lvgl_private.h"
#include <math.h>
#include <stdint.h>
#include <string.h>
/* Native class is defined in the pinned widget but not exported publicly. */
extern const lv_obj_class_t lv_lottie_class;
extern bool lv_aic_lottie_animation_is_active(lv_obj_t *obj);
#if AIC_LVGL_USE_GE2D
#define AIC_LVGL_USE_PRIVATE_API 1
#include "../draw/ge2d/lv_draw_aic_ge2d.h"
#endif

static lv_aic_lottie_result_t validate(lv_obj_t *obj,const lv_aic_lottie_limits_t *limits,
                                      lv_draw_buf_t **buffer)
{
    if(!obj || !limits || !limits->source_bytes || !limits->staging_bytes ||
       !lv_obj_check_type(obj,&lv_lottie_class)) return LV_AIC_LOTTIE_INVALID;
#if AIC_LVGL_USE_GE2D
    if(lv_draw_aic_ge2d_faulted()) return LV_AIC_LOTTIE_BUSY;
#endif
    lv_lottie_t *lottie=(lv_lottie_t *)obj;
    lv_draw_buf_t *b=lv_canvas_get_draw_buf(obj);
    if(!lottie->anim || !lv_aic_lottie_animation_is_active(obj) || !b || !b->data || !b->header.w || !b->header.h ||
       b->header.cf!=LV_COLOR_FORMAT_ARGB8888_PREMULTIPLIED ||
       b->header.stride%4 || b->header.stride<(uint32_t)b->header.w*4 ||
       (uint64_t)b->header.stride*b->header.h>b->data_size) return LV_AIC_LOTTIE_INVALID;
    if((uint64_t)b->header.stride*b->header.h>limits->staging_bytes) return LV_AIC_LOTTIE_LIMIT;
    *buffer=b;
    return LV_AIC_LOTTIE_OK;
}

lv_aic_lottie_result_t lv_aic_lottie_load_data(lv_obj_t *obj,const void *data,size_t size,
                                             const lv_aic_lottie_limits_t *limits)
{
    lv_draw_buf_t *buffer;
    lv_aic_lottie_result_t result=validate(obj,limits,&buffer);
    if(result!=LV_AIC_LOTTIE_OK) return result;
    if(!data || !size) return LV_AIC_LOTTIE_INVALID;
    if(size>limits->source_bytes || size>=UINT32_MAX) return LV_AIC_LOTTIE_LIMIT;
    if(memchr(data,0,size)) return LV_AIC_LOTTIE_DECODE;
    Tvg_Animation *animation=tvg_animation_new();
    Tvg_Canvas *canvas=NULL;
    lv_draw_buf_t *staging=NULL;
    if(!animation) return LV_AIC_LOTTIE_NO_MEMORY;
    Tvg_Paint *paint=tvg_animation_get_picture(animation);
    result=LV_AIC_LOTTIE_DECODE;
    if(!paint || tvg_picture_load_data(paint,data,(uint32_t)size,"lottie",true)!=TVG_RESULT_SUCCESS) goto done;
    float frames=0,seconds=0;
    if(tvg_animation_get_total_frame(animation,&frames)!=TVG_RESULT_SUCCESS ||
       tvg_animation_get_duration(animation,&seconds)!=TVG_RESULT_SUCCESS ||
       !isfinite(frames) || !isfinite(seconds) || frames<1 ||
       (double)frames>INT32_MAX || seconds<=0 ||
       (double)seconds*1000.0+0.5>INT32_MAX || seconds*1000.0f<1.0f) goto done;
    result=LV_AIC_LOTTIE_NO_MEMORY;
    staging=lv_draw_buf_create(buffer->header.w,buffer->header.h,
                              LV_COLOR_FORMAT_ARGB8888_PREMULTIPLIED,buffer->header.stride);
    canvas=tvg_swcanvas_create();
    if(!staging || !canvas) goto done;
    lv_draw_buf_clear(staging,NULL);
    result=LV_AIC_LOTTIE_RENDER;
    if(tvg_swcanvas_set_target(canvas,(uint32_t *)staging->data,staging->header.stride/4,
          staging->header.w,staging->header.h,TVG_COLORSPACE_ARGB8888)!=TVG_RESULT_SUCCESS ||
       tvg_picture_set_size(paint,staging->header.w,staging->header.h)!=TVG_RESULT_SUCCESS ||
       tvg_canvas_push(canvas,paint)!=TVG_RESULT_SUCCESS ||
       tvg_canvas_draw(canvas)!=TVG_RESULT_SUCCESS || tvg_canvas_sync(canvas)!=TVG_RESULT_SUCCESS) goto done;
    /* Only a completely rendered candidate may touch the current canvas. */
    if(tvg_swcanvas_set_target(canvas,(uint32_t *)buffer->data,buffer->header.stride/4,
          buffer->header.w,buffer->header.h,TVG_COLORSPACE_ARGB8888)!=TVG_RESULT_SUCCESS) goto done;
    lv_lottie_t *lottie=(lv_lottie_t *)obj;
    lv_image_cache_drop(lv_image_get_src(obj));
    memcpy(buffer->data,staging->data,(size_t)buffer->header.stride*buffer->header.h);
    Tvg_Animation *old_animation=lottie->tvg_anim;
    Tvg_Canvas *old_canvas=lottie->tvg_canvas;
    lottie->tvg_anim=animation;lottie->tvg_paint=paint;lottie->tvg_canvas=canvas;
    lv_anim_set_duration(lottie->anim,(uint32_t)(seconds*1000.0f+0.5f));
    lottie->anim->act_time=0;lottie->anim->end_value=(int32_t)frames;
    lottie->anim->reverse_play_in_progress=false;lottie->last_rendered_time=0;
    animation=old_animation;canvas=old_canvas;
    lv_obj_invalidate(obj);
    result=LV_AIC_LOTTIE_OK;
done:
    if(animation) tvg_animation_del(animation);
    if(canvas) tvg_canvas_destroy(canvas);
    if(staging) lv_draw_buf_destroy(staging);
    return result;
}

lv_aic_lottie_result_t lv_aic_lottie_load_file(lv_obj_t *obj,const char *path,
                                             const lv_aic_lottie_limits_t *limits)
{
    lv_draw_buf_t *buffer;
    lv_aic_lottie_result_t result=validate(obj,limits,&buffer);
    if(result!=LV_AIC_LOTTIE_OK) return result;
    if(!path || !*path) return LV_AIC_LOTTIE_INVALID;
    lv_fs_file_t file;
    if(lv_fs_open(&file,path,LV_FS_MODE_RD)!=LV_FS_RES_OK) return LV_AIC_LOTTIE_IO;
    uint32_t size=0,used=0;
    char *data=NULL;
    result=LV_AIC_LOTTIE_IO;
    if(lv_fs_seek(&file,0,LV_FS_SEEK_END)!=LV_FS_RES_OK ||
       lv_fs_tell(&file,&size)!=LV_FS_RES_OK ||
       lv_fs_seek(&file,0,LV_FS_SEEK_SET)!=LV_FS_RES_OK) goto close;
    if(!size) { result=LV_AIC_LOTTIE_DECODE;goto close; }
    if(size>=UINT32_MAX || size>limits->source_bytes) { result=LV_AIC_LOTTIE_LIMIT;goto close; }
    data=lv_malloc(size);
    if(!data) { result=LV_AIC_LOTTIE_NO_MEMORY;goto close; }
    while(used<size) {
        uint32_t received=0;
        if(lv_fs_read(&file,data+used,size-used,&received)!=LV_FS_RES_OK ||
           !received || received>size-used) goto close;
        used+=received;
    }
    result=LV_AIC_LOTTIE_OK;
close:
    if(lv_fs_close(&file)!=LV_FS_RES_OK) result=LV_AIC_LOTTIE_IO;
    if(result==LV_AIC_LOTTIE_OK) result=lv_aic_lottie_load_data(obj,data,size,limits);
    if(data) lv_free(data);
    return result;
}
#endif
