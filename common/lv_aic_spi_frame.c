/* SPDX-License-Identifier: Apache-2.0 */
#include "lv_aic_spi_frame.h"
bool lv_aic_spi_pack_rgb565(const lv_aic_spi_rgb565_frame_t *source,
    uint8_t *output,size_t capacity,uint32_t width,uint32_t height,
    unsigned degrees,bool swap_bytes)
{
    if(!source) return false;
    /* Snapshot metadata before output writes, including callers with descriptor
     * storage adjacent to their destination. Pixel overlap is rejected below. */
    lv_aic_spi_rgb565_frame_t s=*source;
    if(!s.data || !output || !s.width || !s.height || s.width>4096 || s.height>4096 ||
       !width || !height || width>4096 || height>4096 ||
       (degrees!=0 && degrees!=90 && degrees!=180 && degrees!=270) ||
       s.stride<(size_t)s.width*2) return false;
    size_t row_bytes=(size_t)s.width*2;
    if(s.height>1 && s.stride>(SIZE_MAX-row_bytes)/(s.height-1)) return false;
    size_t span=s.stride*(s.height-1)+row_bytes,bytes=(size_t)width*height*2;
    uintptr_t in=(uintptr_t)s.data,out=(uintptr_t)output;
    if(span>s.capacity || bytes>capacity || span>UINTPTR_MAX-in || bytes>UINTPTR_MAX-out ||
       (in<out+bytes && out<in+span)) return false;
    bool rotated=degrees==90 || degrees==270;
    uint32_t rw=rotated?s.height:s.width,rh=rotated?s.width:s.height;
    for(uint32_t y=0;y<height;y++) for(uint32_t x=0;x<width;x++) {
        uint32_t rx=x*rw/width,ry=y*rh/height,sx,sy;
        switch(degrees) {
        case 90: sx=ry;sy=s.height-1-rx;break;
        case 180: sx=s.width-1-rx;sy=s.height-1-ry;break;
        case 270: sx=s.width-1-ry;sy=rx;break;
        default: sx=rx;sy=ry;break;
        }
        const uint8_t *p=s.data+(size_t)sy*s.stride+sx*2;
        size_t offset=((size_t)y*width+x)*2;
        output[offset]=p[swap_bytes?1:0];output[offset+1]=p[swap_bytes?0:1];
    }
    return true;
}
