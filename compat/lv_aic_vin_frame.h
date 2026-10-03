/* SPDX-License-Identifier: Apache-2.0 */
#ifndef LV_AIC_VIN_FRAME_H
#define LV_AIC_VIN_FRAME_H
#include "lv_aic_vin_session.h"
#include "lv_aic_yuv.h"
/* Borrow an already dequeued frame; never dequeue, retain, return, invalidate
 * caches or infer a colorspace here. Caller keeps the index held until every
 * consumer finishes. Output is unchanged on failure. */
bool lv_aic_vin_frame_view(const lv_aic_vin_session_t *session, uint32_t index,
                           lv_aic_yuv_color_space_t color_space, lv_aic_yuv_frame_t *frame);
#endif
