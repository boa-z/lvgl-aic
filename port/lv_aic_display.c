/*
 * Copyright (c) 2024-2026, ArtInChip Technology Co., Ltd
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * Authors:  Ning Fang <ning.fang@artinchip.com>
 *
 * The implementation was reorganized for the standalone lvgl-aic component.
 * The original source path was:
 *   packages/artinchip/lvgl-ui/lvgl_v9/lv_drivers/lv_port_disp.c
 */

#include "lv_aic_display.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

#if AIC_LVGL_USE_DISPLAY && AIC_LVGL_BSP_MPP

#include <rtconfig.h>
#ifndef AIC_LVGL_DISPLAY_ROTATION
#if defined(LV_DISPLAY_ROTATE_EN) && defined(LV_ROTATE_DEGREE)
#define AIC_LVGL_DISPLAY_ROTATION (LV_ROTATE_DEGREE / 90)
#else
#define AIC_LVGL_DISPLAY_ROTATION 0
#endif
#endif
#if AIC_LVGL_DISPLAY_ROTATION < 0 || AIC_LVGL_DISPLAY_ROTATION > 3
#error "AIC_LVGL_DISPLAY_ROTATION must be 0..3"
#endif
/* rtconfig.h emits Kconfig bools as empty macros: test definedness only. */
#ifdef AIC_LVGL_VIRTUAL_RES
#define LV_AIC_VIRTUAL_RES 1
#if AIC_LVGL_DISPLAY_ROTATION != 0
#error "AIC_LVGL_VIRTUAL_RES requires AIC_LVGL_DISPLAY_ROTATION 0"
#endif
#else
#define LV_AIC_VIRTUAL_RES 0
#endif
#include "lv_aic_display_fit.h"
#include <aic_core.h>
#include <aic_osal.h>
#include <mpp_fb.h>
#if AIC_LVGL_USE_GE2D
#include "lv_draw_aic_ge2d_display.h"
#include "lv_draw_aic_ge2d.h"
#endif

#ifndef CACHE_LINE_SIZE
#define CACHE_LINE_SIZE 32U
#endif

static volatile uint32_t lv_aic_display_flush_count;
static uint32_t lv_aic_display_fps_start, lv_aic_display_fps_frames;
static int lv_aic_display_fps_value;

int lv_aic_display_fps(void)
{
    return lv_aic_display_fps_value;
}

typedef struct {
    struct mpp_fb *fb;
    struct aicfb_screeninfo info;
    lv_display_t *display;
    uint32_t framebuffer_size;
    uint8_t *rotation_buffer;
    size_t rotation_buffer_size;
    uint32_t lv_buffer_stride;
    lv_color_format_t lv_color_format;
    unsigned int present_index;
    unsigned int last_presented_index;
    bool last_presented_valid;
    bool use_pan_display;
    bool use_rotation;
    bool powered_on;
    bool dma_quarantined;
    /* Virtual resolution: LVGL renders vres_buffer (vres_w x vres_h) and
     * each present scales it into the fit rectangle of the panel. */
    bool use_vres;
    uint8_t *vres_buffer;
    size_t vres_buffer_size;
    int32_t vres_w, vres_h;
    lv_aic_display_fit_t vres_fit;
} lv_aic_display_ctx_t;

static uint32_t lv_aic_align_up(uint32_t value, uint32_t alignment)
{
    if (alignment == 0U) {
        return value;
    }
    return (value + alignment - 1U) & ~(alignment - 1U);
}

static bool lv_aic_format_to_lvgl(enum mpp_pixel_format format, lv_color_format_t *lv_format)
{
    switch (format) {
    case MPP_FMT_RGB_565:
        *lv_format = LV_COLOR_FORMAT_RGB565;
        return true;
    case MPP_FMT_RGB_888:
        *lv_format = LV_COLOR_FORMAT_RGB888;
        return true;
    case MPP_FMT_ARGB_8888:
        *lv_format = LV_COLOR_FORMAT_ARGB8888;
        return true;
    case MPP_FMT_XRGB_8888:
        *lv_format = LV_COLOR_FORMAT_XRGB8888;
        return true;
    default:
        return false;
    }
}

