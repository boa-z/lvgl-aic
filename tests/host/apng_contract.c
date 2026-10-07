/* SPDX-License-Identifier: Apache-2.0 */
#include "lv_aic_apng.h"
#include "lv_aic_apng_compose.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static unsigned char file[4096],saved[4096];
static size_t used;
static uint32_t read32(const uint8_t *p) { return (uint32_t)p[0]<<24|(uint32_t)p[1]<<16|(uint32_t)p[2]<<8|p[3]; }
static void put32(uint8_t *p,uint32_t v) { p[0]=v>>24;p[1]=v>>16;p[2]=v>>8;p[3]=v; }
static uint32_t crc(const uint8_t *p,size_t n)
{
    uint32_t v=~0U;
    while(n--) { v^=*p++; for(int i=0;i<8;i++) v=(v>>1)^((0U-(v&1))&0xedb88320U); }
    return ~v;
}
static void add(const char *kind,const void *data,uint32_t n)
{
    assert(used+n+12<=sizeof(file)); put32(file+used,n);memcpy(file+used+4,kind,4);
    if(n) memcpy(file+used+8,data,n);
    put32(file+used+8+n,crc(file+used+4,n+4)); used+=n+12;
}
static size_t find(const char *kind,unsigned occurrence)
{
    for(size_t p=8;p+12<=used;p+=read32(file+p)+12)
        if(!memcmp(file+p+4,kind,4) && occurrence--==0) return p;
    assert(!"missing chunk"); return 0;
}
static void fix_crc(size_t p) { uint32_t n=read32(file+p);put32(file+p+8+n,crc(file+p+4,n+4)); }
static void fctl(uint32_t seq,uint32_t w,uint32_t x)
{
    uint8_t data[26]={0};put32(data,seq);put32(data+4,w);put32(data+8,1);put32(data+12,x);
    data[21]=1; data[23]=10; data[24]=2; data[25]=1; add("fcTL",data,26);
}
static void make(bool animated,bool poster,bool indexed)
{
    static const uint8_t sig[]={137,80,78,71,13,10,26,10};
    static const uint8_t compressed[]={0x78,0x9c,0x63,0xf8,0xcf,0xc0,0xf0,0x1f,0x00,0x05,0x00,0x01,0xff};
    memcpy(file,sig,8);used=8;
    uint8_t header[13]={0};put32(header,2);put32(header+4,1);header[8]=8;header[9]=indexed?3:6;add("IHDR",header,13);
    uint8_t hints[6]={1,1,0,0,1,0}; add("dcTL",hints,6);
    if(animated) { uint8_t actl[8]={0,0,0,2,0,0,0,3};add("acTL",actl,8); }
    if(indexed) { uint8_t palette[6]={255,0,0,0,255,0},alpha[2]={255,128}; add("PLTE",palette,6); add("tRNS",alpha,2); }
    if(animated && !poster) fctl(0,2,0);
    add("IDAT",compressed,5);add("IDAT",compressed+5,sizeof(compressed)-5);
    if(animated) {
        uint32_t seq=poster?0:1;
        if(poster) { fctl(seq++,1,0);uint8_t frame[17];put32(frame,seq++);memcpy(frame+4,compressed,13);add("fdAT",frame,17); }
        fctl(seq++,1,1);uint8_t frame[17];put32(frame,seq++);memcpy(frame+4,compressed,13);add("fdAT",frame,17);
    }
    add("IEND",NULL,0);
}
static const lv_aic_apng_limits_t limits={4*1024*1024,1024*1024,4096*4096,4096};
static void invalid(size_t size)
{
    lv_aic_apng_t out={.width=12345}; assert(!lv_aic_apng_open(file,size,&limits,&out));assert(out.width==12345);
}
static void contract(void)
{
    for(unsigned mode=0;mode<6;mode++) {
        bool animated=mode>=2,poster=mode>=4,indexed=(mode&1)!=0;make(animated,poster,indexed);
        lv_aic_apng_t d;assert(lv_aic_apng_open(file,used,&limits,&d));
        assert(d.width==2 && d.height==1 && d.animated==animated && d.frames==(animated?2:1));
        assert(d.plays==(animated?3:1));
        size_t cursor=0;lv_aic_apng_frame_t f;unsigned count=0;
        while(lv_aic_apng_next(&d,&cursor,&f)) {
            assert(f.width==(animated && (poster || count)?1:2) && f.height==1);
            assert(f.x==(animated && count?1:0));
            if(animated) assert(f.dispose==2 && f.blend==1 && f.delay_num==1 && f.delay_den==10);
            uint8_t out[4096];memset(out,0xa5,sizeof(out));
            assert(!lv_aic_apng_extract(&d,&f,out,f.png_bytes-1) && out[0]==0xa5);
            assert(!lv_aic_apng_extract(&d,&f,file,sizeof(file)));
            assert(lv_aic_apng_extract(&d,&f,out,sizeof(out)) && out[f.png_bytes]==0xa5);
            lv_aic_apng_t png;assert(lv_aic_apng_open(out,f.png_bytes,&limits,&png));
            assert(!png.animated && png.width==f.width && png.height==1);
            for(size_t p=8;p<f.png_bytes;p+=read32(out+p)+12) {
                assert(memcmp(out+p+4,"fcTL",4) && memcmp(out+p+4,"fdAT",4) && memcmp(out+p+4,"dcTL",4));
            }
            count++;
        }
        assert(cursor==used && count==d.frames);
        f.width=77;assert(!lv_aic_apng_next(&d,&cursor,&f) && f.width==77 && cursor==used);
    }
    make(true,false,false);size_t length=used;memcpy(saved,file,used);
    for(size_t n=0;n<length;n++) invalid(n);
    size_t p=find("fcTL",1);file[p+8+23]=0;fix_crc(p);
    lv_aic_apng_t d;assert(lv_aic_apng_open(file,used,&limits,&d));size_t cursor=0;lv_aic_apng_frame_t f;
    assert(lv_aic_apng_next(&d,&cursor,&f) && lv_aic_apng_next(&d,&cursor,&f) && f.delay_den==100);
    memcpy(file,saved,length);file[20]^=1;invalid(length); /* CRC */
    memcpy(file,saved,length);p=find("fdAT",0);put32(file+p+8,88);fix_crc(p);invalid(length);
    memcpy(file,saved,length);p=find("fcTL",1);put32(file+p+8+12,2);fix_crc(p);invalid(length);
    memcpy(file,saved,length);p=find("fcTL",0);put32(file+p+8+4,1);fix_crc(p);invalid(length);
    memcpy(file,saved,length);p=find("fcTL",1);file[p+8+24]=3;fix_crc(p);invalid(length);
    memcpy(file,saved,length);p=find("acTL",0);put32(file+p+8,3);fix_crc(p);invalid(length);
    memcpy(file,saved,length);p=find("dcTL",0);file[p+4]='D';fix_crc(p);invalid(length); /* Unknown critical */
    memcpy(file,saved,length);file[length]=0;invalid(length+1);
    memcpy(file,saved,length);p=find("IDAT",0);put32(file+p,0x7fffffff);invalid(length);
    memcpy(file,saved,length);lv_aic_apng_limits_t bound=limits;bound.frames=1;assert(!lv_aic_apng_open(file,length,&bound,&d));
    bound=limits;bound.canvas_pixels=1;assert(!lv_aic_apng_open(file,length,&bound,&d));
    bound=limits;bound.file_bytes=length-1;assert(!lv_aic_apng_open(file,length,&bound,&d));
    bound=limits;bound.frame_png_bytes=45;assert(!lv_aic_apng_open(file,length,&bound,&d));
    puts("PASS PNG/APNG CRC, sequence, bounds, poster, palette and extraction contracts");
}
int main(int argc,char **argv)
{
    if(argc==1) { contract();return 0; }
    assert(argc>=2 && argc<=4);FILE *fp=fopen(argv[1],"rb");assert(fp);fseek(fp,0,SEEK_END);long n=ftell(fp);assert(n>0);rewind(fp);
    uint8_t *bytes=malloc((size_t)n);assert(bytes && fread(bytes,1,(size_t)n,fp)==(size_t)n);fclose(fp);
    lv_aic_apng_t d;assert(lv_aic_apng_open(bytes,(size_t)n,&limits,&d));
    size_t cursor=0;lv_aic_apng_frame_t f;unsigned count=0;
    lv_aic_apng_canvas_t canvas={0};uint8_t *pixels=NULL,*scratch=NULL;
    size_t canvas_bytes=(size_t)d.width*d.height*4;
    if(argc==4) {
        pixels=malloc(canvas_bytes);scratch=malloc(canvas_bytes);assert(pixels && scratch);
        assert(lv_aic_apng_canvas_init(&canvas,d.width,d.height,pixels,(size_t)d.width*4,canvas_bytes,scratch,canvas_bytes));
    }
    while(lv_aic_apng_next(&d,&cursor,&f)) {
        uint8_t *png=malloc(f.png_bytes);assert(png && lv_aic_apng_extract(&d,&f,png,f.png_bytes));
        if(argc>=3) { char path[1024];snprintf(path,sizeof(path),"%s/%04u.png",argv[2],count);fp=fopen(path,"wb");assert(fp);assert(fwrite(png,1,f.png_bytes,fp)==f.png_bytes);fclose(fp); }
        if(argc==4) {
            char path[1024];size_t raw_bytes=(size_t)f.width*f.height*4;uint8_t *raw=malloc(raw_bytes);assert(raw);
            snprintf(path,sizeof(path),"%s/%04u.rgba",argv[3],count);fp=fopen(path,"rb");assert(fp);
            assert(fread(raw,1,raw_bytes,fp)==raw_bytes);fclose(fp);
            assert(lv_aic_apng_compose(&canvas,&f,raw,(size_t)f.width*4,raw_bytes));free(raw);
            snprintf(path,sizeof(path),"%s/%04u.canvas.rgba",argv[2],count);fp=fopen(path,"wb");assert(fp);
            assert(fwrite(pixels,1,canvas_bytes,fp)==canvas_bytes);fclose(fp);
        }
        free(png);count++;
    }
    assert(count==d.frames);printf("PASS %s: %u frames %ux%u plays=%u\n",argv[1],count,d.width,d.height,d.plays);free(pixels);free(scratch);free(bytes);return 0;
}
