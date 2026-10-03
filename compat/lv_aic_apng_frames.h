/* SPDX-License-Identifier: Apache-2.0 */
#ifndef LV_AIC_APNG_FRAMES_H
#define LV_AIC_APNG_FRAMES_H
#include "lv_aic_rgb_image.h"
typedef struct lv_aic_apng_frames lv_aic_apng_frames_t;
/* Fixed snapshot pool, 2..8 frames, <=8M pixels. Budget includes pool metadata
 * and native pixel storage, excluding allocator overhead/LVGL image caches.
 * Create before sharing. One serialized producer and one LVGL owner consumer. */
lv_aic_apng_frames_t *lv_aic_apng_frames_create(uint32_t width,uint32_t height,
    unsigned count,size_t budget);
/* Worker: copy RGBA into immutable native ARGB storage. Latest queued frame
 * replaces older unconsumed frames; retained readers are never overwritten.
 * False means closed, invalid input or reader backpressure. Continue composing
 * even when publication is dropped. Sequence must increase after success. */
bool lv_aic_apng_frames_publish(lv_aic_apng_frames_t *p,const void *rgba,
    size_t stride,size_t capacity,uint64_t sequence);
/* LVGL owner only; initialize RGB decoder first. Caller owns returned image.
 * Retire it only after detaching widgets/queued draws; GE/native readers can
 * keep the snapshot alive beyond retirement. Outputs unchanged on false. */
bool lv_aic_apng_frames_poll(lv_aic_apng_frames_t *p,lv_aic_rgb_image_t **image,uint64_t *sequence);
/* Close publication, discard unconsumed frames. Destroy only after producer
 * and poll calls stop; false while readers/active calls still own a slot. */
void lv_aic_apng_frames_close(lv_aic_apng_frames_t *p);
/* Discard only unconsumed publication, e.g. at a worker replay boundary.
 * Producer must be idle; retained images remain valid. Does not reopen close. */
void lv_aic_apng_frames_discard(lv_aic_apng_frames_t *p);
bool lv_aic_apng_frames_destroy(lv_aic_apng_frames_t *p);
#endif
