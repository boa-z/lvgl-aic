/* SPDX-License-Identifier: Apache-2.0 */
#ifndef LV_AIC_SPI_FRAME_H
#define LV_AIC_SPI_FRAME_H
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif
typedef struct {
    const uint8_t *data;
    size_t capacity, stride;
    uint32_t width, height;
} lv_aic_spi_rgb565_frame_t;
/* CPU frame preparation, no allocation or device access. Source is little-endian
 * RGB565 with optional row padding, immutable and CPU coherent. Output is packed
 * RGB565 with explicit byte swap, clockwise rotation (0/90/180/270), then nearest
 * neighbour resize. Both dimensions 1..4096; source/output spans must not overlap.
 * Only width*height*2 output bytes are written; extra capacity remains unchanged.
 * Failure leaves output unchanged. Caller owns DMA/cache/lifetime and must not
 * write output while a previous SPI transfer still reads it. No panel/bus defaults.
 * Run large frame conversion outside the LVGL refresh thread. */
bool lv_aic_spi_pack_rgb565(const lv_aic_spi_rgb565_frame_t *source,
    uint8_t *output,size_t capacity,uint32_t width,uint32_t height,
    unsigned clockwise_degrees,bool swap_bytes);
#ifdef __cplusplus
}
#endif
#endif
