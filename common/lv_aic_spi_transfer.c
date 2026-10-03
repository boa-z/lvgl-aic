/* SPDX-License-Identifier: Apache-2.0 */
#include "lv_aic_spi_transfer.h"
#include "lvgl.h"
struct lv_aic_spi_transfer {
    uint8_t *tx;
    size_t capacity;
    uint32_t width,height;
    lv_aic_spi_transfer_ops_t ops;
    bool swap,pending,busy,fault;
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
static lv_aic_spi_result_t finish(lv_aic_spi_transfer_t *s)
{
    if(s->fault) return LV_AIC_SPI_FAULT;
    if(s->pending) {
        if(!s->ops.wait(s->ops.context)) { s->fault=true;return LV_AIC_SPI_FAULT; }
        s->pending=false;
    }
    return LV_AIC_SPI_OK;
}
lv_aic_spi_result_t lv_aic_spi_transfer_submit(lv_aic_spi_transfer_t *s,
    const lv_aic_spi_rgb565_frame_t *source,unsigned degrees)
{
    if(!s) return LV_AIC_SPI_INVALID;
    if(s->busy) return LV_AIC_SPI_BUSY;
    s->busy=true;
    lv_aic_spi_result_t result=finish(s);
    if(result==LV_AIC_SPI_OK) {
        if(!lv_aic_spi_pack_rgb565(source,s->tx,s->capacity,s->width,s->height,degrees,s->swap))
            result=LV_AIC_SPI_INVALID;
        else {
            s->pending=true; /* Even failed submission may have started DMA. */
            if(!s->ops.start(s->ops.context,s->tx,(size_t)s->width*s->height*2)) {
                s->fault=true;result=LV_AIC_SPI_FAULT;
            }
        }
    }
    s->busy=false;return result;
}
lv_aic_spi_result_t lv_aic_spi_transfer_drain(lv_aic_spi_transfer_t *s)
{
    if(!s) return LV_AIC_SPI_INVALID;
    if(s->busy) return LV_AIC_SPI_BUSY;
    s->busy=true;lv_aic_spi_result_t result=finish(s);s->busy=false;return result;
}
lv_aic_spi_result_t lv_aic_spi_transfer_close(lv_aic_spi_transfer_t *s)
{
    lv_aic_spi_result_t result=lv_aic_spi_transfer_drain(s);
    if(result==LV_AIC_SPI_OK) lv_free(s);
    return result;
}
