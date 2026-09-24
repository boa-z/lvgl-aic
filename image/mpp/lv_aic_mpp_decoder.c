/**
 * @file lv_aic_mpp_decoder.c
 * @brief ArtInChip MPP JPEG/PNG decoder for LVGL 9.6 (Phase 2A).
 *
 * FILE (.jpg/.jpeg/.png) only. No cache, no GE2D, no YUV hack: MPP is asked
 * for native RGB888 (JPEG, PNG RGB) or ARGB8888 (PNG RGBA) so the SW renderer
 * consumes pixels directly.
 *
 * Buffer ownership: session owns CMA allocation_base; draw_buf.data points at
 * the aligned base; PNG alpha/stride post-process may replace it with a heap
 * buffer (tracked as heap_buf). Close releases both without leaks. All
 * failures return LV_RESULT_INVALID.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "lv_aic_mpp_decoder.h"
#include "lv_aic_mpp_format.h"
#include "lv_aic_mpp_stream.h"

#if defined(AIC_LVGL_USE_MPP_DEC) && AIC_LVGL_BSP_MPP

#define AIC_LVGL_USE_PRIVATE_API 1
#include "lvgl_aic_private.h"

#include <aic_core.h>
#include <aic_osal.h>
#include <mpp_decoder.h>
#include <frame_allocator.h>
#include <string.h>
#if AIC_LVGL_BSP_RTTHREAD
#include <rtthread.h>
#endif

#ifndef CACHE_LINE_SIZE
#define CACHE_LINE_SIZE 32U
#endif

#define LV_AIC_MPP_MAX_FILE_BYTES (8U * 1024U * 1024U)
#define LV_AIC_MPP_MAX_DIMENSION 4096U
#define LV_AIC_MPP_MAX_PIXELS (8U * 1024U * 1024U)

typedef struct {
    lv_draw_buf_t draw_buf;
    void *allocation_base;
    void *aligned_data;
    uint32_t cma_size;
    lv_draw_buf_t *heap_buf;
    struct mpp_buf mpp_buf_copy;
    uint32_t width;
    uint32_t height;
    lv_color_format_t color_format;
} lv_aic_mpp_session_t;

typedef struct {
    struct mpp_frame frame_template;
    struct frame_allocator allocator;
    struct alloc_ops ops;
} lv_aic_mpp_ext_allocator_t;

static lv_image_decoder_t *g_aic_mpp_decoder;
static lv_aic_mpp_decode_stats_t g_aic_mpp_last_stats;

static uint32_t lv_aic_mpp_align_up(uint32_t value, uint32_t align)
{
    if (align == 0U) {
        return value;
    }
    return (value + align - 1U) & ~(align - 1U);
}

static bool lv_aic_mpp_has_ext(const char *src, const char *ext_lower)
{
    const char *dot;
    size_t i;

    if (src == NULL || ext_lower == NULL) {
        return false;
    }
    dot = strrchr(src, '.');
    if (dot == NULL || dot[1] == '\0') {
        return false;
    }
    for (i = 0U; ext_lower[i] != '\0'; i++) {
        char a = dot[i + 1U];
        char b = ext_lower[i];
        if (a >= 'A' && a <= 'Z') {
            a = (char)(a - 'A' + 'a');
        }
        if (a != b) {
            return false;
        }
    }
    return dot[strlen(ext_lower) + 1U] == '\0';
}

static bool lv_aic_mpp_is_jpeg_path(const char *src)
{
    return lv_aic_mpp_has_ext(src, "jpg") || lv_aic_mpp_has_ext(src, "jpeg");
}

static bool lv_aic_mpp_is_png_path(const char *src)
{
    return lv_aic_mpp_has_ext(src, "png");
}

static int lv_aic_mpp_alloc_ext_frame(struct frame_allocator *p, struct mpp_frame *frame,
                                      int width, int height, enum mpp_pixel_format format)
{
    lv_aic_mpp_ext_allocator_t *ext;

    (void)width;
    (void)height;
    (void)format;
    if (p == NULL || frame == NULL) {
        return -1;
    }
    ext = (lv_aic_mpp_ext_allocator_t *)p;
    memcpy(frame, &ext->frame_template, sizeof(*frame));
    return 0;
}

static int lv_aic_mpp_free_ext_frame(struct frame_allocator *p, struct mpp_frame *frame)
{
    (void)p;
    (void)frame;
    return 0;
}

static int lv_aic_mpp_close_ext_allocator(struct frame_allocator *p)
{
    (void)p;
    return 0;
}

static lv_result_t lv_aic_mpp_parse_jpeg_header(lv_aic_mpp_stream_t *stream, int *width,
                                                int *height, int *components)
{
    uint8_t buf[16];
    uint32_t got = 0U;

    if (lv_aic_mpp_stream_read(stream, buf, 2U, &got) != LV_FS_RES_OK || got != 2U) {
        return LV_RESULT_INVALID;
    }
    if (buf[0] != 0xFFU || buf[1] != 0xD8U) {
        return LV_RESULT_INVALID;
    }

    for (;;) {
        uint16_t marker;
        uint16_t size;
        uint8_t sof[15];

        if (lv_aic_mpp_stream_read(stream, buf, 4U, &got) != LV_FS_RES_OK || got != 4U) {
            return LV_RESULT_INVALID;
        }
        marker = lv_aic_mpp_stream_u16_be(buf);
        size = lv_aic_mpp_stream_u16_be(buf + 2U);
        if (size < 2U) {
            return LV_RESULT_INVALID;
        }
        if (marker == 0xFFC0U || marker == 0xFFC1U) {
            uint8_t precision;
            if (lv_aic_mpp_stream_read(stream, sof, 15U, &got) != LV_FS_RES_OK ||
                got != 15U) {
                return LV_RESULT_INVALID;
            }
            precision = sof[0];
            if (precision != 8U) {
                return LV_RESULT_INVALID;
            }
            *height = (int)lv_aic_mpp_stream_u16_be(sof + 1U);
            *width = (int)lv_aic_mpp_stream_u16_be(sof + 3U);
            *components = (int)sof[5];
            if (*width <= 0 || *height <= 0) {
                return LV_RESULT_INVALID;
            }
            if (*components != 1 && *components != 3) {
                return LV_RESULT_INVALID;
            }
            return LV_RESULT_OK;
        }
        {
            uint32_t cur = stream->cursor;
            uint32_t next = cur + (uint32_t)size - 2U;
            if (lv_aic_mpp_stream_seek(stream, next) != LV_FS_RES_OK) {
                return LV_RESULT_INVALID;
            }
        }
        if (stream->cursor > LV_AIC_MPP_MAX_FILE_BYTES) {
            return LV_RESULT_INVALID;
        }
    }
}

static lv_result_t lv_aic_mpp_parse_png_header(lv_aic_mpp_stream_t *stream, int *width,
                                               int *height, lv_color_format_t *cf)
{
    static const uint8_t kPngSig[8] = {137U, 80U, 78U, 71U, 13U, 10U, 26U, 10U};
    uint8_t buf[33];
    uint32_t got = 0U;
    uint8_t bit_depth;
    uint8_t color_type;

    if (lv_aic_mpp_stream_read(stream, buf, sizeof(buf), &got) != LV_FS_RES_OK ||
        got != sizeof(buf)) {
        return LV_RESULT_INVALID;
    }
    if (memcmp(buf, kPngSig, sizeof(kPngSig)) != 0) {
        return LV_RESULT_INVALID;
    }
    /* IHDR length must be 13 and type "IHDR". */
    if (lv_aic_mpp_stream_u32_be(buf + 8U) != 13U ||
        memcmp(buf + 12U, "IHDR", 4U) != 0) {
        return LV_RESULT_INVALID;
    }
    *width = (int)lv_aic_mpp_stream_u32_be(buf + 16U);
    *height = (int)lv_aic_mpp_stream_u32_be(buf + 20U);
    bit_depth = buf[24];
    color_type = buf[25];
    if (*width <= 0 || *height <= 0) {
        return LV_RESULT_INVALID;
    }

    switch (color_type) {
    case 2U: /* RGB */
        if (bit_depth != 8U) {
            return LV_RESULT_INVALID;
        }
        *cf = LV_COLOR_FORMAT_RGB888;
        return LV_RESULT_OK;
    case 6U: /* RGBA */
        if (bit_depth != 8U) {
            return LV_RESULT_INVALID;
        }
        *cf = LV_COLOR_FORMAT_ARGB8888;
        return LV_RESULT_OK;
    case 3U: /* palette: MPP expands to 32-bit */
        if (bit_depth != 1U && bit_depth != 2U && bit_depth != 4U && bit_depth != 8U) {
            return LV_RESULT_INVALID;
        }
        *cf = LV_COLOR_FORMAT_ARGB8888;
        return LV_RESULT_OK;
    default:
        /* Gray / gray-alpha have no SW RGB path in Phase 2A. */
        return LV_RESULT_INVALID;
    }
}