static void lv_aic_cache_clean(const void *address, size_t size)
{
    if ((address == NULL) || (size == 0U)) {
        return;
    }

    aicos_dcache_clean_invalid_range((unsigned long *)address,
                                     lv_aic_align_up((uint32_t)size, CACHE_LINE_SIZE));
}

#if AIC_LVGL_DISPLAY_ROTATION != 0 || LV_AIC_VIRTUAL_RES
static void *lv_aic_alloc_cma(size_t size)
{
    return aicos_malloc_align(MEM_CMA, size, CACHE_LINE_SIZE);
}
#endif

#if LV_AIC_VIRTUAL_RES
/* Nearest-neighbour fallback when GE2D declines the present scale. Slow, but
 * keeps the panel correct; the GE2D path is the expected one. */
static void lv_aic_vres_scale_cpu(lv_aic_display_ctx_t *ctx, const uint8_t *src,
                                  uint32_t src_stride, uint8_t *dst)
{
    const lv_aic_display_fit_t *fit = &ctx->vres_fit;
    const uint32_t bpp = lv_color_format_get_size(ctx->lv_color_format);
    for (int32_t y = 0; y < fit->h; y++) {
        const uint8_t *row = src + (size_t)(((int64_t)y * ctx->vres_h) / fit->h) * src_stride;
        uint8_t *out = dst + (size_t)(fit->y + y) * ctx->info.stride + (size_t)fit->x * bpp;
        for (int32_t x = 0; x < fit->w; x++) {
            memcpy(out + (size_t)x * bpp,
                   row + (size_t)(((int64_t)x * ctx->vres_w) / fit->w) * bpp, bpp);
        }
    }
}
#endif

static void *lv_aic_framebuffer_at(lv_aic_display_ctx_t *ctx, unsigned int index)
{
    if ((ctx == NULL) || (ctx->info.framebuffer == NULL)) {
        return NULL;
    }
    return ctx->info.framebuffer + ((size_t)index * ctx->framebuffer_size);
}

static void lv_aic_power_on(lv_aic_display_ctx_t *ctx)
{
    if (ctx->powered_on) {
        return;
    }

    if (mpp_fb_ioctl(ctx->fb, AICFB_POWERON, 0) < 0) {
        LV_LOG_ERROR("AIC framebuffer power-on failed");
        return;
    }
    ctx->powered_on = true;
}

static bool lv_aic_present(lv_aic_display_ctx_t *ctx, unsigned int buffer_index)
{
    bool ok = true;
    if (ctx->use_pan_display) {
        if (mpp_fb_ioctl(ctx->fb, AICFB_PAN_DISPLAY, &buffer_index) < 0) {
            LV_LOG_ERROR("AIC framebuffer pan failed");
            ok = false;
        }
    }
    lv_aic_power_on(ctx);

    if (mpp_fb_ioctl(ctx->fb, AICFB_WAIT_FOR_VSYNC, 0) < 0) {
        LV_LOG_ERROR("AIC framebuffer VSync wait failed");
        ok = false;
    }
    return ok && ctx->powered_on;
}

void lv_aic_display_flush_count_reset(void)
{
    lv_aic_display_flush_count = 0U;
}

uint32_t lv_aic_display_flush_count_get(void)
{
    return lv_aic_display_flush_count;
}

