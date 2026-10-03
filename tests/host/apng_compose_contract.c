/* SPDX-License-Identifier: Apache-2.0 */
#include "lv_aic_apng_compose.h"
#include <assert.h>
#include <string.h>
int main(void)
{
    uint8_t pixels[40],scratch[32],snapshot[40];memset(pixels,0xa5,sizeof(pixels));memset(scratch,0x5a,sizeof(scratch));
    lv_aic_apng_canvas_t c={0};
    assert(!lv_aic_apng_canvas_init(&c,3,2,pixels+4,16,28,scratch+4,23));
    assert(pixels[4]==0xa5 && !c.pixels);
    assert(!lv_aic_apng_canvas_init(&c,3,2,pixels+4,16,28,pixels+5,24));
    assert(lv_aic_apng_canvas_init(&c,3,2,pixels+4,16,28,scratch+4,24));
    uint8_t red[24]={255,0,0,255,255,0,0,255,255,0,0,255,255,0,0,255,255,0,0,255,255,0,0,255};
    lv_aic_apng_frame_t f={.width=3,.height=2};
    assert(lv_aic_apng_compose(&c,&f,red,12,sizeof(red)));
    uint8_t blue[4]={0,0,255,128};f=(lv_aic_apng_frame_t){.width=1,.height=1,.x=1,.blend=1,.dispose=2};
    assert(lv_aic_apng_compose(&c,&f,blue,4,4));
    assert(!memcmp(pixels+8,(uint8_t[]){127,0,128,255},4));
    uint8_t green[4]={0,255,0,255};f=(lv_aic_apng_frame_t){.width=1,.height=1,.y=1,.dispose=1};
    assert(lv_aic_apng_compose(&c,&f,green,4,4));
    assert(!memcmp(pixels+8,red,4)); /* PREVIOUS restores pre-blend pixels. */
    uint8_t clear[4]={23,17,9,0};f=(lv_aic_apng_frame_t){.width=1,.height=1,.x=2,.blend=1};
    assert(lv_aic_apng_compose(&c,&f,clear,4,4));
    assert(!memcmp(pixels+20,(uint8_t[]){0,0,0,0},4)); /* BACKGROUND is transparent. */
    assert(!memcmp(pixels+12,red,4)); /* Transparent OVER is a no-op. */
    f.blend=0;assert(lv_aic_apng_compose(&c,&f,clear,4,4));assert(!memcmp(pixels+12,clear,4));
    memcpy(snapshot,pixels,sizeof(pixels));f.x=3;
    assert(!lv_aic_apng_compose(&c,&f,green,4,4) && !memcmp(pixels,snapshot,sizeof(pixels)));
    f.x=0;assert(!lv_aic_apng_compose(&c,&f,pixels+4,4,4));
    assert(!lv_aic_apng_compose(&c,&f,green,4,3));
    lv_aic_apng_canvas_reset(&c);f=(lv_aic_apng_frame_t){.width=1,.height=1,.dispose=2};
    assert(lv_aic_apng_compose(&c,&f,green,4,4));f.x=1;f.dispose=0;
    assert(lv_aic_apng_compose(&c,&f,green,4,4));assert(!memcmp(pixels+4,(uint8_t[]){0,0,0,0},4));
    /* Straight-alpha arithmetic and transparent destination. */
    lv_aic_apng_canvas_reset(&c);f.x=0;f.blend=1;
    assert(lv_aic_apng_compose(&c,&f,blue,4,4));assert(!memcmp(pixels+4,blue,4));
    uint8_t halfred[4]={255,0,0,128};assert(lv_aic_apng_compose(&c,&f,halfred,4,4));
    assert(!memcmp(pixels+4,(uint8_t[]){170,0,85,192},4));
    for(unsigned i=0;i<4;i++) assert(pixels[i]==0xa5 && pixels[16+i]==0xa5 && pixels[32+i]==0xa5 && scratch[i]==0x5a && scratch[28+i]==0x5a);
    return 0;
}
