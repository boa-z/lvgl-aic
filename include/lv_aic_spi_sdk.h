/* SPDX-License-Identifier: Apache-2.0 */
#ifndef LV_AIC_SPI_SDK_H
#define LV_AIC_SPI_SDK_H
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif
/* Checked wait callback for lv_aic_spi_transfer_ops_t. context is the borrowed
 * underlying struct rt_spi_device* (QSPI uses &qspi->parent), NOT aic_spi_lcd_dev*.
 * Requires AIC_LVGL_USE_SPI_SDK. Call only after accepted async submission, on
 * its exclusive owner worker. Blocks for the SDK driver's completion timeout.
 * Missing completion/status ops, wait error, in-progress or error status fail.
 * It does not claim the device, stop DMA, initialize a panel or free resources.
 * A false result must retain any possibly DMA-referenced storage. */
bool lv_aic_spi_sdk_wait_complete(void *context);
struct rt_qspi_device;
/* Checked SDK command-prefix + pixel submission. Borrowed rt_qspi_device must
 * have exclusive bus ownership for the entire async lifetime. Explicit prefix
 * is 0..4 bytes, MSB first, on 1/2/4 lines (0 lines only for no prefix); data
 * lines must be 1/2/4. This matches the SDK panel's address-prefix framing.
 * Caller configures panel/D-C, cleans cache and owns DMA-reachable pixels until
 * wait_complete succeeds. No cache range, pin or command is guessed here.
 * Returns true only for an exact accepted byte count. false may follow partial
 * DMA submission: retain storage and fault the owning transfer session.
 * No previous transfer may remain pending; use the transfer core to drain it. */
bool lv_aic_spi_sdk_submit_qspi(struct rt_qspi_device *device,const uint8_t *pixels,
    size_t bytes,uint32_t prefix,unsigned prefix_bytes,unsigned prefix_lines,unsigned data_lines);
#ifdef __cplusplus
}
#endif
#endif