static void lv_aic_flush_cb(lv_display_t *display, const lv_area_t *area, uint8_t *px_map)
{
    lv_aic_display_ctx_t *ctx = (lv_aic_display_ctx_t *)lv_display_get_driver_data(display);
    lv_draw_buf_t *active;
    unsigned int buffer_index = 0U;

    (void)area;
    (void)px_map;

    if (ctx == NULL) {
        if (display != NULL) {
            lv_display_flush_ready(display);
        }
        return;
    }

    if (ctx->dma_quarantined) return; /* Keep LVGL source ownership until reboot. */

    if (!lv_display_flush_is_last(display)) {
        lv_display_flush_ready(display);
        return;
    }

    active = lv_display_get_buf_active(display);
    if (active == NULL) {
        LV_LOG_ERROR("LVGL returned no active display buffer");
        lv_display_flush_ready(display);
        return;
    }

    if (ctx->use_rotation) {
        void *destination = lv_aic_framebuffer_at(ctx, ctx->present_index);
        const int32_t source_stride = (int32_t)ctx->lv_buffer_stride;
        const int32_t destination_stride = (int32_t)ctx->info.stride;

        int rotated = 0;
#if AIC_LVGL_USE_GE2D
        lv_draw_buf_t source = *active;
        lv_draw_buf_t target = {0};
        source.header.w = lv_display_get_horizontal_resolution(display);
        source.header.h = lv_display_get_vertical_resolution(display);
        source.header.stride = source_stride;
        target.data = destination;
        target.data_size = ctx->framebuffer_size;
        target.header.w = ctx->info.width;
        target.header.h = ctx->info.height;
        target.header.stride = destination_stride;
        target.header.cf = ctx->lv_color_format;
        rotated = lv_draw_aic_ge2d_display_rotate(&source, &target, lv_display_get_rotation(display));
        if (rotated < 0) {
            LV_LOG_ERROR("GE display DMA fault; retaining buffers until reboot");
            ctx->last_presented_valid = false;
            ctx->dma_quarantined = true;
            /* Completion is unknown: do not release LVGL's source for reuse. */
            return;
        }
#endif
        if (!rotated) {
            lv_aic_cache_clean(active->data, ctx->rotation_buffer_size);
            lv_draw_rotate(active->data, destination,
                           lv_display_get_horizontal_resolution(display),
                           lv_display_get_vertical_resolution(display),
                           source_stride, destination_stride,
                           lv_display_get_rotation(display),
                           ctx->lv_color_format);
            lv_aic_cache_clean(destination, ctx->framebuffer_size);
        }
        buffer_index = ctx->present_index;
#if LV_AIC_VIRTUAL_RES
    } else if (ctx->use_vres) {
        /* DIRECT mode keeps the whole virtual frame current, so every present
         * scales it entirely: no seams from partial filtered updates. */
        uint8_t *destination = lv_aic_framebuffer_at(ctx, ctx->present_index);
        const uint32_t source_stride = ctx->lv_buffer_stride;
        int scaled = 0;
#if AIC_LVGL_USE_GE2D
        lv_draw_buf_t source = *active;
        lv_draw_buf_t target = {0};
        source.header.w = (uint32_t)ctx->vres_w;
        source.header.h = (uint32_t)ctx->vres_h;
        source.header.stride = source_stride;
        target.data = destination;
        target.data_size = ctx->framebuffer_size;
        target.header.w = ctx->info.width;
        target.header.h = ctx->info.height;
        target.header.stride = ctx->info.stride;
        target.header.cf = ctx->lv_color_format;
        scaled = lv_draw_aic_ge2d_display_scale(&source, &target, ctx->vres_fit.x,
                                                ctx->vres_fit.y, ctx->vres_fit.w,
                                                ctx->vres_fit.h);
        if (scaled < 0) {
            LV_LOG_ERROR("GE display DMA fault; retaining buffers until reboot");
            ctx->last_presented_valid = false;
            ctx->dma_quarantined = true;
            return;
        }
#endif
        if (!scaled) {
            lv_aic_cache_clean(active->data, ctx->vres_buffer_size);
            lv_aic_vres_scale_cpu(ctx, active->data, source_stride, destination);
            lv_aic_cache_clean(destination, ctx->framebuffer_size);
        }
        buffer_index = ctx->present_index;
#endif
    } else {
        void *framebuffer = lv_aic_framebuffer_at(ctx, 0U);
        buffer_index = (active->data == framebuffer) ? 0U : 1U;
        lv_aic_cache_clean(active->data, ctx->framebuffer_size);
    }

    ctx->last_presented_valid = lv_aic_present(ctx, buffer_index);
    if (ctx->last_presented_valid) ctx->last_presented_index = buffer_index;
    lv_aic_display_flush_count++;
    {
        /* Presented frames over the last full second (SDK fbdev_draw_fps). */
        uint32_t now = lv_tick_get();
        lv_aic_display_fps_frames++;
        if (lv_tick_diff(now, lv_aic_display_fps_start) >= 1000U) {
            lv_aic_display_fps_value = (int)lv_aic_display_fps_frames;
            lv_aic_display_fps_frames = 0U;
            lv_aic_display_fps_start = now;
        }
    }
    if ((ctx->use_rotation || ctx->use_vres) && ctx->use_pan_display) {
        ctx->present_index = (ctx->present_index == 0U) ? 1U : 0U;
    }
    lv_display_flush_ready(display);
}

