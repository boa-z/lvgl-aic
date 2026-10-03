/* SPDX-License-Identifier: Apache-2.0 */
#include "lv_aic_apng_decoder.h"
#include "lv_aic_apng_stream.h"
#include <mpp_decoder.h>
#include <frame_allocator.h>
#include <aic_osal.h>
#include <assert.h>
#include <stdlib.h>
#include <string.h>
#ifdef _WIN32
#include <windows.h>
#else
#include <sys/mman.h>
#endif
static int locked,mutex_live,allocated,live,creates,puts,invalidates;
static int fail,corrupt;
static bool put_fail,stream_mode;
static uint64_t wall_us;
static void *pixels;
static size_t pixel_bytes;
aicos_mutex_t aicos_mutex_create(void) { assert(!mutex_live);mutex_live=1;return &mutex_live; }
void aicos_mutex_delete(aicos_mutex_t m) { assert(m==&mutex_live && !locked);mutex_live=0; }
int aicos_mutex_take(aicos_mutex_t m,uint32_t timeout)
{ assert(m==&mutex_live && !locked && timeout==AICOS_WAIT_FOREVER);locked=1;return 0; }
int aicos_mutex_give(aicos_mutex_t m) { assert(m==&mutex_live && locked);locked=0;return 0; }
void *aicos_malloc_align(unsigned type,size_t size,size_t align)
{
    assert(type==MEM_CMA && align==32 && locked && !allocated && size<=65536);
#ifdef _WIN32
    for(uintptr_t p=0x10000000;p<0x70000000 && !pixels;p+=0x10000)
        pixels=VirtualAlloc((void *)p,65536,MEM_COMMIT|MEM_RESERVE,PAGE_READWRITE);
#else
    pixels=mmap(NULL,65536,PROT_READ|PROT_WRITE,MAP_PRIVATE|MAP_ANONYMOUS|MAP_32BIT,-1,0);
    if(pixels==MAP_FAILED) pixels=NULL;
#endif
    assert(pixels && (uintptr_t)pixels<=UINT32_MAX);allocated=1;pixel_bytes=size;return pixels;
}
void aicos_free_align(unsigned type,void *p)
{
    assert(type==MEM_CMA && locked && allocated && p==pixels);
#ifdef _WIN32
    assert(VirtualFree(p,0,MEM_RELEASE));
#else
    assert(!munmap(p,65536));
#endif
    pixels=NULL;allocated=0;
}
void aicos_dcache_clean_invalid_range(unsigned long *p,unsigned long n)
{ assert(locked && p==pixels && n==pixel_bytes); }
void aicos_dcache_invalid_range(unsigned long *p,unsigned long n)
{ assert(locked && p==pixels && n==pixel_bytes);invalidates++; }
struct mpp_decoder { struct frame_allocator *allocator;struct mpp_frame frame;bool held; };
static unsigned char packet_data[256];
struct mpp_decoder *mpp_decoder_create(enum mpp_codec_type type)
{
    assert(type==MPP_CODEC_VIDEO_DECODER_PNG && !live);creates++;
    if(fail==1) return NULL;
    live=1;return calloc(1,sizeof(struct mpp_decoder));
}
int mpp_decoder_control(struct mpp_decoder *d,int cmd,void *p)
{
    assert(cmd==MPP_DEC_INIT_CMD_SET_EXT_FRAME_ALLOCATOR);
    if(fail==2) return -1;
    d->allocator=p;return 0;
}
int mpp_decoder_init(struct mpp_decoder *d,struct decode_config *c)
{
    assert(d->allocator && c->pix_fmt==MPP_FMT_ARGB_8888 && c->packet_count==1 &&
           c->extra_frame_num==0 && c->bitstream_buffer_size==256);
    return fail==3?-1:0;
}
int mpp_decoder_get_packet(struct mpp_decoder *d,struct mpp_packet *p,int n)
{ assert(d && n<=256);p->data=packet_data;p->size=n;return fail==4?-1:0; }
int mpp_decoder_put_packet(struct mpp_decoder *d,struct mpp_packet *p)
{ assert(d && p->len==p->size && p->flag==PACKET_FLAG_EOS);return fail==5?-1:0; }
int mpp_decoder_decode(struct mpp_decoder *d)
{
    if(fail==6) return -1;
    unsigned width=packet_data[19],height=packet_data[23],stride=(width*4+7)&~7U;
    d->frame.buf.size.width=width;d->frame.buf.size.height=height;
    if(d->allocator->ops->alloc_frame_buffer(d->allocator,&d->frame,(int)stride,(int)height,MPP_FMT_ARGB_8888)) return -1;
    unsigned char *p=pixels;
    for(unsigned y=0;y<height;y++) for(unsigned x=0;x<width;x++) {
        p[y*stride+x*4]=10+x;p[y*stride+x*4+1]=20+y;
        p[y*stride+x*4+2]=90+x;p[y*stride+x*4+3]=stream_mode?255:x*100;
    }
    if(stream_mode) wall_us+=7000;
    return 0;
}
int mpp_decoder_get_frame(struct mpp_decoder *d,struct mpp_frame *f)
{
    if(fail==7) return -1;
    *f=d->frame;d->held=true;f->buf.crop_en=1;f->buf.crop=(struct mpp_rect){0,0,d->frame.buf.size.width,d->frame.buf.size.height};
    switch(corrupt) {
    case 1:f->flags=FRAME_FLAG_ERROR;break;
    case 2:f->buf.format=MPP_FMT_ABGR_8888;break;
    case 3:f->buf.size.width=4;break;
    case 4:f->buf.crop.x=1;break;
    case 5:f->buf.stride[0]=8;break; /* Smaller than the visible row. */
    case 6:f->buf.phy_addr[0]+=32;break;
    }
    return 0;
}
int mpp_decoder_put_frame(struct mpp_decoder *d,struct mpp_frame *f)
{ assert(d->held && f);puts++;if(put_fail) return -1;d->held=false;return 0; }
void mpp_decoder_destory(struct mpp_decoder *d)
{
    assert(live && !d->held);
    if(allocated) assert(!d->allocator->ops->free_frame_buffer(d->allocator,&d->frame));
    if(d->allocator) assert(!d->allocator->ops->close_allocator(d->allocator));
    free(d);live=0;
}
static uint8_t png[512];static size_t length;
static void be32(uint8_t *p,uint32_t v) { p[0]=v>>24;p[1]=v>>16;p[2]=v>>8;p[3]=v; }
static void chunk(const char *type,const uint8_t *data,size_t n)
{
    be32(png+length,(uint32_t)n);memcpy(png+length+4,type,4);
    if(n) memcpy(png+length+8,data,n);
    uint32_t crc=~0U;
    for(size_t i=length+4;i<length+8+n;i++) {
        crc^=png[i];for(int bit=0;bit<8;bit++) crc=(crc>>1)^((0U-(crc&1))&0xedb88320U);
    }
    be32(png+length+8+n,~crc);length+=n+12;
}
static void stream_contract(void);
int main(void)
{
    memcpy(png,"\211PNG\r\n\032\n",8);length=8;
    const uint8_t ihdr[13]={0,0,0,3,0,0,0,2,8,6,0,0,0},zlib[1]={1};
    chunk("IHDR",ihdr,13);chunk("IDAT",zlib,1);chunk("IEND",NULL,0);
    /* Mock engine: payload intentionally opaque; real pixels are board work. */
    assert(!lv_aic_apng_decoder_create(0,256));assert(!lv_aic_apng_decoder_create(32,255));
    lv_aic_apng_decoder_t *d=lv_aic_apng_decoder_create(32,256);assert(d);
    uint8_t out[40];
    for(fail=1;fail<=7;fail++) {
        memset(out,0xa5,sizeof(out));
        assert(!lv_aic_apng_decoder_decode(d,png,length,3,2,out,20,sizeof(out)));
        assert(!live && !allocated && out[0]==0xa5);
    }
    fail=0;
    for(corrupt=1;corrupt<=6;corrupt++) {
        memset(out,0xa5,sizeof(out));
        assert(!lv_aic_apng_decoder_decode(d,png,length,3,2,out,20,sizeof(out)));
        assert(!live && !allocated && out[0]==0xa5);
    }
    corrupt=0;
    int before=creates;
    assert(!lv_aic_apng_decoder_decode(d,png,length,3,2,out,11,sizeof(out)));
    assert(!lv_aic_apng_decoder_decode(d,png,length,3,2,out,20,31));
    assert(!lv_aic_apng_decoder_decode(d,png,length,3,2,out,SIZE_MAX,sizeof(out)));
    assert(!lv_aic_apng_decoder_decode(d,png,length,3,2,png,20,sizeof(png)));
    assert(!lv_aic_apng_decoder_decode(d,png,length,4,2,out,20,sizeof(out)));
    png[29]^=1;assert(!lv_aic_apng_decoder_decode(d,png,length,3,2,out,20,sizeof(out)));png[29]^=1;
    assert(creates==before);
    for(int cycle=0;cycle<100;cycle++) {
        memset(out,0xa5,sizeof(out));
        assert(lv_aic_apng_decoder_decode(d,png,length,3,2,out,20,sizeof(out)));
        assert(!live && !allocated);
        for(unsigned y=0;y<2;y++) {
            for(unsigned x=0;x<3;x++) {
                assert(out[y*20+x*4]==90+x && out[y*20+x*4+1]==20+y &&
                       out[y*20+x*4+2]==10+x && out[y*20+x*4+3]==x*100);
            }
            for(unsigned x=12;x<20;x++) assert(out[y*20+x]==0xa5);
        }
    }
    assert(invalidates==100);
    put_fail=true;
    assert(!lv_aic_apng_decoder_decode(d,png,length,3,2,out,20,sizeof(out)));
    before=creates;assert(live && allocated);
    assert(!lv_aic_apng_decoder_decode(d,png,length,3,2,out,20,sizeof(out)) && creates==before);
    assert(!lv_aic_apng_decoder_destroy(d) && live && allocated && mutex_live);
    put_fail=false;assert(lv_aic_apng_decoder_close(d));assert(!live && !allocated);
    assert(lv_aic_apng_decoder_decode(d,png,length,3,2,out,20,sizeof(out)));
    assert(lv_aic_apng_decoder_destroy(d) && !mutex_live);
    d=lv_aic_apng_decoder_create(31,256);assert(d);
    assert(!lv_aic_apng_decoder_decode(d,png,length,3,2,out,20,sizeof(out)) && !live && !allocated);
    assert(lv_aic_apng_decoder_destroy(d));
    assert(lv_aic_apng_decoder_destroy(NULL));
    stream_contract();
    return 0;
}

