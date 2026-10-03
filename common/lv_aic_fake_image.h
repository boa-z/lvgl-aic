/* SPDX-License-Identifier: Apache-2.0 */
#ifndef LV_AIC_FAKE_IMAGE_H
#define LV_AIC_FAKE_IMAGE_H
#include <stdbool.h>
#include <stdint.h>

typedef struct {
    uint16_t width, height;
    bool blend;
    uint32_t argb;
} lv_aic_fake_image_t;

/* SDK syntax: L:/<width>x<height>_<0|1>_<8 hex digits>.fake.
 * Any ASCII drive letter is accepted. Dimensions are 1..4096 and the
 * resource area is at most 8M pixels, matching the resource decoder budget.
 * No file is opened. On failure, *image remains unchanged. */
bool lv_aic_fake_image_parse(const char *path, lv_aic_fake_image_t *image);
#endif
