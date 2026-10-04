/* SPDX-License-Identifier: Apache-2.0 */
#include "lv_aic_spi_ge2d.h"
#include "lv_aic_spi_session.h"
#include <rtdevice.h>
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
static uint8_t *tx,*extra_tx;
static const uint8_t *active_tx;
static unsigned overlap_calls;
static unsigned submissions,waits,status;
static bool spi_fail;
static uint8_t submitted[56];
static void present(void) {}
int rt_spi_wait_completion(struct rt_spi_device *d)
{ assert(d && !memcmp(active_tx,submitted,56));waits++;status=0;return spi_fail?-1:0; }
rt_uint32_t rt_spi_get_transfer_status(struct rt_spi_device *d) { assert(d);return status; }
int rt_spi_nonblock_set(struct rt_spi_device *d,unsigned mode) { assert(d && mode==1);return 0; }
size_t rt_qspi_transfer_message(struct rt_qspi_device *d,struct rt_qspi_message *m)
{
    assert(d && (m->parent.send_buf==tx || m->parent.send_buf==extra_tx) && m->parent.length==56 && !status);
    active_tx=m->parent.send_buf;memcpy(submitted,active_tx,56);submissions++;status=1;return 56;
}
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
{
    if((void *)p==tx || (void *)p==extra_tx) { assert(bytes==64 && status==0);return; }
    assert(p && bytes && stage++==0);
}
void aicos_dcache_clean_invalid_range(unsigned long *p,unsigned long bytes)
{ assert(p && bytes && stage++==1); }
void aicos_dcache_invalid_range(unsigned long *p,unsigned long bytes)
{ assert(p && bytes && stage++==5); }
int mpp_ge_bitblt(struct mpp_ge *g,struct ge_bitblt *b)
{
    assert(g && stage++==2);calls++;command=*b;
    if(status) {
        overlap_calls++;assert(active_tx && !memcmp(active_tx,submitted,56));
        assert((uintptr_t)active_tx!=b->src_buf.phy_addr[0] &&
               (uintptr_t)active_tx!=b->dst_buf.phy_addr[0]);
    }
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
    uint8_t large[1024]={0},small[1024];
    lv_aic_spi_rgb565_frame_t geometry={.data=large,.capacity=sizeof(large),.stride=130,.width=65,.height=4};
    active=lv_aic_spi_ge2d_create(65,4,4,4,4096);assert(active);before=calls;
    assert(lv_aic_spi_ge2d_convert(active,&geometry,small,sizeof(small),0,false)==LV_AIC_SPI_INVALID);
    assert(calls==before);geometry.width=64;stage=0;
    assert(lv_aic_spi_ge2d_convert(active,&geometry,small,sizeof(small),0,false)==LV_AIC_SPI_OK);
    assert(lv_aic_spi_ge2d_close(active)==LV_AIC_SPI_OK);
    for(unsigned rotated=0;rotated<2;rotated++) {
        active=lv_aic_spi_ge2d_create(64,4,rotated?4:64,rotated?64:4,8192);assert(active);
        geometry.width=63;before=calls;
        assert(lv_aic_spi_ge2d_convert(active,&geometry,small,sizeof(small),rotated?90:0,false)==LV_AIC_SPI_INVALID);
        assert(calls==before);geometry.width=64;stage=0;
        assert(lv_aic_spi_ge2d_convert(active,&geometry,small,sizeof(small),rotated?90:0,false)==LV_AIC_SPI_OK);
        assert(lv_aic_spi_ge2d_close(active)==LV_AIC_SPI_OK);
    }
    active=NULL;assert(allocs==frees && opened==closed);
    /* Real session -> transfer -> GE converter -> SDK SPI bridge. */
    tx=arena+500000;tx=(uint8_t *)(((uintptr_t)tx+63)&~(uintptr_t)63);
    struct rt_spi_ops ops={present,present,present,present,present};
    struct rt_spi_bus bus={&ops};struct rt_qspi_device device={{&bus}};
    lv_aic_spi_session_config_t config={.device=&device,.tx=tx,.capacity=64,
        .width=7,.height=4,.data_lines=1,.swap_bytes=true};
    lv_aic_spi_session_t *session=lv_aic_spi_session_open(&config);assert(session);
    assert(!lv_aic_spi_session_enable_ge2d(session,5,4,511));
    assert(lv_aic_spi_session_enable_ge2d(session,5,4,512));
    assert(!lv_aic_spi_session_enable_ge2d(session,5,4,512));
    stage=0;before=calls;
    assert(lv_aic_spi_session_submit(session,&src,90)==LV_AIC_SPI_OK);
    assert(calls==before+1 && submissions==1 && stage==6);
    assert(lv_aic_spi_pack_rgb565(&src,expected,sizeof(expected),7,4,90,true));
    assert(!memcmp(tx,expected,56));
    src.height=3;before=calls; /* Too small to scale: CPU before hardware commands. */
    assert(lv_aic_spi_session_submit(session,&src,180)==LV_AIC_SPI_OK);
    assert(calls==before && submissions==2 && waits==1);
    assert(lv_aic_spi_pack_rgb565(&src,expected,sizeof(expected),7,4,180,true));
    assert(!memcmp(tx,expected,56));src.height=4;
    assert(lv_aic_spi_session_close(session)==LV_AIC_SPI_OK);
    assert(waits==2 && allocs==frees && opened==closed);
    session=lv_aic_spi_session_open(&config);assert(session);
    assert(lv_aic_spi_session_submit(session,&src,0)==LV_AIC_SPI_OK);
    assert(!lv_aic_spi_session_enable_ge2d(session,5,4,512)); /* Too late; cleanup allocation. */
    assert(lv_aic_spi_session_close(session)==LV_AIC_SPI_OK);
    assert(allocs==frees && opened==closed);

    /* Real GE conversion must run with previous SPI pixels still outstanding,
     * alternate outputs, then drain before handing the next frame to transport. */
    session=lv_aic_spi_session_open(&config);assert(session);
    assert(lv_aic_spi_session_enable_ge2d(session,5,4,512));
    extra_tx=arena+4096*allocs;
    assert(lv_aic_spi_session_enable_overlap(session,64));
    unsigned overlap_before=overlap_calls,wait_before=waits;
    for(unsigned i=0;i<3;i++) {
        stage=0;
        assert(lv_aic_spi_session_submit(session,&src,i%2?270:90)==LV_AIC_SPI_OK);
        assert(active_tx==(i%2?extra_tx:tx));
        assert(lv_aic_spi_pack_rgb565(&src,expected,sizeof(expected),7,4,i%2?270:90,true));
        assert(!memcmp(active_tx,expected,56));
    }
    assert(overlap_calls==overlap_before+2 && waits==wait_before+2 && status==1);
    assert(lv_aic_spi_session_close(session)==LV_AIC_SPI_OK);
    assert(allocs==frees && opened==closed);extra_tx=NULL;

    /* GE fault suppresses SPI and retains the session bus/tx claim too. */
    session=lv_aic_spi_session_open(&config);assert(session);
    assert(lv_aic_spi_session_enable_ge2d(session,5,4,512));
    stage=0;fail_at=3;before=submissions;unsigned free_mark=frees,close_mark=closed;
    memset(tx,0xa5,64);
    assert(lv_aic_spi_session_submit(session,&src,90)==LV_AIC_SPI_FAULT);
    assert(lv_aic_spi_session_close(session)==LV_AIC_SPI_FAULT);
    assert(lv_aic_spi_session_submit(session,&src,0)==LV_AIC_SPI_FAULT);
    assert(submissions==before && frees==free_mark && closed==close_mark);
    for(unsigned i=0;i<64;i++) assert(tx[i]==0xa5);
    assert(!lv_aic_spi_session_open(&config));
    /* Different bus and tx, successful GE followed by SPI completion fault. */
    struct rt_spi_bus bus2={&ops};struct rt_qspi_device device2={{&bus2}};
    config.device=&device2;tx+=128;config.tx=tx;
    session=lv_aic_spi_session_open(&config);assert(session);
    assert(lv_aic_spi_session_enable_ge2d(session,5,4,512));
    stage=0;fail_at=0;spi_fail=true;free_mark=frees;close_mark=closed;
    assert(lv_aic_spi_session_submit(session,&src,90)==LV_AIC_SPI_OK);
    assert(lv_aic_spi_session_close(session)==LV_AIC_SPI_FAULT);
    assert(frees==free_mark && closed==close_mark && !lv_aic_spi_session_open(&config));
    spi_fail=false;
    /* GE failure while an earlier SPI read is still live retains both tx
     * frames and GE stages, without draining/replaying either engine. */
    struct rt_spi_bus bus3={&ops};struct rt_qspi_device device3={{&bus3}};
    config.device=&device3;tx+=128;config.tx=tx;
    session=lv_aic_spi_session_open(&config);assert(session);
    assert(lv_aic_spi_session_enable_ge2d(session,5,4,512));
    extra_tx=arena+4096*allocs;
    assert(lv_aic_spi_session_enable_overlap(session,64));
    stage=0;
    assert(lv_aic_spi_session_submit(session,&src,90)==LV_AIC_SPI_OK);
    free_mark=frees;close_mark=closed;before=submissions;wait_before=waits;
    stage=0;fail_at=3;
    assert(lv_aic_spi_session_submit(session,&src,270)==LV_AIC_SPI_FAULT);
    assert(status==1 && waits==wait_before && submissions==before);
    assert(!memcmp(active_tx,submitted,56));
    assert(lv_aic_spi_session_close(session)==LV_AIC_SPI_FAULT);
    assert(frees==free_mark && closed==close_mark);
    status=0; /* Subsequent probes model independent standalone GE clients. */
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
    /* Retain thirteen allocations and six clients, matching reboot-only recovery. */
    assert(allocs-frees==13 && opened-closed==6);
    return 0;
}