static uint64_t now_us(void *context) { assert(context==&wall_us);return wall_us; }
static void animation(unsigned plays)
{
    memcpy(png,"\211PNG\r\n\032\n",8);length=8;
    const uint8_t header[13]={0,0,0,3,0,0,0,2,8,6,0,0,0},payload[1]={1};
    chunk("IHDR",header,13);
    uint8_t actl[8]={0};be32(actl,2);be32(actl+4,plays);chunk("acTL",actl,8);
    chunk("IDAT",payload,1); /* Poster is excluded from animation. */
    for(unsigned i=0;i<2;i++) {
        uint8_t control[26]={0},data[5]={0};
        be32(control,i*2);be32(control+4,1);be32(control+8,1);
        be32(control+12,i*2);be32(control+16,i);
        control[21]=1;control[23]=10;control[24]=i?2:0;
        chunk("fcTL",control,26);be32(data,i*2+1);data[4]=1;chunk("fdAT",data,5);
    }
    chunk("IEND",NULL,0);
}
static void stream_contract(void)
{
    lv_aic_apng_stream_config_t c={.limits={512,256,6,2},.cpu_budget=4096,
        .frame_budget=32,.packet_limit=256,.minimum_delay_us=1000,
        .now_us=now_us,.clock_context=&wall_us};
    stream_mode=true;wall_us=0;
    /* Static PNG uses the existing fixture and ends after one publication. */
    lv_aic_apng_stream_t *s=lv_aic_apng_stream_open(png,length,&c);assert(s);
    lv_aic_apng_stream_view_t v;
    assert(lv_aic_apng_stream_tick(s,&v) && v.step.action==LV_AIC_APNG_FRAME && v.sequence==1);
    assert(v.width==3 && v.height==2 && v.stride==12 && v.bytes==24 && v.rgba[3]==255);
    assert(lv_aic_apng_stream_tick(s,&v) && v.step.action==LV_AIC_APNG_ENDED && v.sequence==1);
    assert(lv_aic_apng_stream_restart(s));
    assert(lv_aic_apng_stream_tick(s,&v) && v.sequence==2);
    assert(lv_aic_apng_stream_close(s) && !mutex_live);
    animation(2);wall_us=0;
    c.cpu_budget=1;assert(!lv_aic_apng_stream_open(png,length,&c) && !mutex_live);
    c.cpu_budget=4096;c.packet_limit=255;assert(!lv_aic_apng_stream_open(png,length,&c));
    c.packet_limit=256;c.minimum_delay_us=0;assert(!lv_aic_apng_stream_open(png,length,&c));
    c.minimum_delay_us=1000;s=lv_aic_apng_stream_open(png,length,&c);assert(s);
    memset(png,0,length); /* Stream owns a copy, including compressed data. */
    assert(lv_aic_apng_stream_pause(s,true));assert(lv_aic_apng_stream_rate(s,2,1));
    assert(!lv_aic_apng_stream_rate(s,11,1));
    assert(lv_aic_apng_stream_tick(s,&v) && v.step.action==LV_AIC_APNG_PAUSED && !v.rgba);
    assert(lv_aic_apng_stream_pause(s,false));
    for(unsigned frame=0;frame<4;frame++) {
        assert(lv_aic_apng_stream_tick(s,&v) && v.step.action==LV_AIC_APNG_FRAME);
        assert(v.step.frame_index==frame%2 && v.step.completed_plays==frame/2 && v.sequence==frame+1);
        assert(v.rgba[0]==90 && v.rgba[3]==255);
        assert(v.rgba[23]==(frame%2?255:0)); /* Loop start clears last frame. */
        assert(lv_aic_apng_stream_tick(s,&v) && v.step.action==LV_AIC_APNG_WAIT);
        assert(v.step.wait_us==(frame==0?50000:43000)); /* Decode time is counted. */
        wall_us+=v.step.wait_us;
    }
    assert(lv_aic_apng_stream_tick(s,&v) && v.step.action==LV_AIC_APNG_ENDED &&
           v.step.completed_plays==2 && v.rgba[23]==255);
    assert(lv_aic_apng_stream_pause(s,true));assert(lv_aic_apng_stream_restart(s));wall_us+=1000000;
    assert(lv_aic_apng_stream_tick(s,&v) && v.step.action==LV_AIC_APNG_PAUSED && v.sequence==4);
    assert(lv_aic_apng_stream_pause(s,false));
    assert(lv_aic_apng_stream_tick(s,&v) && v.sequence==5 && !v.rgba[23]);
    assert(lv_aic_apng_stream_tick(s,&v) && v.step.wait_us==50000);
    wall_us--;lv_aic_apng_stream_view_t saved=v;
    assert(!lv_aic_apng_stream_tick(s,&v) && !memcmp(&saved,&v,sizeof(v)));
    assert(!lv_aic_apng_stream_restart(s));assert(lv_aic_apng_stream_close(s));
    animation(0);wall_us=0;s=lv_aic_apng_stream_open(png,length,&c);assert(s);
    for(unsigned frame=0;frame<8;frame++) {
        wall_us+=1000000; /* Deliberately late: never skip disposal-dependent frames. */
        assert(lv_aic_apng_stream_tick(s,&v) && v.step.action==LV_AIC_APNG_FRAME &&
               v.step.frame_index==frame%2 && v.step.completed_plays==frame/2);
    }
    assert(lv_aic_apng_stream_close(s));
    s=lv_aic_apng_stream_open(png,length,&c);assert(s);put_fail=true;
    saved=v;assert(!lv_aic_apng_stream_tick(s,&v) && !memcmp(&saved,&v,sizeof(v)));
    assert(live && allocated && !lv_aic_apng_stream_restart(s));
    assert(!lv_aic_apng_stream_close(s) && live && allocated && mutex_live);
    put_fail=false;assert(lv_aic_apng_stream_close(s) && !live && !allocated && !mutex_live);
    s=lv_aic_apng_stream_open(png,length,&c);assert(s);fail=6;
    assert(!lv_aic_apng_stream_tick(s,&v));fail=0;
    assert(!lv_aic_apng_stream_tick(s,&v));assert(lv_aic_apng_stream_close(s));
    assert(lv_aic_apng_stream_close(NULL));stream_mode=false;
}
