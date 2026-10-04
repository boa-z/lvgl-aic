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
typedef struct {
    lv_aic_spi_transfer_t *s;
    uint8_t front[12],back[12],saved[12];
    const uint8_t *active;
    unsigned transforms,starts,waits;
    bool fail_transform,invalid,fail_wait,fail_start;
    unsigned concurrent_conversions;
} overlap_t;
static overlap_t overlap[4];
static bool overlap_start(void *context,const uint8_t *pixels,size_t bytes)
{
    overlap_t *c=context;assert(bytes==12 && !c->active);
    c->active=pixels;memcpy(c->saved,pixels,12);c->starts++;
    assert(lv_aic_spi_transfer_drain(c->s)==LV_AIC_SPI_BUSY);
    return !c->fail_start;
}
static bool overlap_wait(void *context)
{
    overlap_t *c=context;assert(c->active && !memcmp(c->active,c->saved,12));
    c->waits++;if(c->fail_wait) return false;
    c->active=NULL;return true;
}
static lv_aic_spi_result_t overlap_transform(void *context,const lv_aic_spi_rgb565_frame_t *f,
    uint8_t *out,size_t cap,uint32_t w,uint32_t h,unsigned degrees,bool swap)
{
    overlap_t *c=context;c->transforms++;
    if(c->active) { c->concurrent_conversions++;assert(out!=c->active && !memcmp(c->active,c->saved,12)); }
    assert(lv_aic_spi_transfer_close(c->s)==LV_AIC_SPI_BUSY);
    if(c->fail_transform) return LV_AIC_SPI_FAULT;
    if(c->invalid) return LV_AIC_SPI_INVALID;
    assert(lv_aic_spi_pack_rgb565(f,out,cap,w,h,degrees,swap));
    if(c->active) assert(!memcmp(c->active,c->saved,12));
    return LV_AIC_SPI_OK;
}
static void overlap_contract(void)
{
    uint8_t pixels[12]={1,2,3,4,5,6,7,8,9,10,11,12};
    lv_aic_spi_rgb565_frame_t frame={pixels,12,6,3,2};
    for(unsigned n=0;n<4;n++) {
        overlap_t *c=&overlap[n];
        lv_aic_spi_transfer_ops_t ops={overlap_start,overlap_wait,c};
        c->s=lv_aic_spi_transfer_create(c->front,12,3,2,false,&ops);assert(c->s);
        assert(!lv_aic_spi_transfer_set_back_buffer(c->s,c->front,12));
        assert(!lv_aic_spi_transfer_set_back_buffer(c->s,c->back,11));
        assert(lv_aic_spi_transfer_set_back_buffer(c->s,c->back,12));
        assert(!lv_aic_spi_transfer_set_back_buffer(c->s,c->back,12));
        assert(lv_aic_spi_transfer_set_transform(c->s,overlap_transform,c));
        assert(lv_aic_spi_transfer_submit(c->s,&frame,0)==LV_AIC_SPI_OK);
        assert(c->active==c->front && c->waits==0);
        c->invalid=true;
        assert(lv_aic_spi_transfer_submit(c->s,&frame,0)==LV_AIC_SPI_INVALID);
        assert(c->active==c->front && c->waits==0 && c->transforms==2);
        c->invalid=false;
        if(n) {
            c->fail_transform=n==1;c->fail_wait=n==2;c->fail_start=n==3;
            assert(lv_aic_spi_transfer_submit(c->s,&frame,180)==LV_AIC_SPI_FAULT);
            unsigned transforms=c->transforms,starts=c->starts,waits=c->waits;
            assert(lv_aic_spi_transfer_submit(c->s,&frame,0)==LV_AIC_SPI_FAULT);
            assert(lv_aic_spi_transfer_close(c->s)==LV_AIC_SPI_FAULT);
            assert(c->transforms==transforms && c->starts==starts && c->waits==waits);
            assert(!memcmp(c->active,c->saved,12));
            continue;
        }
        for(unsigned i=0;i<3;i++) {
            assert(lv_aic_spi_transfer_submit(c->s,&frame,180)==LV_AIC_SPI_OK);
            assert(c->active==(i%2?c->front:c->back) && c->waits==i+1);
            assert(c->active[0]==11 && c->active[1]==12);
            /* A wait-before-transform implementation would see active==NULL
             * inside the callback. Count proves no extra drain on INVALID. */
        }
        assert(c->transforms==5 && c->concurrent_conversions==4);
        assert(lv_aic_spi_transfer_close(c->s)==LV_AIC_SPI_OK && c->waits==4);
    }
}
int main(void)
{
    lv_init();
    overlap_contract();
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
