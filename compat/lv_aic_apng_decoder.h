/* SPDX-License-Identifier: Apache-2.0 */
#ifndef LV_AIC_APNG_DECODER_H
#define LV_AIC_APNG_DECODER_H
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
typedef struct lv_aic_apng_decoder lv_aic_apng_decoder_t;
/* Serialized worker-only API. No LVGL objects. Frame budget bounds live CMA
 * output allocations, not SDK packet/VE scratch or caller-owned pixels.
 * Packet limit is the aligned SDK bitstream capacity, <= INT_MAX-255. */
lv_aic_apng_decoder_t *lv_aic_apng_decoder_create(size_t frame_budget,size_t packet_limit);
/* Decode an extracted standalone PNG to straight-alpha RGBA8 bytes. Dimensions
 * must match IHDR. Stride/padding belong to caller. Input/output must not alias.
 * Only true makes output publishable; false may have written pixels if frame
 * return failed. Call close after failure before attempting another decode.
 * Source must be trusted: structural/CRC checks do not sandbox the SDK codec. */
bool lv_aic_apng_decoder_decode(lv_aic_apng_decoder_t *d,const void *png,size_t bytes,
    uint32_t width,uint32_t height,void *rgba,size_t stride,size_t capacity);
/* Return outstanding frame, then detach decoder. A failed return retains all
 * ownership for a later retry. Never free/kill the worker on false. */
bool lv_aic_apng_decoder_close(lv_aic_apng_decoder_t *d);
bool lv_aic_apng_decoder_destroy(lv_aic_apng_decoder_t *d);
#endif
