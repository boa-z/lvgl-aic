/*
 * Copyright (c) 2024-2026, ArtInChip Technology Co., Ltd
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * Authors:  Ning Fang <ning.fang@artinchip.com>
 *
 * The implementation was reorganized for the standalone lvgl-aic component.
 * The original source paths were:
 *   packages/artinchip/lvgl-ui/lvgl_v9/lv_drivers/lv_port_indev.c
 *   packages/artinchip/lvgl-ui/aic_drivers/lv_tpc_run.c
 */

#include "lv_aic_indev.h"

#include <stdbool.h>
#include <stdint.h>

#if AIC_LVGL_USE_TOUCH && AIC_LVGL_BSP_RTTHREAD

#include <rtconfig.h>
#include <rtdevice.h>
#include <rtthread.h>
#include <drivers/touch.h>
#include <aic_osal.h>

#ifndef AIC_TOUCH_PANEL_NAME
#define AIC_TOUCH_PANEL_NAME "touch"
#endif
#if defined(AIC_LVGL_TOUCH_DEVICE)
#define LV_AIC_TOUCH_NAME AIC_LVGL_TOUCH_DEVICE
#else
#define LV_AIC_TOUCH_NAME AIC_TOUCH_PANEL_NAME
#endif

static const char *lv_aic_touch_device_name(void)
{
    if ((LV_AIC_TOUCH_NAME != NULL) && (LV_AIC_TOUCH_NAME[0] != '\0')) {
        return LV_AIC_TOUCH_NAME;
    }
    return AIC_TOUCH_PANEL_NAME;
}

#define LV_AIC_TOUCH_STACK_SIZE 4096U
#define LV_AIC_TOUCH_PRIORITY 25U
#define LV_AIC_TOUCH_POLL_PERIOD_MS 10U

#if defined(RT_TOUCH_PIN_IRQ)
#define LV_AIC_TOUCH_USE_IRQ 1
#else
#define LV_AIC_TOUCH_USE_IRQ 0
#endif

typedef struct {
    lv_display_t *display;
    lv_indev_t *indev;
    rt_device_t device;
    aicos_sem_t irq_sem;
    aicos_sem_t exit_sem;
    aicos_sem_t delete_sem;
    aicos_thread_t worker;
    struct rt_touch_info info;
    struct rt_touch_data *read_data;
    volatile bool is_running;
    volatile bool worker_started;
    volatile rt_uint32_t callback_inflight;
    bool device_opened;
    bool callback_installed;
    rt_err_t (*previous_rx_indicate)(rt_device_t device, rt_size_t size);
    int16_t x;
    int16_t y;
    lv_indev_state_t state;
    aicos_mutex_t state_lock;
} lv_aic_touch_ctx_t;

static lv_aic_touch_ctx_t *lv_aic_touch_active_ctx;

static rt_err_t lv_aic_touch_irq_cb(rt_device_t device, rt_size_t size)
{
    lv_aic_touch_ctx_t *ctx;
    rt_base_t level;
    rt_err_t result = RT_EOK;

    level = rt_hw_interrupt_disable();
    ctx = lv_aic_touch_active_ctx;
    if ((ctx != NULL) && ctx->is_running) {
        ctx->callback_inflight++;
        rt_hw_interrupt_enable(level);

        if ((ctx->previous_rx_indicate != NULL) &&
            (ctx->previous_rx_indicate != lv_aic_touch_irq_cb)) {
            result = ctx->previous_rx_indicate(device, size);
        }
        if (ctx->irq_sem != NULL) {
            (void)aicos_sem_give(ctx->irq_sem);
        }

        level = rt_hw_interrupt_disable();
        if (ctx->callback_inflight > 0U) {
            ctx->callback_inflight--;
        }
        rt_hw_interrupt_enable(level);
        return result;
    }

    rt_hw_interrupt_enable(level);
    return result;
}

static int32_t lv_aic_scale_touch_coordinate(int32_t value, int32_t source_range,
                                              int32_t target_range)
{
    if ((source_range <= 1) || (target_range <= 1)) {
        return value;
    }
    return ((int64_t)value * (target_range - 1)) / (source_range - 1);
}