void lv_aic_display_panel_size(lv_display_t *display, int32_t *width, int32_t *height)
{
    lv_aic_display_ctx_t *ctx = display ? lv_display_get_driver_data(display) : NULL;
    if (width == NULL || height == NULL) {
        return;
    }
    if (ctx != NULL) {
        *width = (int32_t)ctx->info.width;
        *height = (int32_t)ctx->info.height;
    } else if (display != NULL) {
        *width = lv_display_get_original_horizontal_resolution(display);
        *height = lv_display_get_original_vertical_resolution(display);
    }
}

void lv_aic_display_panel_to_logical(lv_display_t *display, int32_t *x, int32_t *y)
{
    lv_aic_display_ctx_t *ctx = display ? lv_display_get_driver_data(display) : NULL;
    if (ctx != NULL && ctx->use_vres) {
        lv_aic_display_fit_map(&ctx->vres_fit, ctx->vres_w, ctx->vres_h, x, y);
    }
}

int lv_aic_display_snapshot(lv_display_t *display, lv_draw_buf_t *copy, uint32_t *frame)
{
    lv_aic_display_ctx_t *ctx;
    void *source, *data;
    if (!display || !copy || !frame) return LV_AIC_ERR_INVALID_STATE;
    memset(copy, 0, sizeof(*copy));
    ctx = lv_display_get_driver_data(display);
    if (!ctx || !ctx->last_presented_valid) return LV_AIC_ERR_DISPLAY;
    data = aicos_malloc_align(MEM_CMA, ctx->framebuffer_size, CACHE_LINE_SIZE);
    if (!data) return LV_AIC_ERR_NO_MEMORY;
    source = lv_aic_framebuffer_at(ctx, ctx->last_presented_index);
    /* All CPU/GE work completed before present. Refresh the CPU view of
     * scanout without writing stale cached pixels over the GE output. */
    aicos_dcache_invalid_range((unsigned long *)source,
                              lv_aic_align_up(ctx->framebuffer_size, CACHE_LINE_SIZE));
    memcpy(data, source, ctx->framebuffer_size);
    if (lv_draw_buf_init(copy, ctx->info.width, ctx->info.height,
                         ctx->lv_color_format, ctx->info.stride, data,
                         ctx->framebuffer_size) != LV_RESULT_OK) {
        aicos_free_align(MEM_CMA, data);
        memset(copy, 0, sizeof(*copy));
        return LV_AIC_ERR_DISPLAY;
    }
    *frame = lv_aic_display_flush_count;
    return LV_AIC_OK;
}

void lv_aic_display_snapshot_free(lv_draw_buf_t *copy)
{
    if (copy && copy->data) {
        aicos_free_align(MEM_CMA, copy->data);
        memset(copy, 0, sizeof(*copy));
    }
}

