/* SPDX-License-Identifier: Apache-2.0 */
#ifndef LV_AIC_LOTTIE_H
#define LV_AIC_LOTTIE_H
#include "lvgl.h"
#ifdef __cplusplus
extern "C" {
#endif
#if LV_USE_LOTTIE
/* Limits cover encoded input and staging pixel bytes, not ThorVG's object heap. */
typedef struct {
    size_t source_bytes;
    size_t staging_bytes;
} lv_aic_lottie_limits_t;
typedef enum {
    LV_AIC_LOTTIE_OK=0,
    LV_AIC_LOTTIE_INVALID=-1,
    LV_AIC_LOTTIE_LIMIT=-2,
    LV_AIC_LOTTIE_IO=-3,
    LV_AIC_LOTTIE_NO_MEMORY=-4,
    LV_AIC_LOTTIE_DECODE=-5,
    LV_AIC_LOTTIE_RENDER=-6,
    LV_AIC_LOTTIE_BUSY=-7
} lv_aic_lottie_result_t;
/* LVGL owner only, outside drawing; native widget needs a valid canvas buffer.
 * Failures preserve the old animation, timing, pause state and pixel storage.
 * Success copies frame zero into the caller-owned canvas, resets source timing,
 * and preserves pause intent. Input bytes are copied; exclude the trailing NUL.
 * Renderer-internal allocation assertions are governed by upstream LVGL policy. */
lv_aic_lottie_result_t lv_aic_lottie_load_data(lv_obj_t *obj,const void *data,size_t size,
                                             const lv_aic_lottie_limits_t *limits);
/* Uses an application-registered LVGL filesystem drive. Requires seek/tell.
 * File bytes are staged within source_bytes; short reads and close errors fail
 * before replacing the widget. This differs from native ThorVG C file paths. */
lv_aic_lottie_result_t lv_aic_lottie_load_file(lv_obj_t *obj,const char *path,
                                             const lv_aic_lottie_limits_t *limits);
#endif
#ifdef __cplusplus
}
#endif
#endif
