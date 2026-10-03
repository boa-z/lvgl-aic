/* SPDX-License-Identifier: Apache-2.0 */
#include "lv_aic_spi_panel.h"
#include "lvgl.h"
#include <rtdevice.h>
#include <assert.h>
static unsigned pins,cleans,writes,waits,status;
static int fail_wait,fail_pin;
static lv_aic_spi_panel_t *active;
static void present(void) {}
static bool dc(void *context,bool data)
{
    assert(context==&pins && status==0);
    assert(data==(pins%3!=0));
    pins++;
    if(active) assert(!lv_aic_spi_panel_close(active));
    return !fail_pin;
}
void aicos_dcache_clean_range(unsigned long *ptr,unsigned long bytes)
{ assert((uintptr_t)ptr==0x48000000U+(cleans%2)*64 && bytes==64);cleans++; }
rt_uint32_t rt_spi_get_transfer_status(struct rt_spi_device *d) { assert(d);return status; }
int rt_spi_nonblock_set(struct rt_spi_device *d,unsigned mode) { assert(d && mode==1);return 0; }
size_t rt_qspi_transfer_message(struct rt_qspi_device *d,struct rt_qspi_message *m)
{ assert(d && cleans==writes+1 && m->parent.length==1);writes++;status=1;return 1; }
int rt_spi_wait_completion(struct rt_spi_device *d)
{ assert(d && status==1);waits++;status=0;return fail_wait?-1:0; }
int main(void)
{
    lv_init();struct rt_spi_ops ops={present,present,present,present,present};
    struct rt_spi_bus bus={&ops};struct rt_qspi_device device={{&bus}};
    lv_aic_spi_panel_step_t steps[2]={
        {.data=(void *)(uintptr_t)0x48000000,.bytes=1,.capacity=64,.data_lines=1},
        {.data=(void *)(uintptr_t)0x48000040,.bytes=1,.capacity=64,.data_lines=1,.data_mode=true}};
    steps[1].capacity=1;
    assert(!lv_aic_spi_panel_create(&device,steps,2,3,2,dc,&pins,true));
    assert(!pins && !writes);steps[1].capacity=64;
    active=lv_aic_spi_panel_create(&device,steps,2,3,2,dc,&pins,true);assert(active);
    steps[0].data_lines=3; /* descriptors are snapshotted */
    assert(!lv_aic_spi_panel_prepare(active,2,3) && !pins);
    assert(lv_aic_spi_panel_prepare(active,3,2));
    assert(lv_aic_spi_panel_prepare(active,3,2));
    assert(pins==6 && writes==4 && waits==4 && cleans==4);
    assert(lv_aic_spi_panel_close(active));active=NULL;steps[0].data_lines=1;
    active=lv_aic_spi_panel_create(&device,steps,2,3,2,dc,&pins,true);assert(active);
    fail_wait=1;assert(!lv_aic_spi_panel_prepare(active,3,2));
    assert(pins==7 && writes==5 && waits==5);
    fail_wait=0;assert(!lv_aic_spi_panel_prepare(active,3,2));
    assert(!lv_aic_spi_panel_close(active) && writes==5);active=NULL;
    /* Independent command owner: pin failure submits nothing. */
    struct rt_spi_bus other_bus={&ops};struct rt_qspi_device other={{&other_bus}};
    device=other; /* Never reuse a possibly faulted hardware bus. */
    pins=cleans=writes=waits=0;
    active=lv_aic_spi_panel_create(&device,steps,2,3,2,dc,&pins,true);assert(active);
    fail_pin=1;assert(!lv_aic_spi_panel_prepare(active,3,2));
    assert(pins==1 && !writes && !cleans && !lv_aic_spi_panel_close(active));
    return 0;
}
