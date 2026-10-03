/* SPDX-License-Identifier: Apache-2.0 */
#include "lv_aic_spi_transfer.h"
#include "lvgl.h"
#include <assert.h>
#include <string.h>
static lv_aic_spi_transfer_t *session;
static uint8_t tx[12],snapshot[12];
static unsigned starts,waits;
static bool fail_start,fail_wait;
static bool start(void *ctx,const uint8_t *pixels,size_t bytes)
{
    assert(ctx==tx && pixels==tx && bytes==12);starts++;
    memcpy(snapshot,tx,12);
    assert(lv_aic_spi_transfer_close(session)==LV_AIC_SPI_BUSY);
    assert(lv_aic_spi_transfer_submit(session,NULL,0)==LV_AIC_SPI_BUSY);
    return !fail_start;
}
static bool wait_done(void *ctx)
{
    assert(ctx==tx && !memcmp(snapshot,tx,12));waits++;
    assert(lv_aic_spi_transfer_drain(session)==LV_AIC_SPI_BUSY);
    return !fail_wait;
}
int main(void)
{
    lv_init();
    uint8_t pixels[12]={1,2,3,4,5,6,7,8,9,10,11,12};
    lv_aic_spi_rgb565_frame_t frame={pixels,12,6,3,2};
    lv_aic_spi_transfer_ops_t ops={start,wait_done,tx};
    assert(!lv_aic_spi_transfer_create(tx,11,3,2,false,&ops));
    for(unsigned n=0;n<10;n++) {
        session=lv_aic_spi_transfer_create(tx,12,3,2,true,&ops);assert(session);
        unsigned old=waits;
        assert(lv_aic_spi_transfer_drain(session)==LV_AIC_SPI_OK && waits==old);
        assert(lv_aic_spi_transfer_submit(session,&frame,0)==LV_AIC_SPI_OK);
        assert(tx[0]==2 && tx[1]==1);
        assert(lv_aic_spi_transfer_submit(session,&frame,180)==LV_AIC_SPI_OK && waits==old+1);
        assert(tx[0]==12 && tx[1]==11);
        assert(lv_aic_spi_transfer_submit(session,&frame,45)==LV_AIC_SPI_INVALID && waits==old+2);
        assert(lv_aic_spi_transfer_close(session)==LV_AIC_SPI_OK && waits==old+2);
    }
    /* Faulted metadata intentionally retained. Never replay or overwrite after
     * uncertain transfer. No real DMA exists in this host process. */
    for(unsigned failure=0;failure<2;failure++) {
        session=lv_aic_spi_transfer_create(tx,12,3,2,false,&ops);assert(session);
        fail_start=failure==0;fail_wait=failure==1;
        assert(lv_aic_spi_transfer_submit(session,&frame,0)==(fail_start?LV_AIC_SPI_FAULT:LV_AIC_SPI_OK));
        assert(lv_aic_spi_transfer_close(session)==LV_AIC_SPI_FAULT);
        unsigned old_starts=starts,old_waits=waits;
        assert(lv_aic_spi_transfer_submit(session,&frame,180)==LV_AIC_SPI_FAULT);
        assert(lv_aic_spi_transfer_close(session)==LV_AIC_SPI_FAULT);
        assert(starts==old_starts && waits==old_waits && !memcmp(tx,snapshot,12));
    }
    return 0;
}
