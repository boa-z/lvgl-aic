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
#endif