static uint32_t lv_aic_mpp_stride_for(lv_color_format_t cf, int width)
{
    uint32_t bpp = lv_aic_mpp_format_lvgl_bpp(cf);
    if (bpp == 0U || width <= 0) {
        return 0U;
    }
    /* PNG HW uses 8B stride; JPEG RGB uses 16B. Use 16B for both: it satisfies
     * both engines and LVGL's minimum stride. */
    return lv_aic_mpp_align_up((uint32_t)width * bpp, 16U);
}

static void lv_aic_mpp_session_release(lv_aic_mpp_session_t *session)
{
    if (session == NULL) {
        return;
    }
    if (session->heap_buf != NULL) {
        lv_draw_buf_destroy(session->heap_buf);
        session->heap_buf = NULL;
    }
    if (session->allocation_base != NULL) {
        aicos_free_align(MEM_CMA, session->allocation_base);
        session->allocation_base = NULL;
        session->aligned_data = NULL;
    }
    lv_free(session);
}

static lv_result_t lv_aic_mpp_decode_file(const char *src, enum mpp_codec_type codec,
                                          enum mpp_pixel_format mpp_fmt,
                                          lv_color_format_t lv_fmt, int width, int height,
                                          bool use_post_process, lv_image_decoder_dsc_t *dsc,
                                          lv_aic_mpp_session_t **out_session)
{
    lv_aic_mpp_stream_t stream;
    lv_aic_mpp_session_t *session = NULL;
    lv_aic_mpp_ext_allocator_t ext_alloc;
    struct mpp_decoder *dec = NULL;
    struct decode_config config;
    struct mpp_packet packet;
    struct mpp_frame frame;
    uint32_t file_len = 0U;
    uint32_t stride;
    uint32_t cma_size;
    void *cma_base = NULL;
    uint32_t read_done = 0U;
    uint32_t tick_start = 0U;
    lv_result_t result = LV_RESULT_INVALID;
#if AIC_LVGL_BSP_RTTHREAD
    tick_start = (uint32_t)rt_tick_get_millisecond();
#else
    (void)dsc;
    (void)use_post_process;
#endif

    memset(&stream, 0, sizeof(stream));
    memset(&ext_alloc, 0, sizeof(ext_alloc));
    memset(&config, 0, sizeof(config));
    memset(&packet, 0, sizeof(packet));
    memset(&frame, 0, sizeof(frame));

    if (lv_aic_mpp_stream_open_file(&stream, src) != LV_FS_RES_OK) {
        return LV_RESULT_INVALID;
    }
    file_len = lv_aic_mpp_stream_size(&stream);
    if (file_len == 0U || file_len > LV_AIC_MPP_MAX_FILE_BYTES) {
        lv_aic_mpp_stream_close(&stream);
        return LV_RESULT_INVALID;
    }

    stride = lv_aic_mpp_stride_for(lv_fmt, width);
    if (stride == 0U) {
        lv_aic_mpp_stream_close(&stream);
        return LV_RESULT_INVALID;
    }
    if ((uint64_t)stride * (uint64_t)(uint32_t)height > LV_AIC_MPP_MAX_PIXELS * 4U) {
        lv_aic_mpp_stream_close(&stream);
        return LV_RESULT_INVALID;
    }
    cma_size = lv_aic_mpp_align_up(stride * (uint32_t)height, CACHE_LINE_SIZE);
    if (cma_size == 0U) {
        lv_aic_mpp_stream_close(&stream);
        return LV_RESULT_INVALID;
    }

    session = (lv_aic_mpp_session_t *)lv_malloc_zeroed(sizeof(*session));
    if (session == NULL) {
        lv_aic_mpp_stream_close(&stream);
        return LV_RESULT_INVALID;
    }

    cma_base = aicos_malloc_align(MEM_CMA, cma_size, CACHE_LINE_SIZE);
    if (cma_base == NULL) {
        lv_free(session);
        lv_aic_mpp_stream_close(&stream);
        return LV_RESULT_INVALID;
    }
    memset(cma_base, 0, cma_size);
    aicos_dcache_clean_invalid_range((unsigned long *)cma_base, (unsigned long)cma_size);

    session->allocation_base = cma_base;
    session->aligned_data = cma_base;
    session->cma_size = cma_size;
    session->heap_buf = NULL;
    session->width = (uint32_t)width;
    session->height = (uint32_t)height;
    session->color_format = lv_fmt;

    dec = mpp_decoder_create(codec);
    if (dec == NULL) {
        goto fail;
    }

    config.pix_fmt = mpp_fmt;
    /* file_len is bounded by MAX_FILE_BYTES above, so the int casts are safe. */
    config.bitstream_buffer_size = (int)lv_aic_mpp_align_up(file_len, 256U);
    config.packet_count = 1;
    config.extra_frame_num = 0;

    ext_alloc.frame_template.buf.size.width = width;
    ext_alloc.frame_template.buf.size.height = height;
    ext_alloc.frame_template.buf.format = mpp_fmt;
    ext_alloc.frame_template.buf.buf_type = MPP_PHY_ADDR;
    ext_alloc.frame_template.buf.phy_addr[0] = (unsigned int)(uintptr_t)cma_base;
    ext_alloc.frame_template.buf.stride[0] = stride;
    ext_alloc.ops.alloc_frame_buffer = lv_aic_mpp_alloc_ext_frame;
    ext_alloc.ops.free_frame_buffer = lv_aic_mpp_free_ext_frame;
    ext_alloc.ops.close_allocator = lv_aic_mpp_close_ext_allocator;
    ext_alloc.allocator.ops = &ext_alloc.ops;

    if (mpp_decoder_control(dec, MPP_DEC_INIT_CMD_SET_EXT_FRAME_ALLOCATOR,
                            (void *)&ext_alloc.allocator) != 0) {
        goto fail;
    }
    if (mpp_decoder_init(dec, &config) != 0) {
        goto fail;
    }
    if (mpp_decoder_get_packet(dec, &packet, (int)file_len) != 0 || packet.data == NULL) {
        goto fail;
    }
    if (lv_aic_mpp_stream_read(&stream, packet.data, file_len, &read_done) != LV_FS_RES_OK ||
        read_done != file_len) {
        goto fail;
    }
    packet.size = (int)file_len;
    packet.len = (int)file_len;
    packet.flag = PACKET_FLAG_EOS;
    if (mpp_decoder_put_packet(dec, &packet) != 0) {
        goto fail;
    }
    if (mpp_decoder_decode(dec) != 0) {
        goto fail;
    }
    memset(&frame, 0, sizeof(frame));
    if (mpp_decoder_get_frame(dec, &frame) != 0) {
        goto fail;
    }
    memcpy(&session->mpp_buf_copy, &frame.buf, sizeof(session->mpp_buf_copy));
    mpp_decoder_put_frame(dec, &frame);
    mpp_decoder_destory(dec);
    dec = NULL;
    lv_aic_mpp_stream_close(&stream);

    aicos_dcache_invalid_range((unsigned long *)cma_base, (unsigned long)cma_size);

    if (lv_draw_buf_init(&session->draw_buf, (uint32_t)width, (uint32_t)height,
                         lv_fmt, stride, cma_base, cma_size) != LV_RESULT_OK) {
        goto fail_session;
    }

    if (use_post_process && dsc != NULL) {
        lv_draw_buf_t *adjusted = lv_image_decoder_post_process(dsc, &session->draw_buf);
        if (adjusted == NULL) {
            goto fail_session;
        }
        if (adjusted != &session->draw_buf) {
            /* Post-process copied to a heap buffer; CMA is no longer needed. */
            aicos_free_align(MEM_CMA, session->allocation_base);
            session->allocation_base = NULL;
            session->aligned_data = NULL;
            session->cma_size = 0U;
            session->heap_buf = adjusted;
            dsc->decoded = adjusted;
            dsc->user_data = session;
            goto stats;
        }
    }

    dsc->decoded = &session->draw_buf;
    dsc->user_data = session;

stats:
    {
        uint32_t tick_end = tick_start;
#if AIC_LVGL_BSP_RTTHREAD
        tick_end = (uint32_t)rt_tick_get_millisecond();
#endif
        g_aic_mpp_last_stats.decode_time_ms = tick_end - tick_start;
        g_aic_mpp_last_stats.decoded_bytes = stride * (uint32_t)height;
        g_aic_mpp_last_stats.cma_bytes = cma_size;
        g_aic_mpp_last_stats.width = (uint32_t)width;
        g_aic_mpp_last_stats.height = (uint32_t)height;
        g_aic_mpp_last_stats.color_format = lv_fmt;
    }

    *out_session = session;
    return LV_RESULT_OK;

fail:
    if (dec != NULL) {
        mpp_decoder_destory(dec);
    }
    lv_aic_mpp_stream_close(&stream);
fail_session:
    lv_aic_mpp_session_release(session);
    return result;
}

