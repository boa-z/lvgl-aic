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
static unsigned submits,modes;
static rt_uint32_t submitted_status;
static int mode_error;
static size_t accepted=64;
static struct rt_qspi_message last;
int rt_spi_nonblock_set(struct rt_spi_device *d,unsigned mode) { assert(d && mode==1);modes++;return mode_error; }
size_t rt_qspi_transfer_message(struct rt_qspi_device *d,struct rt_qspi_message *m)
{ assert(d);submits++;last=*m;status=submitted_status;return accepted; }
static void test_submit(struct rt_spi_bus *bus)
{
    struct rt_qspi_device q={{bus}};
    const uint8_t *pixels=(const uint8_t *)(uintptr_t)0x42000000;
    status=0;
    assert(lv_aic_spi_sdk_submit_qspi(&q,pixels,64,0x32002c00,4,1,4));
    assert(submits==1 && modes==1 && last.parent.send_buf==pixels && last.parent.length==64);
    assert(last.address.content==0x32002c00 && last.address.size==4 && last.address.qspi_lines==1);
    assert(last.qspi_data_lines==4 && last.parent.cs_take && last.parent.cs_release);
    assert(!last.parent.recv_buf && !last.parent.next && !last.instruction.qspi_lines &&
           !last.alternate_bytes.size && !last.dummy_cycles);
    accepted=63;assert(!lv_aic_spi_sdk_submit_qspi(&q,pixels,64,0,0,0,1));
    accepted=0;assert(!lv_aic_spi_sdk_submit_qspi(&q,pixels,64,0,0,0,1));accepted=64;
    unsigned old=submits;
    mode_error=-1;assert(!lv_aic_spi_sdk_submit_qspi(&q,pixels,64,0,0,0,1));mode_error=0;
    status=HAL_QSPI_STATUS_IN_PROGRESS;assert(!lv_aic_spi_sdk_submit_qspi(&q,pixels,64,0,0,0,1));status=0;
    assert(!lv_aic_spi_sdk_submit_qspi(&q,pixels,64,0,5,1,1));
    assert(!lv_aic_spi_sdk_submit_qspi(&q,pixels,64,0,4,3,1));
    assert(!lv_aic_spi_sdk_submit_qspi(&q,pixels,64,0,0,1,1));
    assert(!lv_aic_spi_sdk_submit_qspi(&q,pixels,64,0,0,0,3));
    assert(!lv_aic_spi_sdk_submit_qspi(&q,pixels,0,0,0,0,1));
    assert(!lv_aic_spi_sdk_submit_qspi(&q,(void *)(uintptr_t)0xfffffff0,64,0,0,0,1));
    const struct rt_spi_ops *saved=bus->ops;
    for(unsigned missing=0;missing<3;missing++) {
        struct rt_spi_ops broken=*saved;
        if(missing==0) broken.configure=NULL;
        if(missing==1) broken.xfer=NULL;
        if(missing==2) broken.nonblock=NULL;
        bus->ops=&broken;
        assert(!lv_aic_spi_sdk_submit_qspi(&q,pixels,64,0,0,0,1));
    }
    bus->ops=saved;
    assert(submits==old);
}
static void test_write(struct rt_spi_bus *bus)
{
    struct rt_qspi_device q={{bus}};
    const uint8_t *command=(const uint8_t *)(uintptr_t)0x42000100;
    accepted=1;status=0;error=0;
    unsigned before=waits;
    assert(lv_aic_spi_sdk_write_qspi(&q,command,1,0,0,0,1));
    assert(waits==before+1 && last.parent.send_buf==command && last.parent.length==1);
    accepted=0;before=waits;
    assert(!lv_aic_spi_sdk_write_qspi(&q,command,1,0,0,0,1));
    assert(waits==before); /* A short submit cannot be promoted by a good wait. */
    accepted=1;error=-1;
    assert(!lv_aic_spi_sdk_write_qspi(&q,command,1,0,0,0,1));
    assert(waits==before+1);error=0;
    submitted_status=HAL_QSPI_STATUS_IN_PROGRESS;
    assert(!lv_aic_spi_sdk_write_qspi(&q,command,1,0,0,0,1));
    status=0;submitted_status=HAL_QSPI_STATUS_TRAN_DONE | 2U;
    assert(!lv_aic_spi_sdk_write_qspi(&q,command,1,0,0,0,1));
    status=0;submitted_status=HAL_QSPI_STATUS_TRAN_DONE;
    assert(lv_aic_spi_sdk_write_qspi(&q,command,1,0,0,0,1));
    submitted_status=0;
    status=HAL_QSPI_STATUS_IN_PROGRESS;before=submits;
    assert(!lv_aic_spi_sdk_write_qspi(&q,command,1,0,0,0,1));
    assert(submits==before);status=0;
    assert(!lv_aic_spi_sdk_write_qspi(NULL,command,1,0,0,0,1));
}
int main(void)
{
    struct rt_spi_ops ops={present,present,present,present,present};
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
    assert(waits==65 && queries==64);test_submit(&bus);test_write(&bus);return 0;
}