static void lv_aic_touch_transform(const lv_aic_touch_ctx_t *ctx, int16_t *x, int16_t *y)
{
    const int32_t physical_width = lv_display_get_original_horizontal_resolution(ctx->display);
    const int32_t physical_height = lv_display_get_original_vertical_resolution(ctx->display);
    int32_t transformed_x = lv_aic_scale_touch_coordinate(*x, ctx->info.range_x, physical_width);
    int32_t transformed_y = lv_aic_scale_touch_coordinate(*y, ctx->info.range_y, physical_height);

    /* LVGL 9.6 applies display rotation to indev points after this callback.
     * Keep the callback in native physical coordinates to avoid a double
     * rotation. */
    transformed_x = LV_CLAMP(0, transformed_x, physical_width - 1);
    transformed_y = LV_CLAMP(0, transformed_y, physical_height - 1);
    *x = (int16_t)transformed_x;
    *y = (int16_t)transformed_y;
}

static void lv_aic_touch_set_state(lv_aic_touch_ctx_t *ctx, int16_t x, int16_t y,
                                   lv_indev_state_t state)
{
    if (ctx == NULL) {
        return;
    }

    (void)aicos_mutex_take(ctx->state_lock, AICOS_WAIT_FOREVER);
    ctx->x = x;
    ctx->y = y;
    ctx->state = state;
    (void)aicos_mutex_give(ctx->state_lock);
}

static void lv_aic_touch_read_cb(lv_indev_t *indev, lv_indev_data_t *data)
{
    lv_aic_touch_ctx_t *ctx = (lv_aic_touch_ctx_t *)lv_indev_get_driver_data(indev);

    if ((ctx == NULL) || (data == NULL)) {
        return;
    }

    (void)aicos_mutex_take(ctx->state_lock, AICOS_WAIT_FOREVER);
    data->point.x = ctx->x;
    data->point.y = ctx->y;
    data->state = ctx->state;
    (void)aicos_mutex_give(ctx->state_lock);
}

static void lv_aic_touch_read_once(lv_aic_touch_ctx_t *ctx)
{
    rt_size_t count;
    bool have_pressed = false;
    bool have_event = false;
    int16_t event_x = 0;
    int16_t event_y = 0;

    if ((ctx == NULL) || (ctx->device == RT_NULL) || (ctx->read_data == NULL)) {
        return;
    }

    count = rt_device_read(ctx->device, 0, ctx->read_data, ctx->info.point_num);
    if (count > ctx->info.point_num) {
        /* A negative driver error is represented as a large unsigned value
         * by the legacy rt_device_read API. Never interpret that as a full
         * buffer; release a possibly stuck pointer instead. */
        LV_LOG_ERROR("touch read returned an invalid count: %u", (unsigned int)count);
        lv_aic_touch_set_state(ctx, ctx->x, ctx->y, LV_INDEV_STATE_RELEASED);
        return;
    }
    if (count == 0U) {
        return;
    }

    for (rt_size_t i = 0; i < count; ++i) {
        switch (ctx->read_data[i].event) {
        case RT_TOUCH_EVENT_DOWN:
        case RT_TOUCH_EVENT_MOVE:
            event_x = (int16_t)ctx->read_data[i].x_coordinate;
            event_y = (int16_t)ctx->read_data[i].y_coordinate;
            have_pressed = true;
            have_event = true;
            break;
        case RT_TOUCH_EVENT_UP:
            if (!have_event) {
                event_x = (int16_t)ctx->read_data[i].x_coordinate;
                event_y = (int16_t)ctx->read_data[i].y_coordinate;
            }
            have_event = true;
            break;
        default:
            break;
        }
    }

    if (have_event) {
        lv_indev_state_t state = have_pressed ? LV_INDEV_STATE_PRESSED : LV_INDEV_STATE_RELEASED;
        lv_aic_touch_transform(ctx, &event_x, &event_y);
        lv_aic_touch_set_state(ctx, event_x, event_y, state);
    }
}

