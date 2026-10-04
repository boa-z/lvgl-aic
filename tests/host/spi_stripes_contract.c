/* SPDX-License-Identifier: Apache-2.0
 * Real converter/session, descriptor-driven bilinear CPU model, not hardware. */
#include "lv_aic_spi_ge2d.h"
#include "lvgl.h"
#include "lv_aic_spi_session.h"
#include <rtdevice.h>
#include <aic_osal.h>
#include <mpp_ge.h>
#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <string.h>
#ifdef _WIN32
#include <windows.h>
#else
#include <sys/mman.h>
#endif
static uint8_t *arena;
static unsigned allocs,frees,closes,events,fail_event,cache,commands,spi_submits;
static unsigned sw,sh,ow,oh,rotation;
static bool seen[16384];
static uint8_t source[32768],result[32770];
static void present(void) {}
int rt_spi_wait_completion(struct rt_spi_device *d) { assert(d);return 0; }
rt_uint32_t rt_spi_get_transfer_status(struct rt_spi_device *d) { assert(d);return 0; }
int rt_spi_nonblock_set(struct rt_spi_device *d,unsigned m) { assert(d && m==1);return 0; }
size_t rt_qspi_transfer_message(struct rt_qspi_device *d,struct rt_qspi_message *m)
{ assert(d && m);spi_submits++;return m->parent.length; }
void *aicos_malloc_align(unsigned int t,size_t n,size_t a)
{ assert(t==MEM_CMA && a==64 && n<=32768 && allocs<120);return arena+32768*allocs++; }
void aicos_free_align(unsigned int t,void *p) { assert(t==MEM_CMA && p);frees++; }
struct mpp_ge *mpp_ge_open(void) { return (void *)(uintptr_t)1; }
void mpp_ge_close(struct mpp_ge *g) { assert(g);closes++; }
enum ge_mode mpp_ge_get_mode(struct mpp_ge *g) { assert(g);return GE_MODE_CMDQ; }
void aicos_dcache_clean_range(unsigned long *p,unsigned long n)
{ assert(p && n && cache++==0); }
void aicos_dcache_clean_invalid_range(unsigned long *p,unsigned long n)
{ assert(p && n && cache++==1 && !events); }
void aicos_dcache_invalid_range(unsigned long *p,unsigned long n)
{ assert(p && n && cache++==2 && events==commands*3); }
static unsigned channel(uint16_t p,unsigned k)
{ return k==0?p&31:k==1?(p>>5)&63:p>>11; }
static uint16_t read(const uint8_t *p,size_t stride,int x,int y,int w,int h)
{
    if(x<0) x=0;
    if(y<0) y=0;
    if(x>=w) x=w-1;
    if(y>=h) y=h-1;
    uint16_t v;memcpy(&v,p+y*stride+x*2,2);return v;
}
static uint16_t filter(const uint8_t *p,size_t stride,double x,double y,int w,int h)
{
    int ix=(int)floor(x),iy=(int)floor(y);double fx=x-ix,fy=y-iy;
    uint16_t a=read(p,stride,ix,iy,w,h),b=read(p,stride,ix+1,iy,w,h);
    uint16_t c=read(p,stride,ix,iy+1,w,h),d=read(p,stride,ix+1,iy+1,w,h),v=0;
    for(unsigned k=0;k<3;k++) {
        unsigned q=(unsigned)lround(channel(a,k)*(1-fx)*(1-fy)+channel(b,k)*fx*(1-fy)+
                                    channel(c,k)*(1-fx)*fy+channel(d,k)*fx*fy);
        v|=q<<(k==0?0:k==1?5:11);
    }
    return v;
}
int mpp_ge_bitblt(struct mpp_ge *g,struct ge_bitblt *b)
{
    assert(g && cache==2);if(++events==fail_event) return -1;
    commands++;
    assert(b->scale_phase.scale_phase_en && b->scale_phase.channel_num==1);
    assert(b->src_buf.size.width==(int)sw+1 && b->src_buf.size.height==(int)sh);
    assert(b->ctrl.flags==rotation && !b->ctrl.alpha_en);
    int w=b->dst_buf.crop.width,h=b->dst_buf.crop.height;
    assert((rotation&1?h:w)<=31);
    const uint8_t *src=(void *)(uintptr_t)b->src_buf.phy_addr[0];
    uint8_t *dst=(void *)(uintptr_t)b->dst_buf.phy_addr[0];
    for(unsigned y=0;y<sh;y++) {
        assert(!memcmp(src+y*b->src_buf.stride[0],source+y*sw*2,sw*2));
        assert(!memcmp(src+y*b->src_buf.stride[0]+sw*2,source+y*sw*2+(sw-1)*2,2));
    }
    for(int y=0;y<h;y++) for(int x=0;x<w;x++) {
        int u=rotation==0?x:rotation==1?y:rotation==2?w-1-x:h-1-y;
        int v=rotation==0?y:rotation==1?w-1-x:rotation==2?h-1-y:x;
        double sx=b->src_buf.crop.x+(b->scale_phase.h_phase_16[0]+(int64_t)u*b->scale_phase.dx_16[0])/65536.0;
        double sy=b->src_buf.crop.y+(b->scale_phase.v_phase_16[0]+(int64_t)v*b->scale_phase.dy_16[0])/65536.0;
        int xx=b->dst_buf.crop.x+x,yy=b->dst_buf.crop.y+y;
        assert(xx>=0 && xx<(int)ow && yy>=0 && yy<(int)oh && !seen[yy*ow+xx]);seen[yy*ow+xx]=true;
        uint16_t value=filter(src,b->src_buf.stride[0],sx,sy,b->src_buf.size.width,b->src_buf.size.height);
        memcpy(dst+yy*b->dst_buf.stride[0]+xx*2,&value,2);
    }
    return 0;
}
int mpp_ge_emit(struct mpp_ge *g) { assert(g);return ++events==fail_event?-1:0; }
int mpp_ge_sync(struct mpp_ge *g) { assert(g);return ++events==fail_event?-1:0; }
static void reset(void)
{ events=commands=cache=0;memset(seen,0,sizeof seen);memset(result,0xa5,sizeof result); }
static lv_aic_spi_rgb565_frame_t fixture(unsigned w,unsigned h)
{
    sw=w;sh=h;
    for(unsigned y=0;y<h;y++) for(unsigned x=0;x<w;x++) {
        uint16_t v=((x*3+y*7)%32)<<11|((x*5+y*3)%64)<<5|(x*7+y*5)%32;
        memcpy(source+(y*w+x)*2,&v,2);
    }
    return (lv_aic_spi_rgb565_frame_t){.data=source,.capacity=sizeof source,.width=w,.height=h,.stride=w*2};
}
int main(void)
{
    lv_init();
#ifdef _WIN32
    arena=VirtualAlloc((void *)(uintptr_t)0x48000000,4194304,MEM_RESERVE|MEM_COMMIT,PAGE_READWRITE);
#else
    arena=mmap((void *)(uintptr_t)0x48000000,4194304,PROT_READ|PROT_WRITE,
        MAP_PRIVATE|MAP_ANONYMOUS|MAP_FIXED_NOREPLACE,-1,0);
#endif
    assert(arena==(void *)(uintptr_t)0x48000000);
    const unsigned widths[][2]={{31,32},{63,64},{62,67},{94,95},{127,128},{4095,4096}};
    unsigned pixels=0;
    for(unsigned pair=0;pair<6;pair++) for(rotation=0;rotation<4;rotation++) {
        if(pair==5 && (rotation&1)) continue;
        lv_aic_spi_rgb565_frame_t frame=fixture(widths[pair][0],pair==5?4:8);
        unsigned scaled_w=widths[pair][1],scaled_h=pair==5?4:rotation&1?12:6;
        ow=rotation&1?scaled_h:scaled_w;oh=rotation&1?scaled_w:scaled_h;
        lv_aic_spi_ge2d_t *g=lv_aic_spi_ge2d_create(sw,sh,ow,oh,65536);assert(g);
        for(unsigned swap=0;swap<2;swap++) {
            reset();assert(lv_aic_spi_ge2d_convert(g,&frame,result+1,ow*oh*2,rotation*90,swap)==LV_AIC_SPI_OK);
            assert(cache==3 && commands==(scaled_w+30)/31 && events==commands*3);
            double dx=(double)((sw*65536)/scaled_w),dy=(double)((sh*65536)/scaled_h);
            double px=floor(dx/2),py=floor(dy/2)-(dy>=65536?32768:0);
            for(unsigned y=0;y<oh;y++) for(unsigned x=0;x<ow;x++) {
                unsigned u=rotation==0?x:rotation==1?y:rotation==2?ow-1-x:oh-1-y;
                unsigned v=rotation==0?y:rotation==1?ow-1-x:rotation==2?oh-1-y:x;
                uint16_t expected=filter(source,sw*2,(px+u*dx)/65536,(py+v*dy)/65536,sw,sh);
                uint8_t lo=result[1+(y*ow+x)*2+(swap?1:0)],hi=result[1+(y*ow+x)*2+(swap?0:1)];
                assert(seen[y*ow+x] && (uint16_t)(lo+256*hi)==expected);pixels++;
            }
            assert(result[0]==0xa5 && result[ow*oh*2+1]==0xa5);
        }
        assert(lv_aic_spi_ge2d_close(g)==LV_AIC_SPI_OK);
    }
    assert(allocs==frees);
    rotation=0;ow=64;oh=8;lv_aic_spi_rgb565_frame_t frame=fixture(63,8);
    unsigned free_mark=frees,close_mark=closes;
    for(fail_event=1;fail_event<=9;fail_event++) {
        lv_aic_spi_ge2d_t *g=lv_aic_spi_ge2d_create(sw,sh,ow,oh,65536);assert(g);reset();
        assert(lv_aic_spi_ge2d_convert(g,&frame,result+1,ow*oh*2,0,false)==LV_AIC_SPI_FAULT);
        assert(events==fail_event && cache==2 && lv_aic_spi_ge2d_close(g)==LV_AIC_SPI_FAULT);
        assert(lv_aic_spi_ge2d_convert(g,&frame,result+1,ow*oh*2,0,false)==LV_AIC_SPI_FAULT);
        for(unsigned i=0;i<sizeof result;i++) assert(result[i]==0xa5);
    }
    assert(frees==free_mark && closes==close_mark);
    struct rt_spi_ops ops={present,present,present,present,present};
    struct rt_spi_bus bus={&ops};struct rt_qspi_device dev={{&bus}};
    lv_aic_spi_session_config_t config={.device=&dev,.tx=arena+4000000,.capacity=1024,
        .width=64,.height=8,.data_lines=1};
    lv_aic_spi_session_t *session=lv_aic_spi_session_open(&config);assert(session);
    assert(lv_aic_spi_session_enable_ge2d(session,63,8,32768));
    reset();fail_event=4;memset(config.tx,0xa5,1024);
    assert(lv_aic_spi_session_submit(session,&frame,0)==LV_AIC_SPI_FAULT);
    assert(commands==1 && events==4 && !spi_submits && frees==free_mark);
    for(unsigned i=0;i<1024;i++) assert(config.tx[i]==0xa5);
    assert(lv_aic_spi_session_close(session)==LV_AIC_SPI_FAULT && !lv_aic_spi_session_open(&config));
    printf("PASS 44 SPI stripe scenes, %u RGB565 pixels, SDK phases/edge replication, nine DMA faults and late-strip transport suppression\n",pixels);
    return 0;
}