int lv_aic_display_init(lv_display_t **display)
{
    lv_aic_display_ctx_t *ctx;
    lv_color_format_t color_format;
    void *buffer1;
    void *buffer2 = NULL;
#if AIC_LVGL_DISPLAY_ROTATION != 0
    lv_display_rotation_t rotation = LV_DISPLAY_ROTATION_0;
    uint32_t rotation_buffer_size = 0U;
#endif

    if (display == NULL) {
        return LV_AIC_ERR_INVALID_STATE;
    }
    *display = NULL;
#if AIC_LVGL_USE_GE2D
    if (lv_draw_aic_ge2d_faulted()) return LV_AIC_ERR_DISPLAY;
#endif

    ctx = (lv_aic_display_ctx_t *)lv_malloc_zeroed(sizeof(*ctx));
    if (ctx == NULL) {
        return LV_AIC_ERR_NO_MEMORY;
    }

    ctx->fb = mpp_fb_open();
    if (ctx->fb == NULL) {
        LV_LOG_ERROR("mpp_fb_open failed");
        lv_free(ctx);
        return LV_AIC_ERR_DISPLAY;
    }

    if (mpp_fb_ioctl(ctx->fb, AICFB_GET_SCREENINFO, &ctx->info) < 0) {
        LV_LOG_ERROR("AICFB_GET_SCREENINFO failed");
        mpp_fb_close(ctx->fb);
        lv_free(ctx);
        return LV_AIC_ERR_DISPLAY;
    }

    if ((ctx->info.width == 0U) || (ctx->info.height == 0U) ||
        (ctx->info.framebuffer == NULL) || (ctx->info.stride == 0U)) {
        LV_LOG_ERROR("invalid AIC framebuffer information");
        mpp_fb_close(ctx->fb);
        lv_free(ctx);
        return LV_AIC_ERR_DISPLAY;
    }

    if (!lv_aic_format_to_lvgl(ctx->info.format, &color_format)) {
        LV_LOG_ERROR("unsupported AIC framebuffer format: %d", ctx->info.format);
        mpp_fb_close(ctx->fb);
        lv_free(ctx);
        return LV_AIC_ERR_UNSUPPORTED;
    }

    if (ctx->info.height > (UINT32_MAX / ctx->info.stride)) {
        LV_LOG_ERROR("AIC framebuffer size overflows 32-bit arithmetic");
        mpp_fb_close(ctx->fb);
        lv_free(ctx);
        return LV_AIC_ERR_DISPLAY;
    }
    ctx->framebuffer_size = ctx->info.height * ctx->info.stride;
    ctx->lv_color_format = color_format;
    ctx->present_index = 0U;
    if (ctx->info.smem_len < ctx->framebuffer_size) {
        LV_LOG_ERROR("AIC framebuffer length is smaller than the visible frame");
        mpp_fb_close(ctx->fb);
        lv_free(ctx);
        return LV_AIC_ERR_DISPLAY;
    }

#if defined(AIC_PAN_DISPLAY)
    /* The BSP reports smem_len for one plane even when PAN_DISPLAY has
     * reserved two planes. The second plane follows the first plane in the
     * BSP allocation; do not reject the configuration based on smem_len. */
    ctx->use_pan_display = true;
    buffer1 = lv_aic_framebuffer_at(ctx, 0U);
    buffer2 = lv_aic_framebuffer_at(ctx, 1U);
#else
    buffer1 = lv_aic_framebuffer_at(ctx, 0U);
#endif

#if AIC_LVGL_DISPLAY_ROTATION != 0
    rotation = (lv_display_rotation_t)AIC_LVGL_DISPLAY_ROTATION;
    ctx->use_rotation = (rotation != LV_DISPLAY_ROTATION_0);
    if (ctx->use_rotation) {
        int32_t logical_width = (int32_t)ctx->info.width;
        int32_t logical_height = (int32_t)ctx->info.height;
        if ((rotation == LV_DISPLAY_ROTATION_90) || (rotation == LV_DISPLAY_ROTATION_270)) {
            const int32_t temporary = logical_width;
            logical_width = logical_height;
            logical_height = temporary;
        }
        ctx->lv_buffer_stride = lv_draw_buf_width_to_stride((uint32_t)logical_width, color_format);
        {
            /* lv_display_set_buffers_with_stride() validates against the
             * original height before LVGL reshapes the layer for rotation.
             * Reserve the larger of the two row counts so both validations
             * remain valid for portrait and landscape panels. */
            const uint32_t buffer_height = LV_MAX(ctx->info.height, (uint32_t)logical_height);
            if (buffer_height > (UINT32_MAX / ctx->lv_buffer_stride)) {
                LV_LOG_ERROR("LVGL software rotation buffer size overflows 32-bit arithmetic");
                mpp_fb_close(ctx->fb);
                lv_free(ctx);
                return LV_AIC_ERR_NO_MEMORY;
            }
            rotation_buffer_size = ctx->lv_buffer_stride * buffer_height;
        }
        ctx->rotation_buffer = (uint8_t *)lv_aic_alloc_cma(rotation_buffer_size);
        if (ctx->rotation_buffer == NULL) {
            LV_LOG_ERROR("failed to allocate LVGL software rotation buffer");
            mpp_fb_close(ctx->fb);
            lv_free(ctx);
            return LV_AIC_ERR_NO_MEMORY;
        }
        ctx->rotation_buffer_size = rotation_buffer_size;
        buffer1 = ctx->rotation_buffer;
        buffer2 = NULL;
    } else {
        /* LVGL's software rotate helper intentionally does not copy for 0°.
         * Keep the normal direct framebuffer path in that case. */
        ctx->lv_buffer_stride = ctx->info.stride;
        buffer1 = lv_aic_framebuffer_at(ctx, 0U);
#if defined(AIC_PAN_DISPLAY)
        buffer2 = lv_aic_framebuffer_at(ctx, 1U);
#endif
    }
#else
    ctx->lv_buffer_stride = ctx->info.stride;
#endif

#if LV_AIC_VIRTUAL_RES
    {
        const int32_t vw = AIC_LVGL_VIRTUAL_HRES;
        const int32_t vh = AIC_LVGL_VIRTUAL_VRES;
        if ((vw != (int32_t)ctx->info.width || vh != (int32_t)ctx->info.height) &&
            lv_aic_display_fit(vw, vh, (int32_t)ctx->info.width, (int32_t)ctx->info.height,
                               &ctx->vres_fit)) {
            const uint32_t stride = lv_draw_buf_width_to_stride((uint32_t)vw, color_format);
            const unsigned planes = ctx->use_pan_display ? 2U : 1U;
            if ((uint32_t)vh > (UINT32_MAX / stride)) {
                LV_LOG_ERROR("virtual resolution buffer size overflows 32-bit arithmetic");
                mpp_fb_close(ctx->fb);
                lv_free(ctx);
                return LV_AIC_ERR_NO_MEMORY;
            }
            ctx->vres_buffer_size = (size_t)stride * (uint32_t)vh;
            ctx->vres_buffer = (uint8_t *)lv_aic_alloc_cma(ctx->vres_buffer_size);
            if (ctx->vres_buffer == NULL) {
                LV_LOG_ERROR("failed to allocate the %dx%d virtual-resolution buffer",
                             (int)vw, (int)vh);
                mpp_fb_close(ctx->fb);
                lv_free(ctx);
                return LV_AIC_ERR_NO_MEMORY;
            }
            memset(ctx->vres_buffer, 0, ctx->vres_buffer_size);
            lv_aic_cache_clean(ctx->vres_buffer, ctx->vres_buffer_size);
            /* The fit rectangle is rewritten every present; the borders
             * around it are cleared once on every scanout plane. */
            for (unsigned i = 0; i < planes; i++) {
                void *plane = lv_aic_framebuffer_at(ctx, i);
                memset(plane, 0, ctx->framebuffer_size);
                lv_aic_cache_clean(plane, ctx->framebuffer_size);
            }
            ctx->use_vres = true;
            ctx->vres_w = vw;
            ctx->vres_h = vh;
            ctx->lv_buffer_stride = stride;
            buffer1 = ctx->vres_buffer;
            buffer2 = NULL;
        }
    }
#endif

    ctx->display = ctx->use_vres ? lv_display_create(ctx->vres_w, ctx->vres_h)
                                 : lv_display_create((int32_t)ctx->info.width,
                                                     (int32_t)ctx->info.height);
    if (ctx->display == NULL) {
        LV_LOG_ERROR("lv_display_create failed");
        if (ctx->rotation_buffer != NULL) {
            aicos_free_align(MEM_CMA, ctx->rotation_buffer);
            ctx->rotation_buffer = NULL;
        }
        if (ctx->vres_buffer != NULL) {
            aicos_free_align(MEM_CMA, ctx->vres_buffer);
            ctx->vres_buffer = NULL;
        }
        mpp_fb_close(ctx->fb);
        lv_free(ctx);
        return LV_AIC_ERR_DISPLAY;
    }

    lv_display_set_color_format(ctx->display, color_format);
    lv_display_set_flush_cb(ctx->display, lv_aic_flush_cb);
    lv_display_set_driver_data(ctx->display, ctx);
    if (ctx->use_vres) {
        lv_display_set_physical_resolution(ctx->display, ctx->vres_w, ctx->vres_h);
    } else {
        lv_display_set_physical_resolution(ctx->display, (int32_t)ctx->info.width,
                                           (int32_t)ctx->info.height);
    }
#if AIC_LVGL_DISPLAY_ROTATION != 0
    lv_display_set_rotation(ctx->display, rotation);
#endif
    lv_display_set_buffers_with_stride(ctx->display, buffer1, buffer2,
                                       ctx->use_rotation ? ctx->rotation_buffer_size :
                                       ctx->use_vres ? ctx->vres_buffer_size : ctx->framebuffer_size,
                                       (ctx->use_rotation || ctx->use_vres) ? ctx->lv_buffer_stride
                                                                            : ctx->info.stride,
                                       LV_DISPLAY_RENDER_MODE_DIRECT);
    if (ctx->use_vres) {
        LV_LOG_USER("virtual resolution %dx%d -> panel %ux%u at (%d,%d) %dx%d",
                    (int)ctx->vres_w, (int)ctx->vres_h, (unsigned)ctx->info.width,
                    (unsigned)ctx->info.height, (int)ctx->vres_fit.x, (int)ctx->vres_fit.y,
                    (int)ctx->vres_fit.w, (int)ctx->vres_fit.h);
    }

    *display = ctx->display;
    return LV_AIC_OK;
}