static void lv_aic_touch_worker(void *parameter)
{
    lv_aic_touch_ctx_t *ctx = (lv_aic_touch_ctx_t *)parameter;

    while (ctx->is_running) {
        if (ctx->irq_sem != NULL) {
            (void)aicos_sem_take(ctx->irq_sem, AICOS_WAIT_FOREVER);
        } else {
            aicos_msleep(LV_AIC_TOUCH_POLL_PERIOD_MS);
        }

        if (!ctx->is_running) {
            break;
        }

        lv_aic_touch_read_once(ctx);
        if (ctx->is_running && (ctx->irq_sem != NULL)) {
            rt_device_control(ctx->device, RT_TOUCH_CTRL_ENABLE_INT, RT_NULL);
        }
    }

    (void)aicos_sem_give(ctx->exit_sem);
    /* Keep the thread parked until the owner has detached it. This avoids
     * deleting a thread while its entry function is still returning. */
    (void)aicos_sem_take(ctx->delete_sem, AICOS_WAIT_FOREVER);
}

static void lv_aic_touch_cleanup(lv_aic_touch_ctx_t *ctx)
{
    if (ctx == NULL) {
        return;
    }

    if (ctx->device != RT_NULL && ctx->device_opened && (ctx->irq_sem != NULL)) {
        rt_device_control(ctx->device, RT_TOUCH_CTRL_DISABLE_INT, RT_NULL);
    }

    {
        rt_base_t level = rt_hw_interrupt_disable();
        ctx->is_running = false;
        if (ctx->callback_installed && (ctx->device != RT_NULL)) {
            ctx->device->rx_indicate = ctx->previous_rx_indicate;
            ctx->callback_installed = false;
        }
        if (lv_aic_touch_active_ctx == ctx) {
            lv_aic_touch_active_ctx = NULL;
        }
        rt_hw_interrupt_enable(level);
    }

    while (ctx->callback_inflight != 0U) {
        rt_thread_yield();
    }

    if (ctx->irq_sem != NULL) {
        (void)aicos_sem_give(ctx->irq_sem);
    }

    if (ctx->worker_started) {
        (void)aicos_sem_take(ctx->exit_sem, AICOS_WAIT_FOREVER);
        aicos_thread_delete(ctx->worker);
        ctx->worker = NULL;
        ctx->worker_started = false;
    }

    if (ctx->device != RT_NULL && ctx->device_opened) {
        rt_device_close(ctx->device);
        ctx->device_opened = false;
    }
    if (ctx->device != RT_NULL) {
        ctx->device = RT_NULL;
    }
    if (ctx->irq_sem != NULL) {
        aicos_sem_delete(ctx->irq_sem);
        ctx->irq_sem = NULL;
    }
    if (ctx->exit_sem != NULL) {
        aicos_sem_delete(ctx->exit_sem);
        ctx->exit_sem = NULL;
    }
    if (ctx->delete_sem != NULL) {
        aicos_sem_delete(ctx->delete_sem);
        ctx->delete_sem = NULL;
    }
    if (ctx->read_data != NULL) {
        lv_free(ctx->read_data);
        ctx->read_data = NULL;
    }

    if (ctx->indev != NULL) {
        lv_indev_set_driver_data(ctx->indev, NULL);
        lv_indev_set_read_cb(ctx->indev, NULL);
        lv_indev_delete(ctx->indev);
        ctx->indev = NULL;
    }

    if (ctx->state_lock != NULL) {
        aicos_mutex_delete(ctx->state_lock);
        ctx->state_lock = NULL;
    }
    if (lv_aic_touch_active_ctx == ctx) {
        lv_aic_touch_active_ctx = NULL;
    }
    lv_free(ctx);
}

