/* SPDX-License-Identifier: Apache-2.0 */
#ifndef LV_AIC_BMP_HEADER_H
#define LV_AIC_BMP_HEADER_H
#include <stdbool.h>
#include <stdint.h>
typedef struct {
    uint32_t width, height, offset, stride;
    uint16_t bpp;
    bool top_down;
} lv_aic_bmp_header_t;
/* Uncompressed 24/32-bit Windows BMP. 16-bit requires explicit masks and
 * must not be silently interpreted as RGB565 (BI_RGB defaults to RGB555). */
bool lv_aic_bmp_parse_header(const uint8_t *data, uint32_t available,
                             uint32_t file_size, lv_aic_bmp_header_t *out);
#endif
