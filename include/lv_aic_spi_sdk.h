/* SPDX-License-Identifier: Apache-2.0 */
#ifndef LV_AIC_SPI_SDK_H
#define LV_AIC_SPI_SDK_H
#include <stdbool.h>
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
#ifdef __cplusplus
}
#endif
#endif
