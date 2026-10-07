/* SPDX-License-Identifier: Apache-2.0 */
#include "lv_aic_spi_transfer.h"
#include "lvgl.h"
struct lv_aic_spi_transfer {
    uint8_t *tx,*back,*active;
    size_t capacity,back_capacity;
    uint32_t width,height;
    lv_aic_spi_transfer_ops_t ops;
    lv_aic_spi_transform_cb_t transform;
    void *transform_context;
    bool swap,pending,busy,fault,started;
};
lv_aic_spi_transfer_t *lv_aic_spi_transfer_create(uint8_t *tx,size_t capacity,
    uint32_t width,uint32_t height,bool swap,const lv_aic_spi_transfer_ops_t *ops)
{
    if(!tx || !ops || !ops->start || !ops->wait || !width || !height ||
       width>4096 || height>4096) return NULL;
    size_t bytes=(size_t)width*height*2;
    if(capacity<bytes || bytes>UINTPTR_MAX-(uintptr_t)tx) return NULL;
    lv_aic_spi_transfer_t *s=lv_malloc_zeroed(sizeof(*s));
    if(!s) return NULL;
    s->tx=tx;s->capacity=capacity;s->width=width;s->height=height;
    s->swap=swap;s->ops=*ops;return s;
}
bool lv_aic_spi_transfer_set_transform(lv_aic_spi_transfer_t *s,
    lv_aic_spi_transform_cb_t transform,void *context)
{
    if(!s || !transform || s->busy || s->started || s->transform || s->fault) return false;
    s->transform=transform;s->transform_context=context;return true;
}
bool lv_aic_spi_transfer_set_back_buffer(lv_aic_spi_transfer_t *s,uint8_t *pixels,size_t capacity)
{
    if(!s || !pixels || s->busy || s->started || s->fault || s->back) return false;
    size_t bytes=(size_t)s->width*s->height*2;
    uintptr_t address=(uintptr_t)pixels,front=(uintptr_t)s->tx;
    if(capacity<bytes || capacity>UINTPTR_MAX-address || s->capacity>UINTPTR_MAX-front ||
       (address<front+s->capacity && front<address+capacity)) return false;
    s->back=pixels;s->back_capacity=capacity;return true;
}
static lv_aic_spi_result_t finish(lv_aic_spi_transfer_t *s,bool *previous_completed)
{
    if(s->fault) return LV_AIC_SPI_FAULT;
    if(s->pending) {
        if(!s->ops.wait(s->ops.context)) { s->fault=true;return LV_AIC_SPI_FAULT; }
        s->pending=false;
        if(previous_completed) *previous_completed=true;
    }
    return LV_AIC_SPI_OK;
}
lv_aic_spi_result_t lv_aic_spi_transfer_submit_ex(lv_aic_spi_transfer_t *s,
    const lv_aic_spi_rgb565_frame_t *source,unsigned degrees,bool *previous_completed)
{
    if(previous_completed) *previous_completed=false;
    if(!s) return LV_AIC_SPI_INVALID;
    if(s->busy) return LV_AIC_SPI_BUSY;
    s->busy=true;s->started=true;
    /* With two buffers, convert into the idle one while transport owns active.
     * Panel setup/start remain after verified completion of the previous DMA. */
    lv_aic_spi_result_t result=s->back ? (s->fault?LV_AIC_SPI_FAULT:LV_AIC_SPI_OK) : finish(s,previous_completed);
    uint8_t *output=s->back && s->pending && s->active==s->tx ? s->back : s->tx;
    size_t capacity=output==s->tx?s->capacity:s->back_capacity;
    if(result==LV_AIC_SPI_OK) {
        if(s->transform)
            result=s->transform(s->transform_context,source,output,capacity,
                s->width,s->height,degrees,s->swap);
        else if(!lv_aic_spi_pack_rgb565(source,output,capacity,s->width,s->height,degrees,s->swap))
            result=LV_AIC_SPI_INVALID;
        if(result!=LV_AIC_SPI_OK && result!=LV_AIC_SPI_INVALID && result!=LV_AIC_SPI_BUSY)
            { s->fault=true;result=LV_AIC_SPI_FAULT; }
        if(result==LV_AIC_SPI_OK) {
            if(s->back) result=finish(s,previous_completed);
        }
        if(result==LV_AIC_SPI_OK) {
            s->active=output;
            s->pending=true; /* Even failed submission may have started DMA. */
            if(!s->ops.start(s->ops.context,output,(size_t)s->width*s->height*2)) {
                s->fault=true;result=LV_AIC_SPI_FAULT;
            }
        }
    }
    s->busy=false;return result;
}
lv_aic_spi_result_t lv_aic_spi_transfer_submit(lv_aic_spi_transfer_t *s,
    const lv_aic_spi_rgb565_frame_t *source,unsigned degrees)
{ return lv_aic_spi_transfer_submit_ex(s,source,degrees,NULL); }
lv_aic_spi_result_t lv_aic_spi_transfer_drain(lv_aic_spi_transfer_t *s)
{
    if(!s) return LV_AIC_SPI_INVALID;
    if(s->busy) return LV_AIC_SPI_BUSY;
    s->busy=true;lv_aic_spi_result_t result=finish(s,NULL);s->busy=false;return result;
}
lv_aic_spi_result_t lv_aic_spi_transfer_close(lv_aic_spi_transfer_t *s)
{
    lv_aic_spi_result_t result=lv_aic_spi_transfer_drain(s);
    if(result==LV_AIC_SPI_OK) lv_free(s);
    return result;
}
