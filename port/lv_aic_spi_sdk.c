/* SPDX-License-Identifier: Apache-2.0 */
#include "lvgl_aic_feature_config.h"
#if defined(AIC_LVGL_USE_SPI_SDK) && AIC_LVGL_USE_SPI_SDK
#include "lv_aic_spi_sdk.h"
#include <rtdevice.h>
#include <hal_qspi.h>
bool lv_aic_spi_sdk_wait_complete(void *context)
{
    struct rt_spi_device *device=context;
    /* rt_spi_wait_completion dereferences wait_completion unconditionally.
     * rt_spi_get_transfer_status silently returns OK when gstatus is absent. */
    if(!device || !device->bus || !device->bus->ops ||
       !device->bus->ops->wait_completion || !device->bus->ops->gstatus) return false;
    if(rt_spi_wait_completion(device)!=RT_EOK) return false;
    rt_uint32_t status=rt_spi_get_transfer_status(device);
    /* D13x normal completion reports OK; TRAN_DONE is also an explicit
     * terminal indication. Reject unknown bits rather than masking errors. */
    return status==HAL_QSPI_STATUS_OK || status==HAL_QSPI_STATUS_TRAN_DONE;
}
static bool wire_lines(unsigned lines) { return lines==1 || lines==2 || lines==4; }
bool lv_aic_spi_sdk_submit_qspi(struct rt_qspi_device *device,const uint8_t *pixels,
    size_t bytes,uint32_t prefix,unsigned prefix_bytes,unsigned prefix_lines,unsigned data_lines)
{
    if(!device || !pixels || !bytes || bytes>INT32_MAX || prefix_bytes>4 ||
       !wire_lines(data_lines) || (prefix_bytes ? !wire_lines(prefix_lines) : prefix_lines!=0)) return false;
    uintptr_t address=(uintptr_t)pixels;
    if(address>UINT32_MAX || bytes>(uint64_t)UINT32_MAX+1-address) return false;
#if defined(AIC_CHIP_D13X) || defined(AIC_CHIP_G73X)
    if(address<0x40000000U) return false;
#endif
    struct rt_spi_device *spi=&device->parent;
    if(!spi->bus || !spi->bus->ops || !spi->bus->ops->configure ||
       !spi->bus->ops->xfer || !spi->bus->ops->nonblock ||
       !spi->bus->ops->wait_completion || !spi->bus->ops->gstatus) return false;
    rt_uint32_t status=rt_spi_get_transfer_status(spi);
    if(status!=HAL_QSPI_STATUS_OK && status!=HAL_QSPI_STATUS_TRAN_DONE) return false;
    if(rt_spi_nonblock_set(spi,1)!=RT_EOK) return false;
    /* Zero the extended message: D13x xfer reads QSPI fields even on one lane. */
    struct rt_qspi_message message={0};
    message.parent.send_buf=pixels;message.parent.length=bytes;
    message.parent.cs_take=1;message.parent.cs_release=1;
    message.address.content=prefix;message.address.size=prefix_bytes;
    message.address.qspi_lines=prefix_lines;message.qspi_data_lines=data_lines;
    return rt_qspi_transfer_message(device,&message)==bytes;
}
#endif
