/* SPDX-License-Identifier: Apache-2.0 */
#ifndef LV_AIC_APNG_H
#define LV_AIC_APNG_H
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
/* Pure immutable-memory PNG/APNG container interface. No SDK/LVGL allocation.
 * Caller owns bytes for document lifetime; do not mutate document/frame fields.
 * Explicit limits apply before decoding. Zlib/pixel validation is decoder work. */
typedef struct {
    size_t file_bytes,frame_png_bytes;
    uint32_t canvas_pixels,frames;
} lv_aic_apng_limits_t;
typedef struct {
    const uint8_t *data;
    size_t size,prefix_end,prefix_bytes,max_frame_png_bytes;
    uint32_t width,height,frames,plays;
    bool animated;
} lv_aic_apng_t;
typedef struct {
    uint32_t width,height,x,y;
    uint16_t delay_num,delay_den; /* Denominator zero is normalized to 100. */
    uint8_t dispose,blend; /* PNG dispose NONE/BACKGROUND/PREVIOUS, SOURCE/OVER. */
    size_t data_start,data_end,png_bytes; /* Internal validated chunk span. */
} lv_aic_apng_frame_t;
/* Transactional outputs on failure; normal PNG is a single static frame.
 * Reject trailing bytes, invalid CRC/order/sequence, unknown critical chunks,
 * out-of-canvas rectangles and files/frames exceeding the supplied limits.
 * ArtInChip private dcTL is validated as ancillary data then ignored. */
bool lv_aic_apng_open(const void *data,size_t size,const lv_aic_apng_limits_t *limits,lv_aic_apng_t *out);
/* Set cursor=0 initially; each success advances to the next frame, with size
 * marking EOF. A false result leaves frame/cursor untouched, including EOF.
 * A default PNG poster without fcTL is excluded from the animation. */
bool lv_aic_apng_next(const lv_aic_apng_t *doc,size_t *cursor,lv_aic_apng_frame_t *frame);
/* Materialize one standalone PNG into caller storage, preserving color chunks,
 * resizing IHDR and converting fdAT payload to IDAT with regenerated CRC.
 * No source/output aliasing. Buffer must hold frame.png_bytes; no partial write
 * on invalid capacity/span. Extracted dimensions are the frame rectangle. */
bool lv_aic_apng_extract(const lv_aic_apng_t *doc,const lv_aic_apng_frame_t *frame,void *output,size_t capacity);
#endif