static lv_result_t lv_aic_mpp_info_cb(lv_image_decoder_t *decoder,
                                      lv_image_decoder_dsc_t *dsc,
                                      lv_image_header_t *header)
{
    const char *src;
    lv_aic_mpp_stream_t stream;
    int width = 0;
    int height = 0;

    (void)decoder;
    if (dsc == NULL || header == NULL) {
        return LV_RESULT_INVALID;
    }
    if (dsc->src_type != LV_IMAGE_SRC_FILE) {
        return LV_RESULT_INVALID;
    }
    src = (const char *)dsc->src;
    if (src == NULL) {
        return LV_RESULT_INVALID;
    }

    if (lv_aic_mpp_is_jpeg_path(src)) {
        int components = 0;
        memset(&stream, 0, sizeof(stream));
        if (lv_aic_mpp_stream_open_file(&stream, src) != LV_FS_RES_OK) {
            return LV_RESULT_INVALID;
        }
        if (lv_aic_mpp_parse_jpeg_header(&stream, &width, &height,
                                         &components) != LV_RESULT_OK) {
            lv_aic_mpp_stream_close(&stream);
            return LV_RESULT_INVALID;
        }
        lv_aic_mpp_stream_close(&stream);
        if (width <= 0 || height <= 0 ||
            width > (int)LV_AIC_MPP_MAX_DIMENSION ||
            height > (int)LV_AIC_MPP_MAX_DIMENSION ||
            (uint64_t)width * (uint64_t)height > LV_AIC_MPP_MAX_PIXELS) {
            return LV_RESULT_INVALID;
        }
        header->w = (uint32_t)width;
        header->h = (uint32_t)height;
        header->cf = LV_COLOR_FORMAT_RGB888;
        header->stride = 0U;
        return LV_RESULT_OK;
    }

    if (lv_aic_mpp_is_png_path(src)) {
        lv_color_format_t cf = LV_COLOR_FORMAT_RGB888;
        memset(&stream, 0, sizeof(stream));
        if (lv_aic_mpp_stream_open_file(&stream, src) != LV_FS_RES_OK) {
            return LV_RESULT_INVALID;
        }
        if (lv_aic_mpp_parse_png_header(&stream, &width, &height, &cf) != LV_RESULT_OK) {
            lv_aic_mpp_stream_close(&stream);
            return LV_RESULT_INVALID;
        }
        lv_aic_mpp_stream_close(&stream);
        if (width <= 0 || height <= 0 ||
            width > (int)LV_AIC_MPP_MAX_DIMENSION ||
            height > (int)LV_AIC_MPP_MAX_DIMENSION ||
            (uint64_t)width * (uint64_t)height > LV_AIC_MPP_MAX_PIXELS) {
            return LV_RESULT_INVALID;
        }
        header->w = (uint32_t)width;
        header->h = (uint32_t)height;
        header->cf = cf;
        header->stride = 0U;
        return LV_RESULT_OK;
    }

    return LV_RESULT_INVALID;
}

