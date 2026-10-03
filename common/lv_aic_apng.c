/* SPDX-License-Identifier: Apache-2.0 */
#include "lv_aic_apng.h"
#include <string.h>
static const uint8_t signature[8]={137,80,78,71,13,10,26,10};
typedef struct { const uint8_t *type,*data; uint32_t length; size_t end; } chunk_t;
static uint32_t be32(const uint8_t *p)
{ return (uint32_t)p[0]<<24|(uint32_t)p[1]<<16|(uint32_t)p[2]<<8|p[3]; }
static uint16_t be16(const uint8_t *p) { return (uint16_t)((uint16_t)p[0]<<8|p[1]); }
static void put32(uint8_t *p,uint32_t v)
{ p[0]=(uint8_t)(v>>24); p[1]=(uint8_t)(v>>16); p[2]=(uint8_t)(v>>8); p[3]=(uint8_t)v; }
static bool kind(const chunk_t *c,const char *name) { return !memcmp(c->type,name,4); }
static uint32_t crc32(const uint8_t *p,size_t n)
{
    uint32_t crc=UINT32_MAX;
    while(n--) {
        crc^=*p++;
        for(unsigned bit=0;bit<8;bit++) crc=(crc>>1)^((crc&1)?UINT32_C(0xedb88320):0);
    }
    return crc^UINT32_MAX;
}
static bool chunk(const lv_aic_apng_t *d,size_t pos,chunk_t *c)
{
    if(pos>d->size || d->size-pos<12) return false;
    uint32_t n=be32(d->data+pos);
    if(n>INT32_MAX || n>d->size-pos-12) return false;
    *c=(chunk_t){d->data+pos+4,d->data+pos+8,n,pos+12+n}; return true;
}
static bool sum(size_t *value,size_t add,size_t limit)
{ if(*value>limit || add>limit-*value) return false; *value+=add; return true; }
static bool ancillary(const chunk_t *c) { return (c->type[0]&32)!=0; }
static bool prefix(const chunk_t *c)
{ return !kind(c,"acTL") && !kind(c,"fcTL") && !kind(c,"dcTL"); }
static bool control(const chunk_t *c,lv_aic_apng_frame_t *f)
{
    if(c->length!=26) return false;
    const uint8_t *p=c->data;
    *f=(lv_aic_apng_frame_t){.width=be32(p+4),.height=be32(p+8),.x=be32(p+12),.y=be32(p+16),
        .delay_num=be16(p+20),.delay_den=be16(p+22),.dispose=p[24],.blend=p[25]};
    if(!f->delay_den) f->delay_den=100;
    return f->dispose<=2 && f->blend<=1;
}
static bool ihdr(const chunk_t *c)
{
    const uint8_t *p=c->data;
    if(c->length!=13 || p[10] || p[11] || p[12]>1) return false;
    switch(p[9]) {
    case 0: return p[8]==1 || p[8]==2 || p[8]==4 || p[8]==8 || p[8]==16;
    case 2: case 4: case 6: return p[8]==8 || p[8]==16;
    case 3: return p[8]==1 || p[8]==2 || p[8]==4 || p[8]==8;
    default: return false;
    }
}
bool lv_aic_apng_next(const lv_aic_apng_t *d,size_t *cursor,lv_aic_apng_frame_t *out)
{
    if(!d || !cursor || !out || *cursor>=d->size) return false;
    size_t pos=*cursor?*cursor:8,bytes=45;
    if(!sum(&bytes,d->prefix_bytes,d->max_frame_png_bytes)) return false;
    lv_aic_apng_frame_t f={.width=d->width,.height=d->height,.delay_den=100};
    bool have_control=false,have_data=false;
    while(pos<d->size) {
        chunk_t c; if(!chunk(d,pos,&c)) return false;
        if((kind(&c,"fcTL") || kind(&c,"IEND")) && have_data) {
            f.png_bytes=bytes; *out=f; *cursor=kind(&c,"IEND")?d->size:pos; return true;
        }
        if(kind(&c,"fcTL")) { if(!control(&c,&f)) return false; have_control=true; }
        if((kind(&c,"IDAT") && (!d->animated || have_control)) || (kind(&c,"fdAT") && have_control)) {
            size_t remove=kind(&c,"fdAT")?4:0;
            if(c.length<remove || !sum(&bytes,(size_t)c.length-remove+12,d->max_frame_png_bytes)) return false;
            if(!have_data) f.data_start=pos;
            f.data_end=c.end; have_data=true;
        }
        pos=c.end;
    }
    return false;
}
bool lv_aic_apng_open(const void *data,size_t size,const lv_aic_apng_limits_t *limits,lv_aic_apng_t *out)
{
    if(!data || !limits || !out || !limits->file_bytes || !limits->frame_png_bytes ||
       !limits->canvas_pixels || !limits->frames || size>limits->file_bytes || size<45 || memcmp(data,signature,8)) return false;
    lv_aic_apng_t d={.data=data,.size=size,.frames=1,.plays=1,.max_frame_png_bytes=limits->frame_png_bytes};
    chunk_t c; if(!chunk(&d,8,&c) || !kind(&c,"IHDR") || !ihdr(&c)) return false;
    d.width=be32(c.data); d.height=be32(c.data+4);
    if(!d.width || !d.height || d.width>4096 || d.height>4096 || (uint64_t)d.width*d.height>limits->canvas_pixels) return false;
    uint8_t color=c.data[9],depth=c.data[8];
    bool idat=false,idat_end=false,active=false,frame_data=false,plte=false,trns=false,ended=false,included_default=false;
    uint32_t controls=0,palette=0; uint64_t sequence=0;
    size_t payload=0,default_payload=0;
    lv_aic_apng_frame_t f={0};
    for(size_t pos=8;pos<size;pos=c.end) {
        if(!chunk(&d,pos,&c) || crc32(c.type,(size_t)c.length+4)!=be32(c.data+c.length)) return false;
        for(unsigned i=0;i<4;i++) if(!((c.type[i]>='A' && c.type[i]<='Z') || (c.type[i]>='a' && c.type[i]<='z'))) return false;
        if(c.type[2]&32) return false;
        if(idat && !kind(&c,"IDAT")) idat_end=true;
        if(kind(&c,"IHDR")) { if(pos!=8) return false; }
        else if(kind(&c,"acTL")) {
            if(d.animated || idat || c.length!=8) return false;
            d.animated=true; d.frames=be32(c.data); d.plays=be32(c.data+4);
            if(!d.frames || d.frames>limits->frames) return false;
        } else if(kind(&c,"fcTL")) {
            if(!d.animated || controls>=d.frames || (active && (!frame_data || !payload)) ||
               !control(&c,&f) || sequence>UINT32_MAX || be32(c.data)!=sequence++) return false;
            if(!f.width || !f.height || f.x>d.width || f.y>d.height || f.width>d.width-f.x || f.height>d.height-f.y) return false;
            if(!idat && (f.width!=d.width || f.height!=d.height || f.x || f.y)) return false;
            controls++; active=true; frame_data=false; payload=0;
        } else if(kind(&c,"IDAT")) {
            if(idat_end || (color==3 && !plte)) return false;
            if(!idat) { d.prefix_end=pos; included_default=active; }
            idat=true;
            if(!sum(&default_payload,c.length,SIZE_MAX)) return false;
            if(active) { frame_data=true; if(!sum(&payload,c.length,SIZE_MAX)) return false; }
        } else if(kind(&c,"fdAT")) {
            if(!d.animated || !idat || !active || c.length<4 || sequence>UINT32_MAX || be32(c.data)!=sequence++) return false;
            /* The IDAT-backed animation frame must end at another fcTL. */
            if(controls==1 && included_default) return false;
            frame_data=true; if(!sum(&payload,c.length-4,SIZE_MAX)) return false;
        } else if(kind(&c,"PLTE")) {
            if(plte || idat || trns || color==0 || color==4 || !c.length || c.length>768 || c.length%3) return false;
            palette=c.length/3; if(color==3 && palette>(1U<<depth)) return false;
            plte=true;
        } else if(kind(&c,"tRNS")) {
            if(trns || idat || (color==0 && c.length!=2) || (color==2 && c.length!=6) ||
               (color==3 && (!plte || !c.length || c.length>palette)) || (color!=0 && color!=2 && color!=3)) return false;
            trns=true;
        } else if(kind(&c,"IEND")) {
            if(c.length || c.end!=size || !idat || !default_payload ||
               (d.animated && (controls!=d.frames || !active || !frame_data || !payload))) return false;
            ended=true;
        } else if(!ancillary(&c)) return false;
        if(pos>8 && !idat && prefix(&c) && !sum(&d.prefix_bytes,(size_t)c.length+12,limits->frame_png_bytes)) return false;
    }
    if(!ended) return false;
    size_t cursor=0; uint32_t count=0;
    while(lv_aic_apng_next(&d,&cursor,&f)) count++;
    if(cursor!=size || count!=d.frames) return false;
    *out=d; return true;
}
static size_t write_chunk(uint8_t *out,const char *type,const uint8_t *data,uint32_t n)
{
    put32(out,n); memcpy(out+4,type,4); if(n) memcpy(out+8,data,n);
    put32(out+8+n,crc32(out+4,(size_t)n+4)); return (size_t)n+12;
}
bool lv_aic_apng_extract(const lv_aic_apng_t *d,const lv_aic_apng_frame_t *f,void *output,size_t capacity)
{
    if(!d || !f || !output || !f->width || !f->height || f->width>d->width || f->height>d->height ||
       f->data_start<d->prefix_end || f->data_end>d->size || f->data_start>=f->data_end || capacity<f->png_bytes) return false;
    uintptr_t src=(uintptr_t)d->data,dst=(uintptr_t)output;
    if(d->size>UINTPTR_MAX-src || capacity>UINTPTR_MAX-dst ||
       (dst<src?src-dst<capacity:dst-src<d->size)) return false;
    size_t bytes=45;
    if(!sum(&bytes,d->prefix_bytes,d->max_frame_png_bytes)) return false;
    for(size_t pos=f->data_start;pos<f->data_end;) {
        chunk_t c; if(!chunk(d,pos,&c) || c.end>f->data_end) return false;
        if(kind(&c,"IDAT") || kind(&c,"fdAT")) {
            size_t skip=kind(&c,"fdAT")?4:0;
            if(c.length<skip || !sum(&bytes,(size_t)c.length-skip+12,d->max_frame_png_bytes)) return false;
        }
        pos=c.end;
    }
    if(bytes!=f->png_bytes || bytes>capacity) return false;
    uint8_t *out=output,header[13]; memcpy(out,signature,8); memcpy(header,d->data+16,13);
    put32(header,f->width); put32(header+4,f->height);
    size_t offset=8+write_chunk(out+8,"IHDR",header,13);
    for(size_t pos=33;pos<d->prefix_end;) {
        chunk_t c; if(!chunk(d,pos,&c)) return false;
        if(prefix(&c)) { memcpy(out+offset,d->data+pos,c.end-pos); offset+=c.end-pos; }
        pos=c.end;
    }
    for(size_t pos=f->data_start;pos<f->data_end;) {
        chunk_t c; if(!chunk(d,pos,&c)) return false;
        if(kind(&c,"IDAT") || kind(&c,"fdAT")) {
            unsigned skip=kind(&c,"fdAT")?4:0;
            offset+=write_chunk(out+offset,"IDAT",c.data+skip,c.length-skip);
        }
        pos=c.end;
    }
    write_chunk(out+offset,"IEND",NULL,0); return true;
}
