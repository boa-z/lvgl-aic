/**
 * @file lvgl_aic.h
 * @brief Public API for the ArtInChip LVGL platform port.
 */

#ifndef LVGL_AIC_H
#define LVGL_AIC_H

#if defined(LV_LVGL_H_INCLUDE_SIMPLE)
#include "lvgl.h"
#else
#include <lvgl/lvgl.h>
#endif

#if (LVGL_VERSION_MAJOR != 9) || (LVGL_VERSION_MINOR != 6)
#error "lvgl-aic currently supports LVGL 9.6.x only"
#endif

#ifdef __cplusplus
extern "C" {
#endif

/** Return codes used by the platform integration API. */
typedef enum {
    LV_AIC_OK = 0,
    LV_AIC_ERR_INVALID_STATE = -1,
    LV_AIC_ERR_NO_BSP = -2,
    LV_AIC_ERR_DISPLAY = -3,
    LV_AIC_ERR_INPUT = -4,
    LV_AIC_ERR_NO_MEMORY = -5,
    LV_AIC_ERR_UNSUPPORTED = -6,
} lv_aic_result_t;

/**
 * @brief Initialize ArtInChip display and input integration.
 *
 * The caller must have called lv_init() and must own the LVGL task loop.
 * The function does not create product UI objects or start protocol threads.
 *
 * @return LV_AIC_OK on success, otherwise a negative lv_aic_result_t.
 */
int lv_aic_init(void);

/**
 * @brief Deinitialize the ArtInChip integration.
 *
 * Call this only after the LVGL task/flush callbacks have been stopped.
 */
void lv_aic_deinit(void);

/** @brief Return the LVGL display created by the port, or NULL. */
lv_display_t *lv_aic_get_display(void);

/** @brief Return the LVGL pointer input device, or NULL. */
lv_indev_t *lv_aic_get_pointer_indev(void);

#ifdef __cplusplus
}
#endif

#endif /* LVGL_AIC_H */