static lv_result_t lv_aic_mpp_open_cb(lv_image_decoder_t *decoder,
                                      lv_image_decoder_dsc_t *dsc)
{
    const char *src;
    lv_aic_mpp_session_t *session = NULL;

    (void)decoder;
    if (dsc == NULL) {
        return LV_RESULT_INVALID;
    }
    if (dsc->src_type != LV_IMAGE_SRC_FILE) {
        return LV_RESULT_INVALID;
    }
    src = (const char *)dsc->src;
    if (src == NULL || dsc->header.w == 0U || dsc->header.h == 0U) {
        return LV_RESULT_INVALID;
    }

    if (lv_aic_mpp_is_jpeg_path(src)) {
        if (dsc->header.cf != LV_COLOR_FORMAT_RGB888) {
            return LV_RESULT_INVALID;
        }
        if (lv_aic_mpp_decode_file(src, MPP_CODEC_VIDEO_DECODER_MJPEG, MPP_FMT_RGB_888,
                                   LV_COLOR_FORMAT_RGB888, (int)dsc->header.w,
                                   (int)dsc->header.h, false, dsc,
                                   &session) != LV_RESULT_OK) {
            return LV_RESULT_INVALID;
        }
        return LV_RESULT_OK;
    }

    if (lv_aic_mpp_is_png_path(src)) {
        enum mpp_pixel_format mpp_fmt;
        if (dsc->header.cf != LV_COLOR_FORMAT_RGB888 &&
            dsc->header.cf != LV_COLOR_FORMAT_ARGB8888) {
            return LV_RESULT_INVALID;
        }
        if (!lv_aic_mpp_format_from_lvgl(dsc->header.cf, &mpp_fmt)) {
            return LV_RESULT_INVALID;
        }
        if (lv_aic_mpp_decode_file(src, MPP_CODEC_VIDEO_DECODER_PNG, mpp_fmt, dsc->header.cf,
                                   (int)dsc->header.w, (int)dsc->header.h, true, dsc,
                                   &session) != LV_RESULT_OK) {
            return LV_RESULT_INVALID;
        }
        return LV_RESULT_OK;
    }

    return LV_RESULT_INVALID;
}

