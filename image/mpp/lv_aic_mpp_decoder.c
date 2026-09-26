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

#define AIC_LVGL_USE_PRIVATE_API 1
#include "lv_aic_mpp_decoder.h"
#include "lv_aic_mpp_format.h"
#include "lv_aic_mpp_stream.h"

#if AIC_LVGL_USE_MPP_DEC && AIC_LVGL_BSP_MPP

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
    uint32_t cma_size;
    uint32_t stride;
    lv_draw_buf_t *heap_buf;
    uint32_t width;
    uint32_t height;
    lv_color_format_t color_format;
} lv_aic_mpp_session_t;

typedef struct {
    /* First member: MPP passes this exact pointer to the callbacks. */
    struct frame_allocator allocator;
    struct alloc_ops ops;
    lv_aic_mpp_session_t *session;
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

/* The SDK skips PNG chunk CRCs. Validate them before handing the packet to
 * MPP. This does not validate zlib Adler-32 or repair SDK VE error handling. */
static const uint32_t lv_aic_mpp_crc32_table[256] = {
    0x00000000U, 0x77073096U, 0xEE0E612CU, 0x990951BAU,
    0x076DC419U, 0x706AF48FU, 0xE963A535U, 0x9E6495A3U,
    0x0EDB8832U, 0x79DCB8A4U, 0xE0D5E91EU, 0x97D2D988U,
    0x09B64C2BU, 0x7EB17CBDU, 0xE7B82D07U, 0x90BF1D91U,
    0x1DB71064U, 0x6AB020F2U, 0xF3B97148U, 0x84BE41DEU,
    0x1ADAD47DU, 0x6DDDE4EBU, 0xF4D4B551U, 0x83D385C7U,
    0x136C9856U, 0x646BA8C0U, 0xFD62F97AU, 0x8A65C9ECU,
    0x14015C4FU, 0x63066CD9U, 0xFA0F3D63U, 0x8D080DF5U,
    0x3B6E20C8U, 0x4C69105EU, 0xD56041E4U, 0xA2677172U,
    0x3C03E4D1U, 0x4B04D447U, 0xD20D85FDU, 0xA50AB56BU,
    0x35B5A8FAU, 0x42B2986CU, 0xDBBBC9D6U, 0xACBCF940U,
    0x32D86CE3U, 0x45DF5C75U, 0xDCD60DCFU, 0xABD13D59U,
    0x26D930ACU, 0x51DE003AU, 0xC8D75180U, 0xBFD06116U,
    0x21B4F4B5U, 0x56B3C423U, 0xCFBA9599U, 0xB8BDA50FU,
    0x2802B89EU, 0x5F058808U, 0xC60CD9B2U, 0xB10BE924U,
    0x2F6F7C87U, 0x58684C11U, 0xC1611DABU, 0xB6662D3DU,
    0x76DC4190U, 0x01DB7106U, 0x98D220BCU, 0xEFD5102AU,
    0x71B18589U, 0x06B6B51FU, 0x9FBFE4A5U, 0xE8B8D433U,
    0x7807C9A2U, 0x0F00F934U, 0x9609A88EU, 0xE10E9818U,
    0x7F6A0DBBU, 0x086D3D2DU, 0x91646C97U, 0xE6635C01U,
    0x6B6B51F4U, 0x1C6C6162U, 0x856530D8U, 0xF262004EU,
    0x6C0695EDU, 0x1B01A57BU, 0x8208F4C1U, 0xF50FC457U,
    0x65B0D9C6U, 0x12B7E950U, 0x8BBEB8EAU, 0xFCB9887CU,
    0x62DD1DDFU, 0x15DA2D49U, 0x8CD37CF3U, 0xFBD44C65U,
    0x4DB26158U, 0x3AB551CEU, 0xA3BC0074U, 0xD4BB30E2U,
    0x4ADFA541U, 0x3DD895D7U, 0xA4D1C46DU, 0xD3D6F4FBU,
    0x4369E96AU, 0x346ED9FCU, 0xAD678846U, 0xDA60B8D0U,
    0x44042D73U, 0x33031DE5U, 0xAA0A4C5FU, 0xDD0D7CC9U,
    0x5005713CU, 0x270241AAU, 0xBE0B1010U, 0xC90C2086U,
    0x5768B525U, 0x206F85B3U, 0xB966D409U, 0xCE61E49FU,
    0x5EDEF90EU, 0x29D9C998U, 0xB0D09822U, 0xC7D7A8B4U,
    0x59B33D17U, 0x2EB40D81U, 0xB7BD5C3BU, 0xC0BA6CADU,
    0xEDB88320U, 0x9ABFB3B6U, 0x03B6E20CU, 0x74B1D29AU,
    0xEAD54739U, 0x9DD277AFU, 0x04DB2615U, 0x73DC1683U,
    0xE3630B12U, 0x94643B84U, 0x0D6D6A3EU, 0x7A6A5AA8U,
    0xE40ECF0BU, 0x9309FF9DU, 0x0A00AE27U, 0x7D079EB1U,
    0xF00F9344U, 0x8708A3D2U, 0x1E01F268U, 0x6906C2FEU,
    0xF762575DU, 0x806567CBU, 0x196C3671U, 0x6E6B06E7U,
    0xFED41B76U, 0x89D32BE0U, 0x10DA7A5AU, 0x67DD4ACCU,
    0xF9B9DF6FU, 0x8EBEEFF9U, 0x17B7BE43U, 0x60B08ED5U,
    0xD6D6A3E8U, 0xA1D1937EU, 0x38D8C2C4U, 0x4FDFF252U,
    0xD1BB67F1U, 0xA6BC5767U, 0x3FB506DDU, 0x48B2364BU,
    0xD80D2BDAU, 0xAF0A1B4CU, 0x36034AF6U, 0x41047A60U,
    0xDF60EFC3U, 0xA867DF55U, 0x316E8EEFU, 0x4669BE79U,
    0xCB61B38CU, 0xBC66831AU, 0x256FD2A0U, 0x5268E236U,
    0xCC0C7795U, 0xBB0B4703U, 0x220216B9U, 0x5505262FU,
    0xC5BA3BBEU, 0xB2BD0B28U, 0x2BB45A92U, 0x5CB36A04U,
    0xC2D7FFA7U, 0xB5D0CF31U, 0x2CD99E8BU, 0x5BDEAE1DU,
    0x9B64C2B0U, 0xEC63F226U, 0x756AA39CU, 0x026D930AU,
    0x9C0906A9U, 0xEB0E363FU, 0x72076785U, 0x05005713U,
    0x95BF4A82U, 0xE2B87A14U, 0x7BB12BAEU, 0x0CB61B38U,
    0x92D28E9BU, 0xE5D5BE0DU, 0x7CDCEFB7U, 0x0BDBDF21U,
    0x86D3D2D4U, 0xF1D4E242U, 0x68DDB3F8U, 0x1FDA836EU,
    0x81BE16CDU, 0xF6B9265BU, 0x6FB077E1U, 0x18B74777U,
    0x88085AE6U, 0xFF0F6A70U, 0x66063BCAU, 0x11010B5CU,
    0x8F659EFFU, 0xF862AE69U, 0x616BFFD3U, 0x166CCF45U,
    0xA00AE278U, 0xD70DD2EEU, 0x4E048354U, 0x3903B3C2U,
    0xA7672661U, 0xD06016F7U, 0x4969474DU, 0x3E6E77DBU,
    0xAED16A4AU, 0xD9D65ADCU, 0x40DF0B66U, 0x37D83BF0U,
    0xA9BCAE53U, 0xDEBB9EC5U, 0x47B2CF7FU, 0x30B5FFE9U,
    0xBDBDF21CU, 0xCABAC28AU, 0x53B39330U, 0x24B4A3A6U,
    0xBAD03605U, 0xCDD70693U, 0x54DE5729U, 0x23D967BFU,
    0xB3667A2EU, 0xC4614AB8U, 0x5D681B02U, 0x2A6F2B94U,
    0xB40BBE37U, 0xC30C8EA1U, 0x5A05DF1BU, 0x2D02EF8DU,
};

/* Standard PNG/zlib CRC-32 (reflected, poly 0xEDB88320). */
static uint32_t lv_aic_mpp_crc32(uint32_t crc, const uint8_t *buf, uint32_t len)
{
    uint32_t i;

    crc = ~crc;
    for (i = 0U; i < len; i++) {
        crc = lv_aic_mpp_crc32_table[(crc ^ buf[i]) & 0xFFU] ^ (crc >> 8);
    }
    return ~crc;
}

/* Verifies CRC32 over (chunk type || chunk data) for every chunk and requires
 * a terminating IEND. The signature is checked on the actual packet buffer. Any
 * truncated chunk, bad length, or CRC mismatch rejects the file. Trailing
 * bytes after IEND are ignored, matching common decoder leniency. */
static bool lv_aic_mpp_png_chunks_valid(const uint8_t *data, uint32_t len)
{
    uint32_t off = 8U;

    static const uint8_t signature[8] = {137U, 80U, 78U, 71U, 13U, 10U, 26U, 10U};

    if (data == NULL || len < 8U || memcmp(data, signature, 8U) != 0) {
        return false;
    }
    while (len - off >= 12U) {
        const uint8_t *type = data + off + 4U;
        uint32_t chunk_len = lv_aic_mpp_stream_u32_be(data + off);

        /* Need 4 (type) + chunk_len + 4 (CRC) beyond the length field. */
        if (chunk_len > len - off - 12U) {
            return false;
        }
        if (lv_aic_mpp_crc32(0U, type, 4U + chunk_len) !=
            lv_aic_mpp_stream_u32_be(type + 4U + chunk_len)) {
            return false;
        }
        if (memcmp(type, "IEND", 4U) == 0) {
            return chunk_len == 0U;
        }
        off += 12U + chunk_len;
    }
    return false;
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

    lv_aic_mpp_session_t *session;
    uint64_t bytes;

    if (p == NULL || frame == NULL || width <= 0 || height <= 0) {
        return -1;
    }
    ext = (lv_aic_mpp_ext_allocator_t *)p;
    session = ext->session;
    /* SDK frame_manager passes byte stride and padded height, NOT pixel
     * width. JPEG MCU padding can exceed the visible image dimensions. */
    bytes = (uint64_t)(uint32_t)width * (uint32_t)height;
    if (session == NULL || session->allocation_base != NULL ||
        bytes > LV_AIC_MPP_MAX_PIXELS * 4U ||
        (uint32_t)height < session->height ||
        (uint32_t)width < session->width * lv_aic_mpp_format_lvgl_bpp(session->color_format)) {
        return -1;
    }
    enum mpp_pixel_format expected;
    if (!lv_aic_mpp_format_from_lvgl(session->color_format, &expected) || format != expected) {
        return -1;
    }
    session->cma_size = lv_aic_mpp_align_up((uint32_t)bytes, CACHE_LINE_SIZE);
    session->allocation_base = aicos_malloc_align(MEM_CMA, session->cma_size, CACHE_LINE_SIZE);
    if (session->allocation_base == NULL) {
        return -1;
    }
    session->stride = (uint32_t)width;
    memset(session->allocation_base, 0, session->cma_size);
    aicos_dcache_clean_invalid_range((unsigned long *)session->allocation_base, session->cma_size);
    /* Preserve the frame manager's dimensions/metadata. */
    frame->buf.format = format;
    frame->buf.buf_type = MPP_PHY_ADDR;
    frame->buf.phy_addr[0] = (unsigned int)(uintptr_t)session->allocation_base;
    frame->buf.stride[0] = width;
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

    session = (lv_aic_mpp_session_t *)lv_malloc_zeroed(sizeof(*session));
    if (session == NULL) {
        lv_aic_mpp_stream_close(&stream);
        return LV_RESULT_INVALID;
    }

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

    ext_alloc.session = session;
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
    /* Reject invalid chunk CRCs before the packet reaches the SDK. */
    if (codec == MPP_CODEC_VIDEO_DECODER_PNG &&
        !lv_aic_mpp_png_chunks_valid((const uint8_t *)packet.data, file_len)) {
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
    mpp_decoder_put_frame(dec, &frame);
    mpp_decoder_destory(dec);
    dec = NULL;
    lv_aic_mpp_stream_close(&stream);

    cma_base = session->allocation_base;
    cma_size = session->cma_size;
    stride = session->stride;
    if (cma_base == NULL || stride == 0U) {
        goto fail_session;
    }
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
