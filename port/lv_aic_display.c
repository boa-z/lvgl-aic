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

#if defined(AIC_LVGL_USE_DISPLAY) && AIC_LVGL_BSP_MPP

#include <rtconfig.h>
#include <aic_core.h>
#include <aic_osal.h>
#include <mpp_fb.h>

#ifndef CACHE_LINE_SIZE
#define CACHE_LINE_SIZE 32U
#endif

static volatile uint32_t lv_aic_display_flush_count;

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
    bool use_pan_display;
    bool use_rotation;
    bool powered_on;
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

#if defined(LV_DISPLAY_ROTATE_EN) && defined(LV_ROTATE_DEGREE)
static void *lv_aic_alloc_cma(size_t size)
{
    return aicos_malloc_align(MEM_CMA, size, CACHE_LINE_SIZE);
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

static void lv_aic_present(lv_aic_display_ctx_t *ctx, unsigned int buffer_index)
{
    if (ctx->use_pan_display) {
        if (mpp_fb_ioctl(ctx->fb, AICFB_PAN_DISPLAY, &buffer_index) < 0) {
            LV_LOG_ERROR("AIC framebuffer pan failed");
        }
    }
    lv_aic_power_on(ctx);

    if (mpp_fb_ioctl(ctx->fb, AICFB_WAIT_FOR_VSYNC, 0) < 0) {
        LV_LOG_ERROR("AIC framebuffer VSync wait failed");
    }
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

        /* Phase 1 deliberately uses the LVGL software rotate path. GE2D is
         * introduced only after this baseline is validated. */
        lv_aic_cache_clean(active->data, ctx->rotation_buffer_size);
        lv_draw_rotate(active->data, destination,
                       lv_display_get_horizontal_resolution(display),
                       lv_display_get_vertical_resolution(display),
                       source_stride, destination_stride,
                       lv_display_get_rotation(display),
                       ctx->lv_color_format);
        lv_aic_cache_clean(destination, ctx->framebuffer_size);
        buffer_index = ctx->present_index;
    } else {
        void *framebuffer = lv_aic_framebuffer_at(ctx, 0U);
        buffer_index = (active->data == framebuffer) ? 0U : 1U;
        lv_aic_cache_clean(active->data, ctx->framebuffer_size);
    }

    lv_aic_present(ctx, buffer_index);
    lv_aic_display_flush_count++;
    if (ctx->use_rotation && ctx->use_pan_display) {
        ctx->present_index = (ctx->present_index == 0U) ? 1U : 0U;
    }
    lv_display_flush_ready(display);
}

int lv_aic_display_init(lv_display_t **display)
{
    lv_aic_display_ctx_t *ctx;
    lv_color_format_t color_format;
    void *buffer1;
    void *buffer2 = NULL;
#if defined(LV_DISPLAY_ROTATE_EN) && defined(LV_ROTATE_DEGREE)
    lv_display_rotation_t rotation = LV_DISPLAY_ROTATION_0;
    uint32_t rotation_buffer_size = 0U;
#endif

    if (display == NULL) {
        return LV_AIC_ERR_INVALID_STATE;
    }
    *display = NULL;

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

#if defined(LV_DISPLAY_ROTATE_EN) && defined(LV_ROTATE_DEGREE)
    rotation = (lv_display_rotation_t)(LV_ROTATE_DEGREE / 90);
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

    ctx->display = lv_display_create((int32_t)ctx->info.width, (int32_t)ctx->info.height);
    if (ctx->display == NULL) {
        LV_LOG_ERROR("lv_display_create failed");
        if (ctx->rotation_buffer != NULL) {
            aicos_free_align(MEM_CMA, ctx->rotation_buffer);
            ctx->rotation_buffer = NULL;
        }
        mpp_fb_close(ctx->fb);
        lv_free(ctx);
        return LV_AIC_ERR_DISPLAY;
    }

    lv_display_set_color_format(ctx->display, color_format);
    lv_display_set_flush_cb(ctx->display, lv_aic_flush_cb);
    lv_display_set_driver_data(ctx->display, ctx);
    lv_display_set_physical_resolution(ctx->display, (int32_t)ctx->info.width,
                                       (int32_t)ctx->info.height);
#if defined(LV_DISPLAY_ROTATE_EN) && defined(LV_ROTATE_DEGREE)
    lv_display_set_rotation(ctx->display, rotation);
#endif
    lv_display_set_buffers_with_stride(ctx->display, buffer1, buffer2,
                                       ctx->use_rotation ? ctx->rotation_buffer_size : ctx->framebuffer_size,
                                       ctx->use_rotation ? ctx->lv_buffer_stride : ctx->info.stride,
                                       LV_DISPLAY_RENDER_MODE_DIRECT);

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

    lv_display_set_driver_data(display, NULL);
    lv_display_delete(display);

    if (ctx->rotation_buffer != NULL) {
        aicos_free_align(MEM_CMA, ctx->rotation_buffer);
        ctx->rotation_buffer = NULL;
    }

    if (ctx->fb != NULL) {
        mpp_fb_close(ctx->fb);
        ctx->fb = NULL;
    }
    lv_free(ctx);
}

#else /* AIC_LVGL_USE_DISPLAY && AIC_LVGL_BSP_MPP */

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
