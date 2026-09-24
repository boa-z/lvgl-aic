/**
 * @file lv_aic_mpp_decoder.c
 * @brief ArtInChip MPP JPEG decoder for LVGL 9.6 (Phase 2A).
 *
 * FILE (.jpg/.jpeg) only. PNG arrives in the next commit. No cache, no GE2D,
 * no YUV hack: MPP is asked for native RGB888 so the SW renderer consumes
 * pixels directly.
 *
 * Buffer ownership: session owns CMA allocation_base; draw_buf.data points at
 * the aligned base; close frees the base pointer. All failures return
 * LV_RESULT_INVALID with no dangling buffer.
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

#define LV_AIC_MPP_MAX_JPEG_BYTES (8U * 1024U * 1024U)
#define LV_AIC_MPP_MAX_DIMENSION 4096U
#define LV_AIC_MPP_MAX_PIXELS (8U * 1024U * 1024U)

typedef struct {
    lv_draw_buf_t draw_buf;
    void *allocation_base;
    void *aligned_data;
    uint32_t cma_size;
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

static bool lv_aic_mpp_is_jpeg_path(const char *src)
{
    const char *dot;
    size_t len;

    if (src == NULL) {
        return false;
    }
    /* LVGL FILE sources carry a drive prefix like "A:/..."; accept it. */
    dot = strrchr(src, '.');
    if (dot == NULL || dot[1] == '\0') {
        return false;
    }
    len = strlen(dot);
    if (len == 4U) {
        return ((dot[1] == 'j' || dot[1] == 'J') &&
                (dot[2] == 'p' || dot[2] == 'P') &&
                (dot[3] == 'g' || dot[3] == 'G'));
    }
    if (len == 5U) {
        return ((dot[1] == 'j' || dot[1] == 'J') &&
                (dot[2] == 'p' || dot[2] == 'P') &&
                (dot[3] == 'e' || dot[3] == 'E') &&
                (dot[4] == 'g' || dot[4] == 'G'));
    }
    return false;
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

/* Parse SOF0/SOF1 to get dimensions + component count. Reuses the sampling
 * rules from the legacy port without copying its GE2D-coupled wrapper. */
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
                /* Phase 2A: gray + truecolor only; 4-component paths deferred. */
                return LV_RESULT_INVALID;
            }
            return LV_RESULT_OK;
        }
        /* Skip chunk payload (size includes its own 2 length bytes). */
        {
            uint32_t cur = stream->cursor;
            uint32_t next = cur + (uint32_t)size - 2U;
            if (lv_aic_mpp_stream_seek(stream, next) != LV_FS_RES_OK) {
                return LV_RESULT_INVALID;
            }
        }
        /* Guard against pathological marker loops. */
        if (stream->cursor > LV_AIC_MPP_MAX_JPEG_BYTES) {
            return LV_RESULT_INVALID;
        }
    }
}

static lv_result_t lv_aic_mpp_jpeg_info(const char *src, lv_image_header_t *header)
{
    lv_aic_mpp_stream_t stream;
    int width = 0;
    int height = 0;
    int components = 0;

    memset(&stream, 0, sizeof(stream));
    if (lv_aic_mpp_stream_open_file(&stream, src) != LV_FS_RES_OK) {
        return LV_RESULT_INVALID;
    }
    if (lv_aic_mpp_parse_jpeg_header(&stream, &width, &height, &components) != LV_RESULT_OK) {
        lv_aic_mpp_stream_close(&stream);
        return LV_RESULT_INVALID;
    }
    lv_aic_mpp_stream_close(&stream);

    if (width > (int)LV_AIC_MPP_MAX_DIMENSION || height > (int)LV_AIC_MPP_MAX_DIMENSION) {
        return LV_RESULT_INVALID;
    }
    if ((uint64_t)width * (uint64_t)height > LV_AIC_MPP_MAX_PIXELS) {
        /* Large-image downscale is Phase 2B; refuse loudly for now. */
        return LV_RESULT_INVALID;
    }

    header->w = (uint32_t)width;
    header->h = (uint32_t)height;
    header->cf = LV_COLOR_FORMAT_RGB888;
    header->stride = 0U; /* let core derive LVGL stride; open uses MPP stride */
    return LV_RESULT_OK;
}