void lv_aic_display_deinit(lv_display_t *display)
{
    lv_aic_display_ctx_t *ctx;

    if (display == NULL) {
        return;
    }

    ctx = (lv_aic_display_ctx_t *)lv_display_get_driver_data(display);
    if (ctx == NULL) {
        return;
    }

    /* Draw-unit faults can precede flush and still reference this display's
     * target layer. The local rotation flag alone cannot prove DMA quiescence. */
#if AIC_LVGL_USE_GE2D
    if (lv_draw_aic_ge2d_faulted()) ctx->dma_quarantined = true;
#endif
    if (ctx->dma_quarantined) {
        ctx->last_presented_valid = false;
        LV_LOG_ERROR("GE display DMA fault; deinit requires reboot");
        return;
    }
    lv_display_set_driver_data(display, NULL);
    lv_display_delete(display);

    if (ctx->rotation_buffer != NULL) {
        aicos_free_align(MEM_CMA, ctx->rotation_buffer);
        ctx->rotation_buffer = NULL;
    }
    if (ctx->vres_buffer != NULL) {
        aicos_free_align(MEM_CMA, ctx->vres_buffer);
        ctx->vres_buffer = NULL;
    }

    if (ctx->fb != NULL) {
        mpp_fb_close(ctx->fb);
        ctx->fb = NULL;
    }
    lv_free(ctx);
}

