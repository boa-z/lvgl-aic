/* SPDX-License-Identifier: Apache-2.0 */
#include "lv_aic_spi_session.h"
#include "lvgl.h"
#include <rtdevice.h>
#include <assert.h>
#include <aic_osal.h>
#include <string.h>
#ifdef _WIN32
#include <windows.h>
#else
#include <sys/mman.h>
#endif
static unsigned cleans,submits,waits,prepares;
static bool fail_prepare;
static lv_aic_spi_session_t *preparing_session;
static unsigned status;
static int wait_error;
static uint8_t *tx;
static uint8_t saved[12];
static unsigned allocs,frees;
static bool fail_alloc,bad_alignment;
static uint8_t *owned_pixels;
void *aicos_malloc_align(unsigned int type,size_t bytes,size_t alignment)
{
    assert(type==MEM_CMA && bytes==64 && alignment==64);
    if(fail_alloc) return NULL;
    allocs++;return owned_pixels+(bad_alignment?1:0);
}
void aicos_free_align(unsigned int type,void *pointer)
{ assert(type==MEM_CMA && (pointer==owned_pixels || pointer==owned_pixels+1));frees++; }
static bool prepare(void *context,uint32_t width,uint32_t height)
{
    assert(context==&prepares && width==3 && height==2);
    assert(status==0 && cleans==submits); /* Prior DMA drained; no new cache handoff. */
    if(preparing_session) {
        assert(lv_aic_spi_session_drain(preparing_session)==LV_AIC_SPI_BUSY);
        assert(lv_aic_spi_session_close(preparing_session)==LV_AIC_SPI_BUSY);
    }
    prepares++;
    return !fail_prepare;
}
static void present(void) {}
void aicos_dcache_clean_range(unsigned long *p,unsigned long bytes)
{ assert((uint8_t *)p==tx && bytes==64);cleans++; }
int rt_spi_wait_completion(struct rt_spi_device *d)
{ assert(d && !memcmp(tx,saved,12));waits++;status=0;return wait_error; }
rt_uint32_t rt_spi_get_transfer_status(struct rt_spi_device *d) { assert(d);return status; }
int rt_spi_nonblock_set(struct rt_spi_device *d,unsigned mode) { assert(d && mode==1);return 0; }
size_t rt_qspi_transfer_message(struct rt_qspi_device *d,struct rt_qspi_message *m)
{
    assert(d && cleans==submits+1 && m->parent.send_buf==tx && m->parent.length==12);
    memcpy(saved,tx,12);submits++;status=1;return 12;
}
int main(void)
{
    lv_init();
#ifdef _WIN32
    tx=VirtualAlloc((void *)(uintptr_t)0x48000000,65536,MEM_RESERVE|MEM_COMMIT,PAGE_READWRITE);
#else
    tx=mmap((void *)(uintptr_t)0x48000000,65536,PROT_READ|PROT_WRITE,MAP_PRIVATE|MAP_ANONYMOUS|MAP_FIXED_NOREPLACE,-1,0);
#endif
    assert(tx==(void *)(uintptr_t)0x48000000);
    memset(tx,0xa5,65536);
    struct rt_spi_ops ops={present,present,present,present,present};
    struct rt_spi_bus bus={&ops},other_bus={&ops};
    struct rt_qspi_device device={{&bus}},other={{&bus}};
    lv_aic_spi_session_config_t c={.device=&device,.tx=tx,.capacity=64,.width=3,.height=2,.data_lines=1,.swap_bytes=true,
        .prepare=prepare,.prepare_context=&prepares};
    assert(!lv_aic_spi_session_enable_ge2d(NULL,5,4,512));
    c.capacity=12;assert(!lv_aic_spi_session_open(&c));c.capacity=64;
    status=1;assert(!lv_aic_spi_session_open(&c));status=0;
    lv_aic_spi_session_t *s=lv_aic_spi_session_open(&c);assert(s);
    assert(!lv_aic_spi_session_open(&c));
    c.device=&other;c.tx=tx+128;assert(!lv_aic_spi_session_open(&c)); /* same bus */
    other.parent.bus=&other_bus;c.tx=tx;assert(!lv_aic_spi_session_open(&c)); /* same tx */
    c.tx=tx+128;lv_aic_spi_session_t *second=lv_aic_spi_session_open(&c);assert(second);
    assert(lv_aic_spi_session_close(second)==LV_AIC_SPI_OK && waits==0);
    uint8_t input[12]={1,2,3,4,5,6,7,8,9,10,11,12};
    lv_aic_spi_rgb565_frame_t f={input,12,6,3,2};
    preparing_session=s;
    assert(lv_aic_spi_session_submit(s,&f,0)==LV_AIC_SPI_OK);
    assert(tx[0]==2 && tx[1]==1 && tx[12]==0xa5);
    assert(lv_aic_spi_session_submit(s,&f,180)==LV_AIC_SPI_OK && waits==1);
    assert(tx[0]==12 && tx[1]==11);
    assert(lv_aic_spi_session_close(s)==LV_AIC_SPI_OK && waits==2);
    preparing_session=NULL;
    assert(prepares==2);
    uint8_t *borrowed=tx;owned_pixels=tx+4096;
    c.device=&device;c.tx=NULL;c.capacity=0;
    assert(!lv_aic_spi_session_open_owned(&c,63) && !allocs);
    fail_alloc=true;assert(!lv_aic_spi_session_open_owned(&c,64));fail_alloc=false;
    bad_alignment=true;assert(!lv_aic_spi_session_open_owned(&c,64));bad_alignment=false;
    assert(allocs==frees);
    c.data_lines=3;assert(!lv_aic_spi_session_open_owned(&c,64));c.data_lines=1;
    assert(allocs==frees);
    for(unsigned cycle=0;cycle<10;cycle++) {
        s=lv_aic_spi_session_open_owned(&c,64);assert(s);tx=owned_pixels;
        assert(lv_aic_spi_session_submit(s,&f,0)==LV_AIC_SPI_OK);
        unsigned before_free=frees;
        assert(lv_aic_spi_session_close(s)==LV_AIC_SPI_OK && frees==before_free+1);
        assert(allocs==frees);
    }
    /* Owned fault retains CMA just as borrowed faults retain caller storage. */
    s=lv_aic_spi_session_open_owned(&c,64);assert(s);tx=owned_pixels;
    assert(lv_aic_spi_session_submit(s,&f,0)==LV_AIC_SPI_OK);
    wait_error=-1;unsigned before_free=frees;
    assert(lv_aic_spi_session_close(s)==LV_AIC_SPI_FAULT && frees==before_free);
    assert(allocs==frees+1);
    /* A real allocator must not recycle the still DMA-owned block. */
    owned_pixels+=64;
    assert(!lv_aic_spi_session_open_owned(&c,64) && allocs==frees+1);
    /* A second bus can independently exercise borrowed fault retention. */
    wait_error=0;tx=borrowed;c.device=&other;c.tx=tx;c.capacity=64;
    s=lv_aic_spi_session_open(&c);assert(s);
    assert(lv_aic_spi_session_submit(s,&f,0)==LV_AIC_SPI_OK);
    wait_error=-1;assert(lv_aic_spi_session_close(s)==LV_AIC_SPI_FAULT);
    assert(!lv_aic_spi_session_open(&c));
    unsigned before=submits;
    assert(lv_aic_spi_session_submit(s,&f,180)==LV_AIC_SPI_FAULT && submits==before);
    assert(!memcmp(saved,tx,12)); /* Fault retains metadata and mapping until process exit. */
    /* Failed panel command setup never hands pixels to DMA, and stays faulted. */
    struct rt_spi_bus third_bus={&ops};
    struct rt_qspi_device third={{&third_bus}};
    c.device=&third;c.tx=borrowed+8192;tx=c.tx;status=0;wait_error=0;
    s=lv_aic_spi_session_open(&c);assert(s);
    fail_prepare=true;before=submits;unsigned before_prepare=prepares;
    assert(lv_aic_spi_session_submit(s,&f,0)==LV_AIC_SPI_FAULT);
    assert(submits==before && cleans==submits && prepares==before_prepare+1);
    fail_prepare=false;
    assert(lv_aic_spi_session_submit(s,&f,0)==LV_AIC_SPI_FAULT);
    assert(prepares==before_prepare+1 && lv_aic_spi_session_close(s)==LV_AIC_SPI_FAULT);
    assert(!lv_aic_spi_session_open(&c));
    return 0;
}
