/* SPDX-License-Identifier: Apache-2.0 */
#define AIC_LVGL_USE_PRIVATE_API 1
#include "lv_aic_yuv_image.h"
#include "lvgl_aic_private.h"
#include <assert.h>
#include <string.h>

static int retained, released, live, reject_retain;
static bool retain_frame(void *context)
{
    assert(context == &live);
    if (reject_retain) return false;
    retained++; live++; return true;
}
static void release_frame(void *context)
{ assert(context == &live && live>0); released++; live--; }
static void *fail_buffer(size_t size, lv_color_format_t cf)
{ (void)size; (void)cf; return NULL; }
static void flush(lv_display_t *display, const lv_area_t *area, uint8_t *data)
{ (void)area; (void)data; lv_display_flush_ready(display); }

int main(void)
{
    uint8_t y[16], u[4], v[4];
    lv_aic_yuv_frame_t frame = {0};
    frame.width=4; frame.height=4; frame.format=LV_COLOR_FORMAT_I420;
    frame.color_space=LV_AIC_YUV_BT601_LIMITED;
    frame.planes[0]=(lv_aic_yuv_plane_t){y,4,sizeof(y)};
    frame.planes[1]=(lv_aic_yuv_plane_t){u,2,sizeof(u)};
    frame.planes[2]=(lv_aic_yuv_plane_t){v,2,sizeof(v)};
    memset(y,81,sizeof(y)); memset(u,90,sizeof(u)); memset(v,240,sizeof(v));
    lv_init();
    assert(!lv_aic_yuv_image_create(&frame,retain_frame,release_frame,&live));
    assert(!lv_aic_yuv_image_decoder_is_initialized());
    assert(lv_aic_yuv_image_decoder_init());
    assert(lv_aic_yuv_image_decoder_is_initialized());
    assert(!lv_aic_yuv_image_decoder_init());
    reject_retain=1;
    assert(!lv_aic_yuv_image_create(&frame,retain_frame,release_frame,&live));
    assert(!live && !retained && !released); reject_retain=0;
    frame.planes[2].capacity--;
    assert(!lv_aic_yuv_image_create(&frame,retain_frame,release_frame,&live));
    frame.planes[2].capacity++;
    lv_aic_yuv_image_t *image=lv_aic_yuv_image_create(&frame,retain_frame,release_frame,&live);
    assert(image && live==1 && retained==1);
    const lv_image_dsc_t *source=lv_aic_yuv_image_source(image);
    lv_image_header_t header;
    assert(lv_image_decoder_get_info(source,&header)==LV_RESULT_OK);
    assert(header.w==4 && header.h==4 && header.cf==LV_COLOR_FORMAT_RGB888);
    assert(!lv_aic_yuv_image_decoder_deinit());
    lv_image_decoder_dsc_t a, b, fail;
    lv_draw_buf_handlers_t *handlers=lv_draw_buf_get_handlers();
    lv_draw_buf_malloc_cb_t original=handlers->buf_malloc_cb;
    handlers->buf_malloc_cb=fail_buffer;
    assert(lv_image_decoder_open(&fail,source,NULL)==LV_RESULT_INVALID);
    handlers->buf_malloc_cb=original;
    assert(live==1 && !released);
    assert(lv_image_decoder_open(&a,source,NULL)==LV_RESULT_OK);
    assert(lv_image_decoder_open(&b,source,NULL)==LV_RESULT_OK);
    assert(a.decoded != b.decoded && a.decoded->data != b.decoded->data);
    assert(!a.cache_entry && !b.cache_entry);
    assert(a.decoded->data[0]==0 && a.decoded->data[1]==0 && a.decoded->data[2]==254);
    lv_aic_yuv_image_destroy(image);
    assert(live==1 && !released && !lv_aic_yuv_image_decoder_deinit());
    assert(lv_image_decoder_open(&fail,source,NULL)==LV_RESULT_INVALID);
    lv_image_decoder_close(&a); assert(live==1);
    assert(b.decoded->data[2]==254);
    lv_image_decoder_close(&b); assert(live==0 && released==1);
    assert(lv_aic_yuv_image_decoder_deinit());
    assert(!lv_aic_yuv_image_decoder_is_initialized());
    assert(lv_aic_yuv_image_decoder_init());
    assert(lv_aic_yuv_image_decoder_is_initialized());

    /* Actual native image widget -> software draw -> framebuffer pixels.
     * Frame replacement must not reuse a previous source/cache entry. */
    static uint8_t pixels[16*16*3];
    lv_display_t *display=lv_display_create(16,16);
    lv_display_set_color_format(display,LV_COLOR_FORMAT_RGB888);
    lv_display_set_buffers(display,pixels,NULL,sizeof(pixels),LV_DISPLAY_RENDER_MODE_DIRECT);
    lv_display_set_flush_cb(display,flush);
    lv_obj_t *screen=lv_screen_active();
    lv_obj_set_style_bg_color(screen,lv_color_black(),0);
    lv_obj_set_style_bg_opa(screen,LV_OPA_COVER,0);
    lv_obj_t *widget=lv_image_create(screen);
    lv_obj_set_pos(widget,0,0);
    uint8_t uv[16]; memset(uv,128,sizeof(uv));
    const lv_aic_yuv_format_t publication_formats[]={LV_COLOR_FORMAT_I420,LV_AIC_YUV_NV16,LV_AIC_YUV_NV61};
    for(unsigned f=0;f<3;f++) {
    frame.format=publication_formats[f];
    frame.planes[1]=f ? (lv_aic_yuv_plane_t){uv,4,sizeof(uv)} : (lv_aic_yuv_plane_t){u,2,sizeof(u)};
    for (int cycle=0;cycle<20;cycle++) {
        memset(y,cycle&1 ? 235 : 16,sizeof(y));
        memset(u,128,sizeof(u)); memset(v,128,sizeof(v));
        image=lv_aic_yuv_image_create(&frame,retain_frame,release_frame,&live);
        assert(image);
        lv_image_set_src(widget,lv_aic_yuv_image_source(image));
        lv_refr_now(display);
        for (unsigned row=0;row<4;row++)
            for (unsigned byte=0;byte<12;byte++) assert(pixels[row*48+byte]==(cycle&1 ? 255 : 0));
        lv_image_set_src(widget,NULL);
        lv_refr_now(display);
        lv_aic_yuv_image_destroy(image);
        assert(live==0);
    }
    }
    lv_obj_delete(widget);
    lv_display_delete(display);
    assert(retained==released);
    assert(lv_aic_yuv_image_decoder_deinit());
    lv_deinit();
    /* Repeat a complete LVGL lifetime: no stale decoder/registry state. */
    lv_init();
    assert(!lv_aic_yuv_image_decoder_is_initialized());
    assert(lv_aic_yuv_image_decoder_init());
    assert(lv_aic_yuv_image_decoder_is_initialized());
    assert(lv_aic_yuv_image_decoder_deinit());
    lv_deinit();
    return 0;
}
