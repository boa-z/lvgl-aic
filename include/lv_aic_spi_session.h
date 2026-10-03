/* SPDX-License-Identifier: Apache-2.0 */
#ifndef LV_AIC_SPI_SESSION_H
#define LV_AIC_SPI_SESSION_H
#include "lv_aic_spi_transfer.h"
struct rt_qspi_device;
#ifdef __cplusplus
extern "C" {
#endif
typedef struct lv_aic_spi_session lv_aic_spi_session_t;
typedef struct {
    struct rt_qspi_device *device;
    uint8_t *tx;
    size_t capacity;
    uint32_t width,height,prefix;
    unsigned prefix_bytes,prefix_lines,data_lines;
    bool swap_bytes;
} lv_aic_spi_session_config_t;
/* Bind an already initialized panel/device. Requires SPI_SDK. Claims its bus
 * among component sessions and the cache-rounded tx region. Unmanaged users
 * must be excluded by the application; this does not lock out SDK drivers.
 * tx must be dedicated 64-byte-aligned DMA storage with capacity rounded up
 * to 64 bytes. Device, bus, tx and panel configuration remain stable until
 * successful close. No pixel allocation, bus configuration or panel command.
 * Each session is single-worker-owned; registry contention makes open fail
 * or close return BUSY (retry). Faulted sessions retain their claims forever. */
lv_aic_spi_session_t *lv_aic_spi_session_open(const lv_aic_spi_session_config_t *config);
/* Allocate dedicated 64-byte-aligned CMA tx storage. config.tx must be NULL
 * and config.capacity zero; budget bounds the rounded pixel allocation only
 * (metadata excluded). Failed open frees allocation; successful close drains
 * before freeing. FAULT retains both allocation and bus claim until reboot. */
lv_aic_spi_session_t *lv_aic_spi_session_open_owned(const lv_aic_spi_session_config_t *config,
    size_t pixel_budget);
lv_aic_spi_result_t lv_aic_spi_session_submit(lv_aic_spi_session_t *session,
    const lv_aic_spi_rgb565_frame_t *source,unsigned clockwise_degrees);
lv_aic_spi_result_t lv_aic_spi_session_drain(lv_aic_spi_session_t *session);
lv_aic_spi_result_t lv_aic_spi_session_close(lv_aic_spi_session_t *session);
#ifdef __cplusplus
}
#endif
#endif