static void lv_aic_mpp_close_cb(lv_image_decoder_t *decoder,
                                lv_image_decoder_dsc_t *dsc)
{
    lv_aic_mpp_session_t *session;

    (void)decoder;
    if (dsc == NULL) {
        return;
    }
    session = (lv_aic_mpp_session_t *)dsc->user_data;
    dsc->user_data = NULL;
    dsc->decoded = NULL;
    lv_aic_mpp_session_release(session);
}

int lv_aic_mpp_decoder_init(lv_image_decoder_t **decoder)
{
    if (decoder == NULL) {
        return LV_AIC_ERR_INVALID_STATE;
    }
    *decoder = NULL;

    if (g_aic_mpp_decoder != NULL) {
        return LV_AIC_ERR_INVALID_STATE;
    }

    g_aic_mpp_decoder = lv_image_decoder_create();
    if (g_aic_mpp_decoder == NULL) {
        return LV_AIC_ERR_NO_MEMORY;
    }

    lv_image_decoder_set_info_cb(g_aic_mpp_decoder, lv_aic_mpp_info_cb);
    lv_image_decoder_set_open_cb(g_aic_mpp_decoder, lv_aic_mpp_open_cb);
    lv_image_decoder_set_close_cb(g_aic_mpp_decoder, lv_aic_mpp_close_cb);

    memset(&g_aic_mpp_last_stats, 0, sizeof(g_aic_mpp_last_stats));
    *decoder = g_aic_mpp_decoder;
    return LV_AIC_OK;
}

