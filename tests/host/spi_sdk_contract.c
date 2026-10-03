/* SPDX-License-Identifier: Apache-2.0 */
#include "lv_aic_spi_sdk.h"
#include <rtdevice.h>
#include <hal_qspi.h>
#include <assert.h>
#include <stddef.h>
static int error;
static unsigned waits,queries;
static rt_uint32_t status;
int rt_spi_wait_completion(struct rt_spi_device *d) { assert(d);waits++;return error; }
rt_uint32_t rt_spi_get_transfer_status(struct rt_spi_device *d) { assert(d);queries++;return status; }
static void present(void) {}
int main(void)
{
    struct rt_spi_ops ops={present,present};
    struct rt_spi_bus bus={&ops};struct rt_spi_device device={&bus};
    assert(!lv_aic_spi_sdk_wait_complete(NULL));
    device.bus=NULL;assert(!lv_aic_spi_sdk_wait_complete(&device));device.bus=&bus;
    bus.ops=NULL;assert(!lv_aic_spi_sdk_wait_complete(&device));bus.ops=&ops;
    ops.wait_completion=NULL;assert(!lv_aic_spi_sdk_wait_complete(&device));ops.wait_completion=present;
    ops.gstatus=NULL;assert(!lv_aic_spi_sdk_wait_complete(&device));ops.gstatus=present;
    assert(!waits && !queries);
    error=-1;assert(!lv_aic_spi_sdk_wait_complete(&device));assert(waits==1 && !queries);error=0;
    assert(lv_aic_spi_sdk_wait_complete(&device));
    status=HAL_QSPI_STATUS_TRAN_DONE;assert(lv_aic_spi_sdk_wait_complete(&device));
    for(unsigned bit=0;bit<31;bit++) {
        status=1U<<bit;assert(!lv_aic_spi_sdk_wait_complete(&device));
        status|=HAL_QSPI_STATUS_TRAN_DONE;assert(!lv_aic_spi_sdk_wait_complete(&device));
    }
    assert(waits==65 && queries==64);return 0;
}
