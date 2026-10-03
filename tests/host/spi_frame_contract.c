/* SPDX-License-Identifier: Apache-2.0 */
#include "lv_aic_spi_frame.h"
#include <assert.h>
#include <string.h>
int main(void)
{
    const uint8_t source[]={1,11,2,12,3,13,99,99,4,14,5,15,6,16};
    lv_aic_spi_rgb565_frame_t f={source,sizeof(source),8,3,2};
    /* Literal clockwise grids; unequal dimensions reveal transposition errors. */
    const uint8_t grids[4][6]={{1,2,3,4,5,6},{4,1,5,2,6,3},{6,5,4,3,2,1},{3,6,2,5,1,4}};
    uint8_t output[50];
    for(unsigned rotation=0;rotation<4;rotation++) for(unsigned swap=0;swap<2;swap++) {
        unsigned w=rotation%2?2:3,h=rotation%2?3:2;
        memset(output,0xa5,sizeof(output));
        assert(lv_aic_spi_pack_rgb565(&f,output+1,48,w*2,h*2,rotation*90,swap));
        assert(output[0]==0xa5 && output[49]==0xa5);
        for(unsigned y=0;y<h*2;y++) for(unsigned x=0;x<w*2;x++) {
            unsigned value=grids[rotation][(y/2)*w+x/2],i=1+2*(y*w*2+x);
            assert(output[i]==value+(swap?10:0));assert(output[i+1]==value+(swap?0:10));
        }
    }
    assert(lv_aic_spi_pack_rgb565(&f,output,sizeof(output),2,1,0,false));
    assert(output[0]==1 && output[2]==2); /* nearest downsampling */
    for(unsigned bad=0;bad<9;bad++) {
        lv_aic_spi_rgb565_frame_t invalid=f;
        size_t capacity=sizeof(output);unsigned width=3,rotation=0;
        if(bad==0) invalid.capacity--;
        if(bad==1) invalid.stride=5;
        if(bad==2) invalid.stride=SIZE_MAX;
        if(bad==3) invalid.width=4097;
        if(bad==4) invalid.height=0;
        if(bad==5) invalid.data=(const uint8_t *)(UINTPTR_MAX-5);
        if(bad==6) capacity=11;
        if(bad==7) width=0;
        if(bad==8) rotation=45;
        memset(output,0xa5,sizeof(output));
        assert(!lv_aic_spi_pack_rgb565(&invalid,output,capacity,width,2,rotation,false));
        for(unsigned i=0;i<sizeof(output);i++) assert(output[i]==0xa5);
    }
    uint8_t shared[32]={0};f.data=shared;f.capacity=14;
    assert(!lv_aic_spi_pack_rgb565(&f,shared+1,31,3,2,0,false));
    assert(lv_aic_spi_pack_rgb565(&f,shared+14,18,3,2,0,false));
    return 0;
}