void lv_aic_mpp_decoder_deinit(lv_image_decoder_t *decoder)
{
    if (decoder == NULL || decoder != g_aic_mpp_decoder) {
        return;
    }
    lv_image_decoder_delete(decoder);
    g_aic_mpp_decoder = NULL;
}

const lv_aic_mpp_decode_stats_t *lv_aic_mpp_decoder_last_stats(void)
{
    return &g_aic_mpp_last_stats;
}

/* Phase 2A has no custom image cache. OSAL's try_cma path calls this when CMA
 * is exhausted; returning false makes allocation fail safe instead of hanging.
 * This replaces the legacy lv_mpp_dec.c stub without pulling its cache. */
bool lv_drop_one_cached_image(void)
{
    return false;
}

#else /* defined(AIC_LVGL_USE_MPP_DEC) && AIC_LVGL_BSP_MPP */

int lv_aic_mpp_decoder_init(lv_image_decoder_t **decoder)
{
    if (decoder != NULL) {
        *decoder = NULL;
    }
    return LV_AIC_ERR_NO_BSP;
}

void lv_aic_mpp_decoder_deinit(lv_image_decoder_t *decoder)
{
    (void)decoder;
}

#endif /* defined(AIC_LVGL_USE_MPP_DEC) && AIC_LVGL_BSP_MPP */
