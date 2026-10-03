/* SPDX-License-Identifier: Apache-2.0 */
#define AIC_LVGL_USE_PRIVATE_API 1
#include "lv_aic_yuv_image.h"
#include "lvgl_aic_private.h"

struct lv_aic_yuv_image {
    lv_image_dsc_t source;
    lv_aic_yuv_frame_t frame;
    lv_aic_yuv_release_cb_t release;
    void *context;
    struct lv_aic_yuv_image *next;
    uint32_t readers;
    bool retired;
};
static lv_aic_yuv_image_t *images;
static lv_image_decoder_t *decoder;
static const uint8_t source_tag[] = {'A','I','C','Y'};

static lv_aic_yuv_image_t *find_image(const void *source)
{
    for (lv_aic_yuv_image_t *image=images; image; image=image->next)
        if (source == &image->source) return image;
    return NULL;
}
static void free_image(lv_aic_yuv_image_t *image)
{
    lv_aic_yuv_image_t **link=&images;
    while (*link && *link != image) link=&(*link)->next;
    if (*link) *link=image->next;
    image->release(image->context);
    lv_free(image);
}
static lv_result_t image_info(lv_image_decoder_t *dec, lv_image_decoder_dsc_t *dsc,
                              lv_image_header_t *header)
{
    LV_UNUSED(dec);
    lv_aic_yuv_image_t *image=find_image(dsc->src);
    /* Keep claiming retired-but-still-referenced descriptors. Returning
     * INVALID here would let LVGL's bin decoder reinterpret the RAW marker. */
    if (!image) return LV_RESULT_INVALID;
    *header=image->source.header;
    header->cf=LV_COLOR_FORMAT_RGB888;
    header->stride=lv_draw_buf_width_to_stride(header->w,LV_COLOR_FORMAT_RGB888);
    return LV_RESULT_OK;
}
static lv_result_t image_open(lv_image_decoder_t *dec, lv_image_decoder_dsc_t *dsc)
{
    LV_UNUSED(dec);
    lv_aic_yuv_image_t *image=find_image(dsc->src);
    if (!image || image->retired || image->readers == UINT32_MAX) return LV_RESULT_INVALID;
    lv_draw_buf_t *buffer=lv_draw_buf_create(image->frame.width,image->frame.height,
                                            LV_COLOR_FORMAT_RGB888,LV_STRIDE_AUTO);
    if (!buffer) return LV_RESULT_INVALID;
    if (!lv_aic_yuv_to_rgb888(&image->frame,buffer->data,buffer->header.stride,buffer->data_size)) {
        lv_draw_buf_destroy(buffer);
        return LV_RESULT_INVALID;
    }
    image->readers++;
    dsc->decoded=buffer;
    dsc->user_data=image;
    /* Immutable per-open snapshots; never add producer pointers to LVGL's
     * generic image cache, which could retain/reuse a previous video frame. */
    return LV_RESULT_OK;
}
static void image_close(lv_image_decoder_t *dec, lv_image_decoder_dsc_t *dsc)
{
    LV_UNUSED(dec);
    lv_aic_yuv_image_t *image=dsc->user_data;
    if (!image) return;
    lv_draw_buf_destroy((lv_draw_buf_t *)dsc->decoded);
    dsc->decoded=NULL; dsc->user_data=NULL;
    image->readers--;
    if (image->retired && !image->readers) free_image(image);
}
bool lv_aic_yuv_image_decoder_init(void)
{
    if (decoder) return false;
    decoder=lv_image_decoder_create();
    if (!decoder) return false;
    decoder->name="AIC YUV frames";
    lv_image_decoder_set_info_cb(decoder,image_info);
    lv_image_decoder_set_open_cb(decoder,image_open);
    lv_image_decoder_set_close_cb(decoder,image_close);
    return true;
}
bool lv_aic_yuv_image_decoder_deinit(void)
{
    if (images) return false;
    if (decoder) lv_image_decoder_delete(decoder);
    decoder=NULL;
    return true;
}
lv_aic_yuv_image_t *lv_aic_yuv_image_create(const lv_aic_yuv_frame_t *frame,
                                           lv_aic_yuv_retain_cb_t retain_cb,
                                           lv_aic_yuv_release_cb_t release_cb,
                                           void *context)
{
    if (!decoder || !retain_cb || !release_cb || !lv_aic_yuv_validate(frame)) return NULL;
    lv_aic_yuv_image_t *image=lv_malloc_zeroed(sizeof(*image));
    if (!image) return NULL;
    image->frame=*frame;
    if (!retain_cb(context)) { lv_free(image); return NULL; }
    image->release=release_cb; image->context=context;
    image->source.header.magic=LV_IMAGE_HEADER_MAGIC;
    image->source.header.cf=LV_COLOR_FORMAT_RAW;
    image->source.header.w=image->frame.width; image->source.header.h=image->frame.height;
    image->source.data=source_tag; image->source.data_size=sizeof(source_tag);
    image->next=images; images=image;
    return image;
}
const lv_image_dsc_t *lv_aic_yuv_image_source(const lv_aic_yuv_image_t *image)
{
    return image && !image->retired ? &image->source : NULL;
}
void lv_aic_yuv_image_destroy(lv_aic_yuv_image_t *image)
{
    if (!image || image->retired) return;
    image->retired=true;
    lv_image_cache_drop(&image->source);
    if (!image->readers) free_image(image);
}