int lv_aic_indev_init(lv_display_t *display, lv_indev_t **indev)
{
    lv_aic_touch_ctx_t *ctx;
    const char *device_name;
    rt_uint16_t open_flags;

    if ((display == NULL) || (indev == NULL)) {
        return LV_AIC_ERR_INVALID_STATE;
    }
    *indev = NULL;

    ctx = (lv_aic_touch_ctx_t *)lv_malloc_zeroed(sizeof(*ctx));
    if (ctx == NULL) {
        return LV_AIC_ERR_NO_MEMORY;
    }
    ctx->display = display;
    ctx->state = LV_INDEV_STATE_RELEASED;

    ctx->state_lock = aicos_mutex_create();
    if (ctx->state_lock == NULL) {
        lv_free(ctx);
        return LV_AIC_ERR_INPUT;
    }

    device_name = lv_aic_touch_device_name();
    ctx->device = rt_device_find(device_name);
    if (ctx->device == RT_NULL) {
        LV_LOG_ERROR("touch device not found: %s", device_name);
        goto fail;
    }

    open_flags = RT_DEVICE_FLAG_RDWR;
#if LV_AIC_TOUCH_USE_IRQ
    open_flags |= RT_DEVICE_FLAG_INT_RX;
#endif
    if (rt_device_open(ctx->device, open_flags) != RT_EOK) {
        LV_LOG_ERROR("failed to open touch device: %s", device_name);
        goto fail;
    }
    ctx->device_opened = true;

    if (rt_device_control(ctx->device, RT_TOUCH_CTRL_GET_INFO, &ctx->info) != RT_EOK ||
        (ctx->info.point_num == 0U)) {
        LV_LOG_ERROR("failed to query touch device information");
        goto fail;
    }

    ctx->read_data = (struct rt_touch_data *)lv_malloc_zeroed(
        sizeof(struct rt_touch_data) * ctx->info.point_num);
    if (ctx->read_data == NULL) {
        goto fail;
    }

    ctx->irq_sem = NULL;
#if LV_AIC_TOUCH_USE_IRQ
    ctx->irq_sem = aicos_sem_create(0U);
#endif
    ctx->exit_sem = aicos_sem_create(0U);
    ctx->delete_sem = aicos_sem_create(0U);
#if LV_AIC_TOUCH_USE_IRQ
    if ((ctx->irq_sem == NULL) || (ctx->exit_sem == NULL) || (ctx->delete_sem == NULL)) {
#else
    if ((ctx->exit_sem == NULL) || (ctx->delete_sem == NULL)) {
#endif
        goto fail;
    }

    ctx->indev = lv_indev_create();
    if (ctx->indev == NULL) {
        goto fail;
    }

    lv_indev_set_type(ctx->indev, LV_INDEV_TYPE_POINTER);
    lv_indev_set_read_cb(ctx->indev, lv_aic_touch_read_cb);
    lv_indev_set_driver_data(ctx->indev, ctx);
    lv_indev_set_display(ctx->indev, display);

    lv_aic_touch_active_ctx = ctx;
    ctx->is_running = true;
    ctx->previous_rx_indicate = ctx->device->rx_indicate;
    if (ctx->previous_rx_indicate == lv_aic_touch_irq_cb) {
        LV_LOG_ERROR("touch receive callback is already owned by lvgl-aic");
        goto fail;
    }
#if LV_AIC_TOUCH_USE_IRQ
    {
        rt_base_t level = rt_hw_interrupt_disable();
        ctx->device->rx_indicate = lv_aic_touch_irq_cb;
        ctx->callback_installed = true;
        rt_hw_interrupt_enable(level);
    }
#endif

    /* Prime the driver once before enabling interrupt delivery. This avoids
     * losing a touch that arrives between device open and worker startup. */
    lv_aic_touch_read_once(ctx);
#if LV_AIC_TOUCH_USE_IRQ
    if (ctx->is_running) {
        rt_device_control(ctx->device, RT_TOUCH_CTRL_ENABLE_INT, RT_NULL);
    }
#endif

    ctx->worker = aicos_thread_create("lv_aic_touch", LV_AIC_TOUCH_STACK_SIZE,
                                      LV_AIC_TOUCH_PRIORITY, lv_aic_touch_worker, ctx);
    if (ctx->worker == NULL) {
        ctx->is_running = false;
        goto fail;
    }
    ctx->worker_started = true;

    *indev = ctx->indev;
    return LV_AIC_OK;

fail:
    lv_aic_touch_cleanup(ctx);
    return LV_AIC_ERR_INPUT;
}

void lv_aic_indev_deinit(lv_indev_t *indev)
{
    if (indev == NULL) {
        return;
    }
    lv_aic_touch_cleanup((lv_aic_touch_ctx_t *)lv_indev_get_driver_data(indev));
}

#else /* AIC_LVGL_USE_TOUCH && AIC_LVGL_BSP_RTTHREAD */

int lv_aic_indev_init(lv_display_t *display, lv_indev_t **indev)
{
    (void)display;
    if (indev != NULL) {
        *indev = NULL;
    }
    return LV_AIC_ERR_NO_BSP;
}

void lv_aic_indev_deinit(lv_indev_t *indev)
{
    (void)indev;
}

#endif /* AIC_LVGL_USE_TOUCH && AIC_LVGL_BSP_RTTHREAD */