#else /* AIC_LVGL_USE_DISPLAY && AIC_LVGL_BSP_MPP */

void lv_aic_display_panel_size(lv_display_t *display, int32_t *width, int32_t *height)
{
    if (display != NULL && width != NULL && height != NULL) {
        *width = lv_display_get_original_horizontal_resolution(display);
        *height = lv_display_get_original_vertical_resolution(display);
    }
}

void lv_aic_display_panel_to_logical(lv_display_t *display, int32_t *x, int32_t *y)
{
    (void)display; (void)x; (void)y;
}

int lv_aic_display_fps(void)
{
    return 0;
}

int lv_aic_display_snapshot(lv_display_t *display, lv_draw_buf_t *copy, uint32_t *frame)
{
    (void)display; (void)copy; (void)frame;
    return LV_AIC_ERR_NO_BSP;
}
void lv_aic_display_snapshot_free(lv_draw_buf_t *copy) { (void)copy; }

int lv_aic_display_init(lv_display_t **display)
{
    if (display != NULL) {
        *display = NULL;
    }
    return LV_AIC_ERR_NO_BSP;
}

void lv_aic_display_flush_count_reset(void)
{
}

uint32_t lv_aic_display_flush_count_get(void)
{
    return 0U;
}

void lv_aic_display_deinit(lv_display_t *display)
{
    (void)display;
}

#endif /* AIC_LVGL_USE_DISPLAY && AIC_LVGL_BSP_MPP */
