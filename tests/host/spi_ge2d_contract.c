/* SPDX-License-Identifier: Apache-2.0 */
#include "lv_aic_spi_ge2d.h"
#include "lvgl.h"
#include <mpp_ge.h>
#include <aic_osal.h>
#include <assert.h>
#include <string.h>
#ifdef _WIN32
#include <windows.h>
#else
#include <sys/mman.h>
#endif
static uint8_t *arena;
static unsigned allocs,frees,opened,closed,calls,stage;
static int fail_at,fail_alloc;
static bool normal_mode,fail_open;
static struct ge_bitblt command;
static lv_aic_spi_ge2d_t *active;
void *aicos_malloc_align(unsigned int type,size_t bytes,size_t alignment)
{
    assert(type==MEM_CMA && alignment==64 && bytes<=4096);
    if(fail_alloc && (int)(allocs+1)==fail_alloc) { fail_alloc=0;return NULL; }
    assert(allocs<128);return arena+4096*allocs++;
}
void aicos_free_align(unsigned int type,void *p)
{ assert(type==MEM_CMA && p);frees++; }
struct mpp_ge *mpp_ge_open(void)
{ if(fail_open) return NULL;opened++;return (struct mpp_ge *)(uintptr_t)opened; }
void mpp_ge_close(struct mpp_ge *g) { assert(g);closed++; }
enum ge_mode mpp_ge_get_mode(struct mpp_ge *g)
{ assert(g);return normal_mode?GE_MODE_NORMAL:GE_MODE_CMDQ; }
void aicos_dcache_clean_range(unsigned long *p,unsigned long bytes)
{ assert(p && bytes && stage++==0); }
void aicos_dcache_clean_invalid_range(unsigned long *p,unsigned long bytes)
{ assert(p && bytes && stage++==1); }
void aicos_dcache_invalid_range(unsigned long *p,unsigned long bytes)
{ assert(p && bytes && stage++==5); }
int mpp_ge_bitblt(struct mpp_ge *g,struct ge_bitblt *b)
{
    assert(g && stage++==2);calls++;command=*b;
    assert(b->src_buf.buf_type==MPP_PHY_ADDR && b->dst_buf.buf_type==MPP_PHY_ADDR);
    assert(b->src_buf.format==MPP_FMT_RGB_565 && b->dst_buf.format==MPP_FMT_RGB_565);
    assert(!b->ctrl.alpha_en && !(b->src_buf.stride[0]&63) && !(b->dst_buf.stride[0]&63));
    if(active) {
        assert(lv_aic_spi_ge2d_close(active)==LV_AIC_SPI_BUSY);
        assert(lv_aic_spi_ge2d_convert(active,NULL,NULL,0,0,false)==LV_AIC_SPI_BUSY);
    }
    return fail_at==1?-1:0;
}
int mpp_ge_emit(struct mpp_ge *g)
{ assert(g && stage++==3);return fail_at==2?-1:0; }
int mpp_ge_sync(struct mpp_ge *g)
{
    assert(g && stage++==4);
    if(fail_at==3) return -1;
    /* Model the engine only: validate command geometry/cache order, then
     * generate deterministic pixels. Physical filtering is not tested here. */
    unsigned degrees=command.ctrl.flags==MPP_ROTATION_90?90:
        command.ctrl.flags==MPP_ROTATION_180?180:command.ctrl.flags==MPP_ROTATION_270?270:0;
    lv_aic_spi_rgb565_frame_t src={
        .data=(uint8_t *)(uintptr_t)command.src_buf.phy_addr[0],.capacity=4096,
        .width=command.src_buf.size.width,.height=command.src_buf.size.height,
        .stride=command.src_buf.stride[0]};
    uint8_t packed[4096];
    unsigned w=command.dst_buf.size.width,h=command.dst_buf.size.height;
    assert(lv_aic_spi_pack_rgb565(&src,packed,sizeof(packed),w,h,degrees,false));
    for(unsigned y=0;y<h;y++) memcpy((uint8_t *)(uintptr_t)command.dst_buf.phy_addr[0]+
        y*command.dst_buf.stride[0],packed+y*w*2,w*2);
    return 0;
}
int main(void)
{
    lv_init();
#ifdef _WIN32
    arena=VirtualAlloc((void *)(uintptr_t)0x48000000,524288,MEM_RESERVE|MEM_COMMIT,PAGE_READWRITE);
#else
    arena=mmap((void *)(uintptr_t)0x48000000,524288,PROT_READ|PROT_WRITE,
        MAP_PRIVATE|MAP_ANONYMOUS|MAP_FIXED_NOREPLACE,-1,0);
#endif
    assert(arena==(void *)(uintptr_t)0x48000000);
    assert(!lv_aic_spi_ge2d_create(0,2,3,4,4096));
    assert(!lv_aic_spi_ge2d_create(4097,2,3,4,4096));
    assert(!lv_aic_spi_ge2d_create(5,4,7,4,511)); /* 256 + 256 */
    assert(!allocs);
    fail_alloc=1;assert(!lv_aic_spi_ge2d_create(5,4,7,4,512));
    assert(allocs==frees);
    fail_alloc=(int)allocs+2;assert(!lv_aic_spi_ge2d_create(5,4,7,4,512));
    assert(allocs==frees);
    normal_mode=true;assert(!lv_aic_spi_ge2d_create(5,4,7,4,512));normal_mode=false;
    fail_open=true;assert(!lv_aic_spi_ge2d_create(5,4,7,4,512));fail_open=false;
    assert(allocs==frees && opened==closed);
    uint8_t pixels[64],output[58],expected[56];
    for(unsigned i=0;i<sizeof(pixels);i++) pixels[i]=(uint8_t)(i+1);
    lv_aic_spi_rgb565_frame_t src={.data=pixels,.width=5,.height=4,.stride=16,.capacity=58};
    active=lv_aic_spi_ge2d_create(5,4,7,4,512);assert(active);
    for(unsigned deg=0;deg<360;deg+=90) for(unsigned swap=0;swap<2;swap++) {
        memset(output,0xa5,sizeof(output));stage=0;
        assert(lv_aic_spi_ge2d_convert(active,&src,output+1,56,deg,swap)==LV_AIC_SPI_OK);
        assert(stage==6 && output[0]==0xa5 && output[57]==0xa5);
        assert(lv_aic_spi_pack_rgb565(&src,expected,sizeof(expected),7,4,deg,swap));
        assert(!memcmp(expected,output+1,56));
        assert((void *)(uintptr_t)command.src_buf.phy_addr[0]!=pixels);
        assert((void *)(uintptr_t)command.dst_buf.phy_addr[0]!=output+1);
    }
    unsigned before=calls;memset(output,0xa5,sizeof(output));
    src.capacity=57;assert(lv_aic_spi_ge2d_convert(active,&src,output,58,0,false)==LV_AIC_SPI_INVALID);
    src.capacity=58;src.stride=SIZE_MAX;
    assert(lv_aic_spi_ge2d_convert(active,&src,output,58,0,false)==LV_AIC_SPI_INVALID);
    src.stride=16;src.height=3;
    assert(lv_aic_spi_ge2d_convert(active,&src,output,58,0,false)==LV_AIC_SPI_INVALID);
    src.height=4;
    assert(lv_aic_spi_ge2d_convert(active,&src,output,55,0,false)==LV_AIC_SPI_INVALID);
    assert(lv_aic_spi_ge2d_convert(active,&src,output,58,45,false)==LV_AIC_SPI_INVALID);
    assert(lv_aic_spi_ge2d_convert(active,&src,pixels,58,0,false)==LV_AIC_SPI_INVALID);
    assert(lv_aic_spi_ge2d_convert(active,&src,(void *)(uintptr_t)command.dst_buf.phy_addr[0],58,0,false)==LV_AIC_SPI_INVALID);
    assert(calls==before);
    for(unsigned i=0;i<sizeof(output);i++) assert(output[i]==0xa5);
    assert(lv_aic_spi_ge2d_close(active)==LV_AIC_SPI_OK);active=NULL;
    assert(allocs==frees && opened==closed);
    for(fail_at=1;fail_at<=3;fail_at++) {
        active=lv_aic_spi_ge2d_create(5,4,7,4,512);assert(active);
        memset(pixels,0x31,sizeof(pixels));memset(output,0xa5,sizeof(output));stage=0;
        assert(lv_aic_spi_ge2d_convert(active,&src,output,58,90,true)==LV_AIC_SPI_FAULT);
        assert(stage==(unsigned)fail_at+2);
        memset(pixels,0x72,sizeof(pixels)); /* Producer may safely release/reuse source. */
        assert(*(uint8_t *)(uintptr_t)command.src_buf.phy_addr[0]==0x31);
        for(unsigned i=0;i<sizeof(output);i++) assert(output[i]==0xa5);
        before=calls;unsigned free_before=frees,close_before=closed;
        assert(lv_aic_spi_ge2d_convert(active,&src,output,58,0,false)==LV_AIC_SPI_FAULT);
        assert(lv_aic_spi_ge2d_close(active)==LV_AIC_SPI_FAULT);
        assert(calls==before && frees==free_before && closed==close_before);
    }
    /* Retain six allocations and three clients, matching reboot-only recovery. */
    assert(allocs-frees==6 && opened-closed==3);
    return 0;
}
