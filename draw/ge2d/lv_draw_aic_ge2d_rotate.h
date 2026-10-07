/* SPDX-License-Identifier: Apache-2.0 */
#ifndef LV_DRAW_AIC_GE2D_ROTATE_H
#define LV_DRAW_AIC_GE2D_ROTATE_H

#include "lvgl_aic.h"

/* ROT1 center registers preserve only 14 bits. Use a conservative signed
 * 14-bit domain for both source pivot and crop-relative destination center.
 * Compute the translation in 64 bits before checking/narrowing. */
bool lv_aic_ge2d_rotation_center(const lv_point_t *pivot,
                                  const lv_area_t *image_coords,
                                  const lv_area_t *destination_clip,
                                  lv_point_t *destination_center);

/* Map an inclusive destination rectangle, expressed relative to the
 * untransformed image origin, back to the source rectangle consumed by a GE
 * rotated bitblt. The source crop and destination crop therefore stay paired
 * even when LVGL has clipped the transformed image. */
bool lv_aic_ge2d_rotation_crop(uint32_t src_w, uint32_t src_h,
                               const lv_area_t *dst_local,
                               const lv_point_t *pivot, int32_t rotation,
                               lv_area_t *src_crop, unsigned *flags);

/* Combined transform contract used by 3C4. Coordinates are the visible
 * destination rectangle relative to LVGL's transformed image origin. */
bool lv_aic_ge2d_rotation_scale_crop(uint32_t src_w, uint32_t src_h,
                                     const lv_area_t *dst_local,
                                     const lv_point_t *pivot, int32_t rotation,
                                     uint32_t scale_x, uint32_t scale_y,
                                     lv_area_t *src_crop, unsigned *flags,
                                     int32_t *phase_x_16, int32_t *phase_y_16);

#endif
