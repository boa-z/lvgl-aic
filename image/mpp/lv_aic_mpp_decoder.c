/**
 * @file lv_aic_mpp_decoder.c
 * @brief ArtInChip MPP JPEG/PNG decoder with FILE/RAW inputs and bounded cache.
 *
 * MPP produces native RGB888/ARGB8888, usable by software and GE2D consumers.
 * Encoded memory is borrowed; the packet is copied before synchronous decode.
 * Session-owned CMA or post-processed heap storage remains alive while readers
 * or the component LRU retain it. Invalidation defers release of active data.
 * No global allocator/cache handlers or SDK core files are replaced.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#define AIC_LVGL_USE_PRIVATE_API 1
#include "lv_aic_mpp_decoder.h"
#include "lv_aic_mpp_format.h"
#include "lv_aic_bmp_header.h"
/* Component-local software codec tag, never passed to mpp_decoder_create. */
#define LV_AIC_CODEC_BMP ((enum mpp_codec_type)-1)
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

typedef struct lv_aic_mpp_session {
    lv_draw_buf_t draw_buf;
    void *allocation_base;
    uint32_t cma_size;
    uint32_t stride;
    lv_draw_buf_t *heap_buf;
    uint32_t width;
    uint32_t height;
    lv_color_format_t color_format;
    struct lv_aic_mpp_session *older, *newer;
    const void *key;
    lv_image_src_t key_type;
    lv_image_decoder_args_t args;
    uint32_t refs, cache_cost;
    bool cached, invalidated;
} lv_aic_mpp_session_t;

typedef struct {
    /* First member: MPP passes this exact pointer to the callbacks. */
    struct frame_allocator allocator;
    struct alloc_ops ops;
    lv_aic_mpp_session_t *session;
} lv_aic_mpp_ext_allocator_t;

static lv_image_decoder_t *g_aic_mpp_decoder;
static lv_aic_mpp_decode_stats_t g_aic_mpp_last_stats;
static lv_aic_mpp_cma_stats_t g_aic_mpp_cma_stats;

#ifndef AIC_LVGL_MPP_CACHE_BYTES
#define AIC_LVGL_MPP_CACHE_BYTES (512U * 1024U)
#endif
#define LV_AIC_MPP_CACHE_ENTRIES 16U
static lv_aic_mpp_session_t *g_cache_oldest, *g_cache_newest;
static lv_aic_mpp_cache_stats_t g_cache = { .limit_bytes = AIC_LVGL_MPP_CACHE_BYTES };
static uint32_t g_open_sessions;
static bool lv_aic_mpp_cache_evict_one(void);

static uint32_t lv_aic_mpp_align_up(uint32_t value, uint32_t align)
{
    if (align == 0U) {
        return value;
    }
    return (value + align - 1U) & ~(align - 1U);
}

/* CMA lifecycle accounting. Every MEM_CMA allocation and its matching release
 * goes through this pair, so alloc_count/free_count stay symmetric by
 * construction and current_cma_bytes is exact for the wrapper's own buffers.
 * These counters are debug-only: they do not affect allocation behaviour. */
static void *lv_aic_mpp_cma_alloc(uint32_t size)
{
    void *ptr = aicos_malloc_align(MEM_CMA, size, CACHE_LINE_SIZE);
    while (!ptr && lv_aic_mpp_cache_evict_one())
        ptr = aicos_malloc_align(MEM_CMA, size, CACHE_LINE_SIZE);

    if (ptr != NULL) {
        g_aic_mpp_cma_stats.current_cma_bytes += size;
        g_aic_mpp_cma_stats.alloc_count++;
        if (g_aic_mpp_cma_stats.current_cma_bytes > g_aic_mpp_cma_stats.peak_cma_bytes) {
            g_aic_mpp_cma_stats.peak_cma_bytes = g_aic_mpp_cma_stats.current_cma_bytes;
        }
    }
    return ptr;
}

