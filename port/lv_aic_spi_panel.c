/* SPDX-License-Identifier: Apache-2.0 */
#include "lvgl_aic_feature_config.h"
#if defined(AIC_LVGL_USE_SPI_SDK) && AIC_LVGL_USE_SPI_SDK
#include "lv_aic_spi_panel.h"
#include "lvgl.h"
#include <aic_osal.h>
#include <rtdevice.h>
#include <hal_qspi.h>
struct lv_aic_spi_panel {
    struct rt_qspi_device *device;
    struct rt_spi_bus *bus;
    size_t count;
    uint32_t width,height;
    bool (*set_dc)(void *,bool);
    void *context;
    bool final_mode,busy,fault;
    lv_aic_spi_panel_step_t steps[];
};
static bool lines(unsigned n) { return n==1 || n==2 || n==4; }
lv_aic_spi_panel_t *lv_aic_spi_panel_create(struct rt_qspi_device *device,
    const lv_aic_spi_panel_step_t *steps,size_t count,uint32_t width,uint32_t height,
    bool (*set_dc)(void *,bool),void *context,bool final_mode)
{
    if(!device || !device->parent.bus || !steps || !count || count>64 ||
       !width || !height || width>4096 || height>4096) return NULL;
    for(size_t i=0;i<count;i++) {
        const lv_aic_spi_panel_step_t *s=&steps[i];uintptr_t a=(uintptr_t)s->data;
        if(!s->data || !s->bytes || s->bytes>INT32_MAX || (a&63) || a>UINT32_MAX ||
           s->prefix_bytes>4 || !lines(s->data_lines) ||
           (s->prefix_bytes ? !lines(s->prefix_lines) : s->prefix_lines!=0)) return NULL;
        size_t rounded=(s->bytes+63)&~(size_t)63;
        if(rounded>s->capacity || rounded>(uint64_t)UINT32_MAX+1-a) return NULL;
#if defined(AIC_CHIP_D13X) || defined(AIC_CHIP_G73X)
        if(a<0x40000000U) return NULL;
#endif
    }
    lv_aic_spi_panel_t *p=lv_malloc_zeroed(sizeof(*p)+count*sizeof(*steps));
    if(!p) return NULL;
    p->device=device;p->bus=device->parent.bus;p->count=count;p->width=width;p->height=height;
    p->set_dc=set_dc;p->context=context;p->final_mode=final_mode;
    lv_memcpy(p->steps,steps,count*sizeof(*steps));return p;
}
static bool idle(lv_aic_spi_panel_t *p)
{
    if(p->device->parent.bus!=p->bus || !p->bus->ops || !p->bus->ops->gstatus) return false;
    rt_uint32_t status=rt_spi_get_transfer_status(&p->device->parent);
    return status==HAL_QSPI_STATUS_OK || status==HAL_QSPI_STATUS_TRAN_DONE;
}
bool lv_aic_spi_panel_prepare(void *context,uint32_t width,uint32_t height)
{
    lv_aic_spi_panel_t *p=context;
    if(!p || p->busy || p->fault || width!=p->width || height!=p->height) return false;
    p->busy=true;
    if(!idle(p)) goto fault;
    for(size_t i=0;i<p->count;i++) {
        const lv_aic_spi_panel_step_t *s=&p->steps[i];
        if(p->set_dc && !p->set_dc(p->context,s->data_mode)) goto fault;
        if(!idle(p)) goto fault;
        aicos_dcache_clean_range((unsigned long *)s->data,(unsigned long)((s->bytes+63)&~(size_t)63));
        if(!lv_aic_spi_sdk_write_qspi(p->device,s->data,s->bytes,s->prefix,
                                    s->prefix_bytes,s->prefix_lines,s->data_lines)) goto fault;
    }
    if(p->set_dc && !p->set_dc(p->context,p->final_mode)) goto fault;
    if(!idle(p)) goto fault;
    p->busy=false;return true;
fault:
    p->fault=true;p->busy=false;return false;
}
bool lv_aic_spi_panel_close(lv_aic_spi_panel_t *p)
{
    if(!p || p->busy || p->fault) return false;
    lv_free(p);return true;
}
#endif
