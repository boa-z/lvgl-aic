/* SPDX-License-Identifier: Apache-2.0 */
#include "lv_aic_barcode.h"
#include <assert.h>
#include <string.h>
static unsigned init_calls,types,decodes,copies,count;
static bool init_fail,reenter;
static lv_aic_yuv_frame_t frame;
unsigned Initial_Decoder(void) { init_calls++;return init_fail?0:1; }
void Set_Donfig_Decoder(int type,int tag) { assert(type==(int)types && tag==1);types++; }
int Decoding_Image(unsigned char *pixels,int w,int h)
{
    assert(w==5 && h==3);decodes++;
    for(int y=0;y<h;y++) for(int x=0;x<w;x++) assert(pixels[y*w+x]==(unsigned)(y*20+x));
    if(reenter) {
        uint8_t out[8]={0};size_t n=99;
        assert(lv_aic_barcode_decode(&frame,out,sizeof(out),&n)==LV_AIC_BARCODE_BUSY && !n);
    }
    memset(pixels,0,15); /* The caller's borrowed pixels must remain unchanged. */
    return 0;
}
unsigned GetResultLength(void) { return count; }
int GetDecoderResult(unsigned char *out) { copies++;memset(out,0x5a,count);out[count]=0;return 0; }
int main(void)
{
    lv_init();uint8_t y[24],uv[24],out[18],saved[24];size_t length;
    memset(y,0xa5,sizeof(y));memset(uv,128,sizeof(uv));
    for(unsigned row=0;row<3;row++) for(unsigned x=0;x<5;x++) y[row*8+x]=row*20+x;
    memcpy(saved,y,sizeof(y));
    frame=(lv_aic_yuv_frame_t){.format=LV_AIC_YUV_NV16,.width=5,.height=3,
        .planes={{y,8,sizeof(y)},{uv,8,sizeof(uv)}}};
    init_fail=true;memset(out,0xa5,sizeof(out));
    assert(lv_aic_barcode_decode(&frame,out+1,16,&length)==LV_AIC_BARCODE_INIT_FAILED);
    assert(!length && !decodes && !types);init_fail=false;
    count=16;reenter=true;
    assert(lv_aic_barcode_decode(&frame,out+1,16,&length)==LV_AIC_BARCODE_OK);
    assert(length==16 && copies==1 && types==14 && init_calls==2);
    assert(out[0]==0xa5 && out[17]==0xa5 && !memcmp(y,saved,sizeof(y)));
    for(unsigned i=1;i<17;i++) assert(out[i]==0x5a);
    count=17;memset(out,0xa5,sizeof(out));
    assert(lv_aic_barcode_decode(&frame,out+1,16,&length)==LV_AIC_BARCODE_TOO_LONG && !length);
    count=4097;assert(lv_aic_barcode_decode(&frame,out+1,16,&length)==LV_AIC_BARCODE_TOO_LONG);
    count=0;assert(lv_aic_barcode_decode(&frame,out+1,16,&length)==LV_AIC_BARCODE_EMPTY);
    assert(copies==1 && init_calls==2);
    for(unsigned i=0;i<18;i++) assert(out[i]==0xa5);
    unsigned before=decodes;
    assert(lv_aic_barcode_decode(&frame,y,sizeof(y),&length)==LV_AIC_BARCODE_INVALID);
    frame.planes[0].capacity=1;
    assert(lv_aic_barcode_decode(&frame,out,sizeof(out),&length)==LV_AIC_BARCODE_INVALID);
    frame.planes[0].capacity=sizeof(y);frame.format=LV_COLOR_FORMAT_YUY2;
    assert(lv_aic_barcode_decode(&frame,out,sizeof(out),&length)==LV_AIC_BARCODE_INVALID);
    assert(decodes==before);lv_deinit();return 0;
}
