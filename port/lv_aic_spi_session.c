/* SPDX-License-Identifier: Apache-2.0 */
#include "lvgl_aic_feature_config.h"
#if defined(AIC_LVGL_USE_SPI_SDK) && AIC_LVGL_USE_SPI_SDK
#include "lv_aic_spi_session.h"
#include "lv_aic_spi_sdk.h"
#include "lv_aic_spi_ge2d.h"
#include "lvgl.h"
#include <rtdevice.h>
#include <aic_osal.h>
#include <hal_qspi.h>
struct lv_aic_spi_session {
    lv_aic_spi_session_config_t config;
    struct rt_spi_bus *bus;
    size_t cache_bytes;
    bool owns_tx;
    uint8_t *back;
    lv_aic_spi_transfer_t *transfer;
    lv_aic_spi_ge2d_t *ge;
    struct lv_aic_spi_session *next;
};
static lv_aic_spi_session_t *sessions;
static volatile unsigned registry_busy;
static bool take_registry(void) { return !__sync_lock_test_and_set(&registry_busy,1); }
static void release_registry(void) { __sync_lock_release(&registry_busy); }
static bool start(void *context,const uint8_t *pixels,size_t bytes)
{
    lv_aic_spi_session_t *s=context;
    if(s->config.device->parent.bus!=s->bus) return false;
    if(s->config.prepare && !s->config.prepare(s->config.prepare_context,
            s->config.width,s->config.height)) return false;
    /* A panel callback must not replace the claimed device bus. */
    if(s->config.device->parent.bus!=s->bus) return false;
    aicos_dcache_clean_range((unsigned long *)pixels,(unsigned long)s->cache_bytes);
    return lv_aic_spi_sdk_submit_qspi(s->config.device,pixels,bytes,s->config.prefix,
        s->config.prefix_bytes,s->config.prefix_lines,s->config.data_lines);
}
static bool wait_complete(void *context)
{
    lv_aic_spi_session_t *s=context;
    return s->config.device->parent.bus==s->bus &&
        lv_aic_spi_sdk_wait_complete(&s->config.device->parent);
}
static bool lines(unsigned value) { return value==1 || value==2 || value==4; }
lv_aic_spi_session_t *lv_aic_spi_session_open(const lv_aic_spi_session_config_t *c)
{
    if(!c || !c->device || !c->tx || !c->width || !c->height || c->width>4096 || c->height>4096 ||
       !lines(c->data_lines) || c->prefix_bytes>4 ||
       (c->prefix_bytes ? !lines(c->prefix_lines) : c->prefix_lines!=0)) return NULL;
    size_t span=((size_t)c->width*c->height*2+63)&~(size_t)63;
    uintptr_t address=(uintptr_t)c->tx;
    if((address&63) || span>c->capacity || address>UINT32_MAX ||
       span>(uint64_t)UINT32_MAX+1-address) return NULL;
#if defined(AIC_CHIP_D13X) || defined(AIC_CHIP_G73X)
    if(address<0x40000000U) return NULL;
#endif
    struct rt_spi_bus *bus=c->device->parent.bus;
    if(!bus || !bus->ops || !bus->ops->configure || !bus->ops->xfer ||
       !bus->ops->nonblock || !bus->ops->wait_completion || !bus->ops->gstatus || !take_registry()) return NULL;
    for(lv_aic_spi_session_t *p=sessions;p;p=p->next) {
        uintptr_t previous=(uintptr_t)p->config.tx;
        uintptr_t back=(uintptr_t)p->back;
        if(p->bus==bus || ((uint64_t)address<(uint64_t)previous+p->cache_bytes &&
                          (uint64_t)previous<(uint64_t)address+span) ||
           (back && (uint64_t)address<(uint64_t)back+p->cache_bytes &&
                    (uint64_t)back<(uint64_t)address+span)) { release_registry();return NULL; }
    }
    rt_uint32_t status=rt_spi_get_transfer_status(&c->device->parent);
    if(status!=HAL_QSPI_STATUS_OK && status!=HAL_QSPI_STATUS_TRAN_DONE) { release_registry();return NULL; }
    lv_aic_spi_session_t *s=lv_malloc_zeroed(sizeof(*s));
    if(s) {
        s->config=*c;s->bus=bus;s->cache_bytes=span;
        lv_aic_spi_transfer_ops_t ops={start,wait_complete,s};
        s->transfer=lv_aic_spi_transfer_create(c->tx,c->capacity,c->width,c->height,c->swap_bytes,&ops);
        if(!s->transfer) { lv_free(s);s=NULL; }
        else { s->next=sessions;sessions=s; }
    }
    release_registry();return s;
}
lv_aic_spi_session_t *lv_aic_spi_session_open_owned(const lv_aic_spi_session_config_t *c,size_t budget)
{
    if(!c || c->tx || c->capacity || !c->width || !c->height || c->width>4096 || c->height>4096)
        return NULL;
    size_t bytes=((size_t)c->width*c->height*2+63)&~(size_t)63;
    if(bytes>budget) return NULL;
    lv_aic_spi_session_config_t owned=*c;
    owned.tx=aicos_malloc_align(MEM_CMA,bytes,64);
    if(!owned.tx) return NULL;
    owned.capacity=bytes;
    lv_aic_spi_session_t *s=lv_aic_spi_session_open(&owned);
    if(!s) aicos_free_align(MEM_CMA,owned.tx);
    else s->owns_tx=true;
    return s;
}
bool lv_aic_spi_session_enable_overlap(lv_aic_spi_session_t *s,size_t budget)
{
    if(!s || !s->transfer || s->back || s->cache_bytes>budget) return false;
    uint8_t *back=aicos_malloc_align(MEM_CMA,s->cache_bytes,64);
    if(!back) return false;
    uintptr_t address=(uintptr_t)back;
    bool valid=!(address&63) && address<=UINT32_MAX &&
        s->cache_bytes<=(uint64_t)UINT32_MAX+1-address;
#if defined(AIC_CHIP_D13X) || defined(AIC_CHIP_G73X)
    valid=valid && address>=0x40000000U;
#endif
    if(!valid || !take_registry()) { aicos_free_align(MEM_CMA,back);return false; }
    for(lv_aic_spi_session_t *p=sessions;p && valid;p=p->next) {
        uintptr_t regions[2]={(uintptr_t)p->config.tx,(uintptr_t)p->back};
        for(unsigned i=0;i<2;i++) if(regions[i] &&
            (uint64_t)address<(uint64_t)regions[i]+p->cache_bytes &&
            (uint64_t)regions[i]<(uint64_t)address+s->cache_bytes) valid=false;
    }
    if(valid) valid=lv_aic_spi_transfer_set_back_buffer(s->transfer,back,s->cache_bytes);
    if(valid) s->back=back;
    release_registry();
    if(!valid) aicos_free_align(MEM_CMA,back);
    return valid;
}
#if AIC_LVGL_USE_GE2D && AIC_LVGL_BSP_MPP
static lv_aic_spi_result_t transform(void *context,const lv_aic_spi_rgb565_frame_t *source,
    uint8_t *output,size_t capacity,uint32_t width,uint32_t height,unsigned degrees,bool swap)
{
    lv_aic_spi_session_t *s=context;
    lv_aic_spi_result_t result=lv_aic_spi_ge2d_convert(s->ge,source,output,capacity,degrees,swap);
    if(result==LV_AIC_SPI_INVALID)
        return lv_aic_spi_pack_rgb565(source,output,capacity,width,height,degrees,swap)?
            LV_AIC_SPI_OK:LV_AIC_SPI_INVALID;
    return result;
}
#endif
bool lv_aic_spi_session_enable_ge2d(lv_aic_spi_session_t *s,uint32_t mw,uint32_t mh,size_t budget)
{
#if AIC_LVGL_USE_GE2D && AIC_LVGL_BSP_MPP
    if(!s || !s->transfer || s->ge) return false;
    lv_aic_spi_ge2d_t *ge=lv_aic_spi_ge2d_create(mw,mh,s->config.width,s->config.height,budget);
    if(!ge) return false;
    if(!lv_aic_spi_transfer_set_transform(s->transfer,transform,s)) {
        lv_aic_spi_ge2d_close(ge);return false;
    }
    s->ge=ge;return true;
#else
    (void)s;(void)mw;(void)mh;(void)budget;return false;
#endif
}
lv_aic_spi_result_t lv_aic_spi_session_submit(lv_aic_spi_session_t *s,
    const lv_aic_spi_rgb565_frame_t *source,unsigned degrees)
{ return s ? lv_aic_spi_transfer_submit(s->transfer,source,degrees) : LV_AIC_SPI_INVALID; }
lv_aic_spi_result_t lv_aic_spi_session_drain(lv_aic_spi_session_t *s)
{ return s ? lv_aic_spi_transfer_drain(s->transfer) : LV_AIC_SPI_INVALID; }
lv_aic_spi_result_t lv_aic_spi_session_close(lv_aic_spi_session_t *s)
{
    if(!s) return LV_AIC_SPI_INVALID;
    if(s->transfer) {
        lv_aic_spi_result_t result=lv_aic_spi_transfer_close(s->transfer);
        if(result!=LV_AIC_SPI_OK) return result;
        s->transfer=NULL;
    }
#if AIC_LVGL_USE_GE2D && AIC_LVGL_BSP_MPP
    if(s->ge) {
        lv_aic_spi_result_t result=lv_aic_spi_ge2d_close(s->ge);
        if(result!=LV_AIC_SPI_OK) return result;
        s->ge=NULL;
    }
#endif
    /* Never hold the shared registry across driver completion waits. */
    if(!take_registry()) return LV_AIC_SPI_BUSY;
    lv_aic_spi_session_t **slot=&sessions;
    while(*slot && *slot!=s) slot=&(*slot)->next;
    if(*slot) *slot=s->next;
    release_registry();
    if(s->back) aicos_free_align(MEM_CMA,s->back);
    if(s->owns_tx) aicos_free_align(MEM_CMA,s->config.tx);
    lv_free(s);return LV_AIC_SPI_OK;
}
#endif
