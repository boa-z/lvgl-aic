/* SPDX-License-Identifier: Apache-2.0 */
#ifndef LV_AIC_APNG_COMPOSE_H
#define LV_AIC_APNG_COMPOSE_H
#include "lv_aic_apng.h"
/* Straight-alpha RGBA8 reference canvas, no SDK/LVGL allocation or draw API.
 * Caller owns distinct canvas/scratch/source storage, all CPU coherent.
 * Scratch holds a tightly packed canvas-sized rectangle backup for PREVIOUS.
 * Metadata is private after init; do not copy an active canvas by value. */
typedef struct {
    uint8_t *pixels,*scratch;
    size_t stride,capacity,scratch_capacity;
    uint32_t width,height;
    lv_aic_apng_frame_t previous;
    bool have_previous;
} lv_aic_apng_canvas_t;
bool lv_aic_apng_canvas_init(lv_aic_apng_canvas_t *canvas,uint32_t width,uint32_t height,
    void *pixels,size_t stride,size_t capacity,void *scratch,size_t scratch_capacity);
/* Clears transparent pixels and disposal history; padding remains untouched. */
void lv_aic_apng_canvas_reset(lv_aic_apng_canvas_t *canvas);
/* Applies preceding disposal, saves PREVIOUS as necessary, then SOURCE/OVER.
 * First-frame PREVIOUS restores transparency on the next draw, as required.
 * Input is decoded frame-rectangle RGBA, not already composed canvas pixels.
 * Failing validation leaves canvas/history unchanged. Current frame remains
 * visible until next compose/reset; the final disposal is never applied early. */
bool lv_aic_apng_compose(lv_aic_apng_canvas_t *canvas,const lv_aic_apng_frame_t *frame,
    const void *rgba,size_t stride,size_t capacity);
#endif