static void lv_aic_mpp_cma_free(void *ptr, uint32_t size)
{
    if (ptr == NULL) {
        return;
    }
    aicos_free_align(MEM_CMA, ptr);
    g_aic_mpp_cma_stats.current_cma_bytes -= size;
    g_aic_mpp_cma_stats.free_count++;
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

/* RAW descriptors borrow encoded bytes for the duration of open. Actual image
 * geometry comes from the bitstream, never from the caller's header. */
static lv_result_t lv_aic_mpp_source_open(const void *src, lv_aic_mpp_stream_t *stream,
                                          enum mpp_codec_type *codec)
{
    lv_fs_res_t result;
    lv_image_src_t type;
    if (!src || !stream || !codec) return LV_RESULT_INVALID;
    type = lv_image_src_get_type(src);
    if (type == LV_IMAGE_SRC_FILE) {
        if (lv_aic_mpp_is_jpeg_path(src)) *codec = MPP_CODEC_VIDEO_DECODER_MJPEG;
        else if (lv_aic_mpp_is_png_path(src)) *codec = MPP_CODEC_VIDEO_DECODER_PNG;
        else if (lv_aic_mpp_has_ext(src, "bmp")) *codec = LV_AIC_CODEC_BMP;
#ifdef AIC_MPP_AICP_DEC_ENABLE
        else if (lv_aic_mpp_has_ext(src, "aicp")) *codec = MPP_CODEC_VIDEO_DECODER_AICP;
#endif
        else return LV_RESULT_INVALID;
        result = lv_aic_mpp_stream_open_file(stream, src);
    }
    else if (type == LV_IMAGE_SRC_VARIABLE) {
        const lv_image_dsc_t *image = src;
        if ((image->header.cf != LV_COLOR_FORMAT_RAW && image->header.cf != LV_COLOR_FORMAT_RAW_ALPHA) ||
            (image->header.flags & LV_IMAGE_FLAGS_COMPRESSED) || !image->data ||
            image->data_size < 2 || image->data_size > LV_AIC_MPP_MAX_FILE_BYTES)
            return LV_RESULT_INVALID;
        if (image->data[0] == 0xff && image->data[1] == 0xd8)
            *codec = MPP_CODEC_VIDEO_DECODER_MJPEG;
        else if (image->data[0] == 'B' && image->data[1] == 'M')
            *codec = LV_AIC_CODEC_BMP;
        else if (image->data_size >= 8 && memcmp(image->data,"\x89PNG\r\n\x1a\n",8) == 0)
            *codec = MPP_CODEC_VIDEO_DECODER_PNG;
#ifdef AIC_MPP_AICP_DEC_ENABLE
        else if (image->data_size >= 6 && memcmp(image->data, "AICP", 4) == 0)
            *codec = MPP_CODEC_VIDEO_DECODER_AICP;
#endif
        else return LV_RESULT_INVALID;
        result = lv_aic_mpp_stream_open_memory(stream,image->data,image->data_size);
    }
    else return LV_RESULT_INVALID;
    if (result != LV_FS_RES_OK) return LV_RESULT_INVALID;
    if (!stream->size || stream->size > LV_AIC_MPP_MAX_FILE_BYTES) {
        lv_aic_mpp_stream_close(stream);
        return LV_RESULT_INVALID;
    }
    return LV_RESULT_OK;
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
    session->allocation_base = lv_aic_mpp_cma_alloc(session->cma_size);
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
            /* SOF has six fixed bytes followed by three bytes per component.
             * A grayscale SOF is shorter than the RGB SOF; never read into
             * the following marker to satisfy a fixed RGB-sized read. */
            if (size < 8U ||
                lv_aic_mpp_stream_read(stream, sof, 6U, &got) != LV_FS_RES_OK ||
                got != 6U) {
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
            if (*components != 1 && *components != 3 && *components != 4) {
                return LV_RESULT_INVALID;
            }
            uint32_t component_bytes = 3U * (uint32_t)*components;
            if (size != 8U + component_bytes ||
                lv_aic_mpp_stream_read(stream, sof, component_bytes, &got) != LV_FS_RES_OK ||
                got != component_bytes) return LV_RESULT_INVALID;
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
        lv_aic_mpp_cma_free(session->allocation_base, session->cma_size);
        session->allocation_base = NULL;
    }
    lv_free(session);
}

/* LRU ownership is independent of LVGL's generic cache: no global draw-buffer
 * handlers or SDK allocator hooks are replaced. Entries include decode args. */
static void lv_aic_mpp_cache_unlink(lv_aic_mpp_session_t *s)
{
    if (s->older) s->older->newer = s->newer; else g_cache_oldest = s->newer;
    if (s->newer) s->newer->older = s->older; else g_cache_newest = s->older;
}

static void lv_aic_mpp_cache_make_newest(lv_aic_mpp_session_t *s)
{
    s->newer = NULL; s->older = g_cache_newest;
    if (g_cache_newest) g_cache_newest->newer = s; else g_cache_oldest = s;
    g_cache_newest = s;
}

static void lv_aic_mpp_cache_remove(lv_aic_mpp_session_t *s)
{
    lv_aic_mpp_cache_unlink(s);
    g_cache.bytes -= s->cache_cost; g_cache.entries--;
    if (s->key_type == LV_IMAGE_SRC_FILE) lv_free((void *)s->key);
    s->cached = false;
    lv_aic_mpp_session_release(s);
}

static bool lv_aic_mpp_cache_evict_one(void)
{
    lv_aic_mpp_session_t *s;
    for (s = g_cache_oldest; s; s = s->newer) {
        if (!s->refs) {
            lv_aic_mpp_cache_remove(s); g_cache.evictions++;
            return true;
        }
    }
    return false;
}

static bool lv_aic_mpp_cache_key_matches(const lv_aic_mpp_session_t *s, const void *src)
{
    if (!src || s->key_type != lv_image_src_get_type(src)) return false;
    return s->key_type == LV_IMAGE_SRC_FILE ? strcmp(s->key,src) == 0 : s->key == src;
}

void lv_aic_mpp_cache_drop(const void *src)
{
    lv_aic_mpp_session_t *s = g_cache_oldest;
    /* File header entries retain decoder pointers and source dimensions. */
    lv_image_header_cache_drop(src);
    while (s) {
        lv_aic_mpp_session_t *next = s->newer;
        if (!src || lv_aic_mpp_cache_key_matches(s,src)) {
            s->invalidated = true;
            if (!s->refs) lv_aic_mpp_cache_remove(s);
        }
        s = next;
    }
}

void lv_aic_mpp_cache_set_limit(uint32_t bytes)
{
    g_cache.limit_bytes = bytes;
    if (!bytes) lv_aic_mpp_cache_drop(NULL);
    while (g_cache.bytes > bytes && lv_aic_mpp_cache_evict_one()) {}
}

const lv_aic_mpp_cache_stats_t *lv_aic_mpp_cache_stats(void) { return &g_cache; }

static bool lv_aic_mpp_cache_acquire(lv_image_decoder_dsc_t *dsc)
{
    lv_aic_mpp_session_t *s;
    if (dsc->args.no_cache || !g_cache.limit_bytes) return false;
    for (s = g_cache_newest; s; s = s->older) {
        if (!s->invalidated && lv_aic_mpp_cache_key_matches(s,dsc->src) &&
            s->args.premultiply == dsc->args.premultiply &&
            s->args.stride_align == dsc->args.stride_align &&
            s->args.use_indexed == dsc->args.use_indexed) {
            s->refs++; g_open_sessions++; g_cache.hits++;
            lv_aic_mpp_cache_unlink(s); lv_aic_mpp_cache_make_newest(s);
            dsc->decoded = s->heap_buf ? s->heap_buf : &s->draw_buf;
            dsc->user_data = s;
            return true;
        }
    }
    g_cache.misses++;
    return false;
}

static void lv_aic_mpp_cache_insert(lv_image_decoder_dsc_t *dsc, lv_aic_mpp_session_t *s)
{
    uint64_t cost;
    size_t key_bytes = dsc->src_type == LV_IMAGE_SRC_FILE ? strlen(dsc->src) + 1 : 0;
    if (dsc->args.no_cache || !g_cache.limit_bytes) return;
    cost = sizeof(*s) + key_bytes + (s->heap_buf ?
           s->heap_buf->data_size + sizeof(lv_draw_buf_t) : s->cma_size);
    if (cost > g_cache.limit_bytes) return;
    while (cost + g_cache.bytes > g_cache.limit_bytes || g_cache.entries >= LV_AIC_MPP_CACHE_ENTRIES) {
        if (!lv_aic_mpp_cache_evict_one()) return;
    }
    s->key = key_bytes ? lv_strdup(dsc->src) : dsc->src;
    if (!s->key) return; /* Allocation failure preserves the uncached decode. */
    s->key_type = dsc->src_type; s->args = dsc->args;
    s->cache_cost = (uint32_t)cost; s->cached = true;
    lv_aic_mpp_cache_make_newest(s); g_cache.entries++; g_cache.bytes += s->cache_cost;
}

static bool lv_aic_bmp_read_header(lv_aic_mpp_stream_t *stream, lv_aic_bmp_header_t *header)
{
    uint8_t bytes[54];
    uint32_t got = 0;
    return lv_aic_mpp_stream_read(stream, bytes, sizeof(bytes), &got) == LV_FS_RES_OK &&
           got == sizeof(bytes) &&
           lv_aic_bmp_parse_header(bytes, got, stream->size, header);
}

static lv_result_t lv_aic_mpp_decode_source(const void *src, enum mpp_codec_type codec,
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

    if (lv_aic_mpp_source_open(src, &stream, &codec) != LV_RESULT_OK) {
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

    if (codec == LV_AIC_CODEC_BMP) {
        lv_aic_bmp_header_t bmp;
        if (!lv_aic_bmp_read_header(&stream, &bmp) || bmp.width != session->width ||
            bmp.height != session->height ||
            (bmp.bpp == 24 ? LV_COLOR_FORMAT_RGB888 : LV_COLOR_FORMAT_ARGB8888) != lv_fmt) goto fail;
        ext_alloc.session = session;
        uint32_t output_stride = lv_aic_mpp_align_up(bmp.width * (bmp.bpp / 8U), 8U);
        if (lv_aic_mpp_alloc_ext_frame(&ext_alloc.allocator, &frame,
                                       output_stride, bmp.height, mpp_fmt) != 0) goto fail;
        for (uint32_t y = 0; y < bmp.height; y++) {
            uint32_t source_row = bmp.top_down ? y : bmp.height - 1U - y;
            uint32_t bytes = bmp.width * (bmp.bpp / 8U);
            if (lv_aic_mpp_stream_seek(&stream, bmp.offset + source_row * bmp.stride) != LV_FS_RES_OK ||
                lv_aic_mpp_stream_read(&stream, (uint8_t *)session->allocation_base + y * output_stride,
                                       bytes, &read_done) != LV_FS_RES_OK || read_done != bytes) goto fail;
        }
        /* Pixels were written by CPU, unlike the MPP hardware path. */
        aicos_dcache_clean_invalid_range((unsigned long *)session->allocation_base, session->cma_size);
        goto decoded_buffer;
    }
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
decoded_buffer:
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
            /* Post-process copied to a heap buffer; CMA is no longer needed.
             * Free before clearing cma_size so the counter sees the real size. */
            lv_aic_mpp_cma_free(session->allocation_base, session->cma_size);
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

    session->refs = 1;
    g_open_sessions++;
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
    lv_aic_mpp_stream_t stream = {0};
    enum mpp_codec_type codec;
    lv_color_format_t cf = LV_COLOR_FORMAT_RGB888;
    int width = 0, height = 0, components = 0;
    lv_result_t result;
    (void)decoder;
    if (!dsc || !header ||
        lv_aic_mpp_source_open(dsc->src,&stream,&codec) != LV_RESULT_OK)
        return LV_RESULT_INVALID;
    if (codec == LV_AIC_CODEC_BMP) {
        lv_aic_bmp_header_t bmp;
        result = lv_aic_bmp_read_header(&stream, &bmp) ? LV_RESULT_OK : LV_RESULT_INVALID;
        if (result == LV_RESULT_OK) {
            width = bmp.width; height = bmp.height;
            cf = bmp.bpp == 24 ? LV_COLOR_FORMAT_RGB888 : LV_COLOR_FORMAT_ARGB8888;
        }
    }
    else if (codec == MPP_CODEC_VIDEO_DECODER_MJPEG) {
        result = lv_aic_mpp_parse_jpeg_header(&stream,&width,&height,&components);
        /* Four-component AICP is not a CMYK JPEG decoder contract. */
        if (components == 4) result = LV_RESULT_INVALID;
    }
#ifdef AIC_MPP_AICP_DEC_ENABLE
    else if (codec == MPP_CODEC_VIDEO_DECODER_AICP) {
        uint8_t magic[4];
        uint32_t got = 0;
        result = LV_RESULT_INVALID;
        if (lv_aic_mpp_stream_read(&stream, magic, sizeof(magic), &got) == LV_FS_RES_OK &&
            got == sizeof(magic) && memcmp(magic, "AICP", 4) == 0) {
            result = lv_aic_mpp_parse_jpeg_header(&stream, &width, &height, &components);
            if (components == 4) {
#ifdef AIC_VE_DRV_V31
                cf = LV_COLOR_FORMAT_ARGB8888;
#else
                result = LV_RESULT_INVALID;
#endif
            }
        }
    }
#endif
    else result = lv_aic_mpp_parse_png_header(&stream,&width,&height,&cf);
    lv_aic_mpp_stream_close(&stream);
    if (result != LV_RESULT_OK || width <= 0 || height <= 0 ||
        width > (int)LV_AIC_MPP_MAX_DIMENSION || height > (int)LV_AIC_MPP_MAX_DIMENSION ||
        (uint64_t)width * height > LV_AIC_MPP_MAX_PIXELS) return LV_RESULT_INVALID;
    memset(header,0,sizeof(*header));
    header->magic = LV_IMAGE_HEADER_MAGIC;
    header->w = width; header->h = height; header->cf = cf;
    return LV_RESULT_OK;
}

static lv_result_t lv_aic_mpp_open_cb(lv_image_decoder_t *decoder,
                                      lv_image_decoder_dsc_t *dsc)
{
    lv_aic_mpp_stream_t stream = {0};
    lv_aic_mpp_session_t *session = NULL;
    enum mpp_codec_type codec;
    enum mpp_pixel_format mpp_fmt;
    (void)decoder;
    if (!dsc || !dsc->src || !dsc->header.w || !dsc->header.h)
        return LV_RESULT_INVALID;
    if (lv_aic_mpp_cache_acquire(dsc)) return LV_RESULT_OK;
    if (lv_aic_mpp_source_open(dsc->src,&stream,&codec) != LV_RESULT_OK)
        return LV_RESULT_INVALID;
    lv_aic_mpp_stream_close(&stream);
    if (!lv_aic_mpp_format_from_lvgl(dsc->header.cf,&mpp_fmt)) return LV_RESULT_INVALID;
    if (lv_aic_mpp_decode_source(dsc->src,codec,mpp_fmt,dsc->header.cf,
                                dsc->header.w,dsc->header.h,true,dsc,&session) != LV_RESULT_OK)
        return LV_RESULT_INVALID;
    lv_aic_mpp_cache_insert(dsc,session);
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
    if (!session) return;
    session->refs--; g_open_sessions--;
    if (!session->cached) lv_aic_mpp_session_release(session);
    else if (!session->refs && (session->invalidated || g_cache.bytes > g_cache.limit_bytes))
        lv_aic_mpp_cache_remove(session);
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

    g_aic_mpp_decoder->name = "AIC MPP";
    lv_image_decoder_set_info_cb(g_aic_mpp_decoder, lv_aic_mpp_info_cb);
    lv_image_decoder_set_open_cb(g_aic_mpp_decoder, lv_aic_mpp_open_cb);
    lv_image_decoder_set_close_cb(g_aic_mpp_decoder, lv_aic_mpp_close_cb);

    memset(&g_aic_mpp_last_stats, 0, sizeof(g_aic_mpp_last_stats));
    memset(&g_aic_mpp_cma_stats, 0, sizeof(g_aic_mpp_cma_stats));
    *decoder = g_aic_mpp_decoder;
    return LV_AIC_OK;
}

bool lv_aic_mpp_decoder_can_deinit(void) { return g_open_sessions == 0; }

void lv_aic_mpp_decoder_deinit(lv_image_decoder_t *decoder)
{
    if (decoder == NULL || decoder != g_aic_mpp_decoder) {
        return;
    }
    if (!lv_aic_mpp_decoder_can_deinit()) {
        LV_LOG_ERROR("Close all MPP image descriptors before decoder deinit");
        return;
    }
    lv_aic_mpp_cache_drop(NULL);
    lv_image_decoder_delete(decoder);
    g_aic_mpp_decoder = NULL;
}

const lv_aic_mpp_decode_stats_t *lv_aic_mpp_decoder_last_stats(void)
{
    return &g_aic_mpp_last_stats;
}

const lv_aic_mpp_cma_stats_t *lv_aic_mpp_cma_stats(void)
{
    return &g_aic_mpp_cma_stats;
}

void lv_aic_mpp_cma_stats_reset(void)
{
    uint32_t current = g_aic_mpp_cma_stats.current_cma_bytes;
    memset(&g_aic_mpp_cma_stats, 0, sizeof(g_aic_mpp_cma_stats));
    g_aic_mpp_cma_stats.current_cma_bytes = current;
    g_aic_mpp_cma_stats.peak_cma_bytes = current;
}

/* Legacy SDK hook may run outside the LVGL owner. Keep it inert: this decoder
 * retries its own allocations by evicting idle entries under decoder ownership. */
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

bool lv_aic_mpp_decoder_can_deinit(void) { return true; }

void lv_aic_mpp_decoder_deinit(lv_image_decoder_t *decoder)
{
    (void)decoder;
}

#endif /* defined(AIC_LVGL_USE_MPP_DEC) && AIC_LVGL_BSP_MPP */
