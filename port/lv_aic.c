/**
 * @file lv_aic.c
 * @brief Public lifecycle glue for the ArtInChip LVGL platform port.
 */

#include "lvgl_aic.h"
#include "lv_aic_display.h"
#include "lv_aic_indev.h"
#if AIC_LVGL_USE_MPP_DEC
#include "lv_aic_mpp_decoder.h"
#endif
#if AIC_LVGL_USE_GE2D
#include "lv_draw_aic_ge2d.h"
#endif

#include <stdbool.h>
#include <stdint.h>

#if AIC_LVGL_BSP_RTTHREAD
#include <rtthread.h>

static uint32_t lv_aic_tick_get_ms(void)
{
    return (uint32_t)rt_tick_get_millisecond();
}
#endif

static lv_display_t *lv_aic_display;
static lv_indev_t *lv_aic_pointer_indev;
#if AIC_LVGL_USE_MPP_DEC
static lv_image_decoder_t *lv_aic_mpp_decoder;
#endif
static bool lv_aic_initialized;

int lv_aic_init(void)
{
    int result;

    if (lv_aic_initialized) {
        return LV_AIC_ERR_INVALID_STATE;
    }

    lv_aic_display = NULL;
    lv_aic_pointer_indev = NULL;
#if AIC_LVGL_USE_MPP_DEC
    lv_aic_mpp_decoder = NULL;
#endif

#if AIC_LVGL_BSP_RTTHREAD
    lv_tick_set_cb(lv_aic_tick_get_ms);
#endif

    result = lv_aic_display_init(&lv_aic_display);
    if (result != LV_AIC_OK) {
        return result;
    }

#if AIC_LVGL_USE_TOUCH
    result = lv_aic_indev_init(lv_aic_display, &lv_aic_pointer_indev);
    if (result != LV_AIC_OK) {
        lv_aic_display_deinit(lv_aic_display);
        lv_aic_display = NULL;
        return result;
    }
#else
    (void)result;
#endif

#if AIC_LVGL_USE_MPP_DEC
    result = lv_aic_mpp_decoder_init(&lv_aic_mpp_decoder);
    if (result != LV_AIC_OK) {
        if (lv_aic_pointer_indev != NULL) {
            lv_aic_indev_deinit(lv_aic_pointer_indev);
            lv_aic_pointer_indev = NULL;
        }
        lv_aic_display_deinit(lv_aic_display);
        lv_aic_display = NULL;
        return result;
    }
#endif

#if AIC_LVGL_USE_GE2D
    /* Registered last, so a failure in any earlier step never leaves the GE2D
     * device open. init() cannot fail: if mpp_ge_open() returns NULL the unit
     * is still created and declines every task, keeping software rendering. */
    lv_draw_aic_ge2d_init();
#endif

    lv_aic_initialized = true;
    return LV_AIC_OK;
}

void lv_aic_deinit(void)
{
    if (!lv_aic_initialized) {
        return;
    }

    if (lv_aic_pointer_indev != NULL) {
        lv_aic_indev_deinit(lv_aic_pointer_indev);
        lv_aic_pointer_indev = NULL;
    }
#if AIC_LVGL_USE_MPP_DEC
    if (lv_aic_mpp_decoder != NULL) {
        lv_aic_mpp_decoder_deinit(lv_aic_mpp_decoder);
        lv_aic_mpp_decoder = NULL;
    }
#endif
#if AIC_LVGL_USE_GE2D
    /* Close the GE2D device before the display goes away: the unit must not be
     * able to touch a freed draw buffer. */
    lv_draw_aic_ge2d_deinit();
#endif
    if (lv_aic_display != NULL) {
        lv_aic_display_deinit(lv_aic_display);
        lv_aic_display = NULL;
    }
    lv_aic_initialized = false;
}

lv_display_t *lv_aic_get_display(void)
{
    return lv_aic_display;
}

lv_indev_t *lv_aic_get_pointer_indev(void)
{
    return lv_aic_pointer_indev;
}

#if AIC_LVGL_USE_MPP_DEC
lv_image_decoder_t *lv_aic_get_mpp_decoder(void)
{
    return lv_aic_mpp_decoder;
}
#endif
