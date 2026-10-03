/* SPDX-License-Identifier: Apache-2.0 */
#define AIC_LVGL_USE_PRIVATE_API 1
#include "lv_aic_rgb_image.h"
#include "lv_aic_rgb_image_private.h"
#include "lvgl_aic_private.h"
#include <string.h>
struct lv_aic_rgb_image {
    lv_image_dsc_t source;
    lv_aic_rgb_frame_t frame;
    lv_draw_buf_t borrowed;
    lv_draw_buf_t *decoded[4];
    lv_aic_yuv_release_cb_t release;
    void *context;
    struct lv_aic_rgb_image *next;
    uint32_t readers;
    bool retired;
};
static lv_aic_rgb_image_t *images;
static lv_image_decoder_t *decoder;
static const uint8_t marker[]={'A','I','C','R'};
static unsigned pixel_bytes(lv_color_format_t cf)
{
    switch(cf) {
    case LV_COLOR_FORMAT_RGB565: return 2;
    case LV_COLOR_FORMAT_RGB888: return 3;
    case LV_COLOR_FORMAT_XRGB8888: case LV_COLOR_FORMAT_ARGB8888: return 4;
    default: return 0;
    }
}
bool lv_aic_rgb_frame_validate(const lv_aic_rgb_frame_t *f)
{
    if(!f || !f->data || !f->width || !f->height || f->width>4096 || f->height>4096 ||
       (uint64_t)f->width*f->height>8U*1024U*1024U) return false;
    unsigned bytes=pixel_bytes(f->format);
    uint64_t span=(uint64_t)(f->height-1)*f->stride+(uint64_t)f->width*bytes;
    return bytes && f->stride>=f->width*bytes && f->stride<=UINT16_MAX &&
        span<=f->capacity && span<=UINTPTR_MAX-(uintptr_t)f->data;
}
static lv_aic_rgb_image_t *find(const void *source)
{
    for(lv_aic_rgb_image_t *i=images;i;i=i->next) if(source==&i->source) return i;
    return NULL;
}
static void dispose(lv_aic_rgb_image_t *image)
{
    lv_aic_rgb_image_t **link=&images;
    while(*link && *link!=image) link=&(*link)->next;
    if(*link) *link=image->next;
    for(unsigned i=0;i<4;i++) if(image->decoded[i] && image->decoded[i]!=&image->borrowed)
        lv_draw_buf_destroy(image->decoded[i]);
    image->release(image->context); lv_free(image);
}
lv_aic_rgb_image_t *lv_aic_rgb_image_acquire(const void *source,const lv_aic_rgb_frame_t **frame)
{
    lv_aic_rgb_image_t *image=find(source);
    if(!image || !frame || image->retired || image->readers==UINT32_MAX) return NULL;
    image->readers++; *frame=&image->frame; return image;
}
void lv_aic_rgb_image_release_lease(lv_aic_rgb_image_t *image)
{
    if(!image || !image->readers) return;
    image->readers--; if(image->retired && !image->readers) dispose(image);
}
static lv_result_t info(lv_image_decoder_t *dec,lv_image_decoder_dsc_t *dsc,lv_image_header_t *header)
{
    LV_UNUSED(dec); lv_aic_rgb_image_t *image=find(dsc->src);
    if(!image) return LV_RESULT_INVALID;
    *header=image->source.header; header->cf=image->frame.format; header->stride=image->frame.stride;
    return LV_RESULT_OK;
}
static lv_result_t open_image(lv_image_decoder_t *dec,lv_image_decoder_dsc_t *dsc)
{
    LV_UNUSED(dec); lv_aic_rgb_image_t *image=find(dsc->src);
    if(!image || image->retired || image->readers==UINT32_MAX) return LV_RESULT_INVALID;
    const lv_aic_rgb_frame_t *f=&image->frame;
    bool premultiply=dsc->args.premultiply && f->format==LV_COLOR_FORMAT_ARGB8888;
    bool align=dsc->args.stride_align;
    unsigned index=(align?1U:0U)|(premultiply?2U:0U);
    if(!image->decoded[index]) {
        unsigned natural=pixel_bytes(f->format)==3?1:pixel_bytes(f->format);
        bool misaligned=((uintptr_t)f->data%natural) || (f->stride%natural);
        uint32_t stride=align || misaligned?lv_draw_buf_width_to_stride(f->width,f->format):f->stride;
        bool copy=premultiply || stride!=f->stride || (uint64_t)f->stride*f->height>f->capacity ||
            misaligned;
        if(copy) {
            lv_draw_buf_t *buffer=lv_draw_buf_create(f->width,f->height,f->format,stride);
            if(!buffer) return LV_RESULT_INVALID;
            memset(buffer->data,0,buffer->data_size);
            for(unsigned y=0;y<f->height;y++)
                memcpy(buffer->data+(size_t)y*stride,f->data+(size_t)y*f->stride,f->width*pixel_bytes(f->format));
            if(premultiply && lv_draw_buf_premultiply(buffer)!=LV_RESULT_OK) {
                lv_draw_buf_destroy(buffer); return LV_RESULT_INVALID;
            }
            lv_draw_buf_clear_flag(buffer,LV_IMAGE_FLAGS_MODIFIABLE);
            image->decoded[index]=buffer;
        } else {
            if(!image->borrowed.data && lv_draw_buf_init(&image->borrowed,f->width,f->height,f->format,
                f->stride,(void *)f->data,f->stride*f->height)!=LV_RESULT_OK) return LV_RESULT_INVALID;
            image->decoded[index]=&image->borrowed;
        }
    }
    image->readers++; dsc->decoded=image->decoded[index]; dsc->user_data=image; return LV_RESULT_OK;
}
static void close_image(lv_image_decoder_t *dec,lv_image_decoder_dsc_t *dsc)
{
    LV_UNUSED(dec); lv_aic_rgb_image_t *image=dsc->user_data;
    dsc->decoded=NULL; dsc->user_data=NULL;
    lv_aic_rgb_image_release_lease(image);
}
bool lv_aic_rgb_image_decoder_is_initialized(void) { return decoder!=NULL; }
bool lv_aic_rgb_image_decoder_init(void)
{
    if(decoder) return false;
    decoder=lv_image_decoder_create(); if(!decoder) return false;
    decoder->name="AIC RGB frames";
    lv_image_decoder_set_info_cb(decoder,info); lv_image_decoder_set_open_cb(decoder,open_image);
    lv_image_decoder_set_close_cb(decoder,close_image); return true;
}
bool lv_aic_rgb_image_decoder_deinit(void)
{
    if(images) return false;
    if(decoder) lv_image_decoder_delete(decoder);
    decoder=NULL; return true;
}
lv_aic_rgb_image_t *lv_aic_rgb_image_create(const lv_aic_rgb_frame_t *frame,
    lv_aic_yuv_retain_cb_t retain,lv_aic_yuv_release_cb_t release,void *context)
{
    if(!decoder || !retain || !release || !lv_aic_rgb_frame_validate(frame)) return NULL;
    lv_aic_rgb_image_t *image=lv_malloc_zeroed(sizeof(*image)); if(!image) return NULL;
    if(!retain(context)) { lv_free(image); return NULL; }
    image->frame=*frame; image->release=release; image->context=context;
    image->source.header.magic=LV_IMAGE_HEADER_MAGIC; image->source.header.cf=LV_COLOR_FORMAT_RAW;
    image->source.header.w=frame->width; image->source.header.h=frame->height;
    image->source.data=marker; image->source.data_size=sizeof(marker);
    image->next=images; images=image; return image;
}
const lv_image_dsc_t *lv_aic_rgb_image_source(const lv_aic_rgb_image_t *image)
{ return image && !image->retired?&image->source:NULL; }
void lv_aic_rgb_image_destroy(lv_aic_rgb_image_t *image)
{
    if(!image || image->retired) return;
    image->retired=true; lv_image_cache_drop(&image->source);
    if(!image->readers) dispose(image);
}
