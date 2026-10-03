/* SPDX-License-Identifier: Apache-2.0 */
#ifndef LV_AIC_BARCODE_H
#define LV_AIC_BARCODE_H
#include "lv_aic_yuv.h"
#ifdef __cplusplus
extern "C" {
#endif
typedef enum {
    LV_AIC_BARCODE_OK, LV_AIC_BARCODE_EMPTY, LV_AIC_BARCODE_BUSY,
    LV_AIC_BARCODE_INVALID, LV_AIC_BARCODE_NOMEM, LV_AIC_BARCODE_INIT_FAILED,
    LV_AIC_BARCODE_TOO_LONG
} lv_aic_barcode_result_t;
/* Synchronous worker API; never call from the LVGL refresh/UI thread. Input
 * is borrowed CPU-coherent I400/NV12/NV16, unchanged for the entire call.
 * Gray staging <=2 MiB; SDK-internal allocations are opaque and not included.
 * The global SDK decoder is serialized without blocking contenders. Do not
 * run any other SDK barcode user concurrently. Initialization persists: the
 * vendor ABI has no shutdown API. Result <=4096 bytes, binary (no terminator).
 * Output must not overlap input planes; length must be separate caller storage.
 * On non-OK, output remains unchanged and *length is zero. */
lv_aic_barcode_result_t lv_aic_barcode_decode(const lv_aic_yuv_frame_t *frame,
    uint8_t *output,size_t capacity,size_t *length);
#ifdef __cplusplus
}
#endif
#endif