static void lv_aic_mpp_session_release(lv_aic_mpp_session_t *session)
{
    if (session == NULL) {
        return;
    }
    if (session->allocation_base != NULL) {
        aicos_free_align(MEM_CMA, session->allocation_base);
        session->allocation_base = NULL;
        session->aligned_data = NULL;
    }
    lv_free(session);
}

static lv_result_t lv_aic_mpp_decode_jpeg_file(const char *src, int width, int height,
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
    if (file_len == 0U || file_len > LV_AIC_MPP_MAX_JPEG_BYTES) {
        /* Size probing needs SEEK_END+TELL; drivers without it fail safe. */
        lv_aic_mpp_stream_close(&stream);
        return LV_RESULT_INVALID;
    }

    stride = lv_aic_mpp_align_up((uint32_t)width * 3U, 16U);
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
    session->width = (uint32_t)width;
    session->height = (uint32_t)height;
    session->color_format = LV_COLOR_FORMAT_RGB888;

    dec = mpp_decoder_create(MPP_CODEC_VIDEO_DECODER_MJPEG);
    if (dec == NULL) {
        goto fail;
    }

    config.pix_fmt = MPP_FMT_RGB_888;
    config.bitstream_buffer_size = (int)lv_aic_mpp_align_up(file_len, 256U);
    config.packet_count = 1;
    config.extra_frame_num = 0;

    ext_alloc.frame_template.buf.size.width = width;
    ext_alloc.frame_template.buf.size.height = height;
    ext_alloc.frame_template.buf.format = MPP_FMT_RGB_888;
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
    /* Keep a copy for diagnostics; the pixels stay in our CMA buffer. */
    memcpy(&session->mpp_buf_copy, &frame.buf, sizeof(session->mpp_buf_copy));
    mpp_decoder_put_frame(dec, &frame);
    mpp_decoder_destory(dec);
    dec = NULL;
    lv_aic_mpp_stream_close(&stream);

    /* VE wrote via DMA; invalidate before the SW renderer reads. */
    aicos_dcache_invalid_range((unsigned long *)cma_base, (unsigned long)cma_size);

    if (lv_draw_buf_init(&session->draw_buf, (uint32_t)width, (uint32_t)height,
                         LV_COLOR_FORMAT_RGB888, stride, cma_base,
                         cma_size) != LV_RESULT_OK) {
        goto fail_session;
    }

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
        g_aic_mpp_last_stats.color_format = LV_COLOR_FORMAT_RGB888;
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

    (void)decoder;
    if (dsc == NULL || header == NULL) {
        return LV_RESULT_INVALID;
    }
    if (dsc->src_type != LV_IMAGE_SRC_FILE) {
        return LV_RESULT_INVALID;
    }
    src = (const char *)dsc->src;
    if (!lv_aic_mpp_is_jpeg_path(src)) {
        return LV_RESULT_INVALID;
    }
    return lv_aic_mpp_jpeg_info(src, header);
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
    if (!lv_aic_mpp_is_jpeg_path(src)) {
        return LV_RESULT_INVALID;
    }
    if (dsc->header.w == 0U || dsc->header.h == 0U) {
        return LV_RESULT_INVALID;
    }
    if (dsc->header.cf != LV_COLOR_FORMAT_RGB888) {
        return LV_RESULT_INVALID;
    }

    if (lv_aic_mpp_decode_jpeg_file(src, (int)dsc->header.w, (int)dsc->header.h,
                                    &session) != LV_RESULT_OK) {
        return LV_RESULT_INVALID;
    }

    dsc->decoded = &session->draw_buf;
    dsc->user_data = session;
    return LV_RESULT_OK;
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

#else /* AIC_LVGL_USE_MPP_DEC && AIC_LVGL_BSP_MPP */

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

#endif /* AIC_LVGL_USE_MPP_DEC && AIC_LVGL_BSP_MPP */
