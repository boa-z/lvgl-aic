/* SPDX-License-Identifier: Apache-2.0 */
#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include "../../tests/manual/lv_aic_native_fill_test.c"
static struct lv_mpp_buf *active;
static unsigned allocs,frees,calls;
static int corruption;
static bool fault;
bool lv_draw_aic_ge2d_faulted(void) { return fault; }
struct lv_mpp_buf *lv_mpp_image_alloc(int w,int h,enum mpp_pixel_format fmt)
{
    assert(!active && w==16 && h==16);
    active=calloc(1,sizeof(*active));assert(active);
    active->size=1024;active->data=malloc(1024);assert(active->data);
    active->buf=(struct mpp_buf){.buf_type=MPP_PHY_ADDR,.format=fmt,.size={w,h},.stride={64}};
    allocs++;return active;
}
void lv_mpp_image_free(struct lv_mpp_buf *image)
{
    assert(image==active);
    if(fault) return;
    free(active->data);free(active);active=NULL;frees++;
}
int lv_ge_fill(struct mpp_buf *buf,enum ge_fillrect_type type,unsigned start,unsigned end,int blend)
{
    calls++;
    if(corruption==4) { fault=true;return LV_RESULT_INVALID; }
    if(corruption==1) return LV_RESULT_OK; /* Engine no-op must not pass. */
    unsigned bpp=buf->format==MPP_FMT_ARGB_8888?4:buf->format==MPP_FMT_RGB_888?3:2;
    for(unsigned y=4;y<12;y++) for(unsigned x=4;x<12;x++) {
        unsigned step=type==GE_H_LINEAR_GRADIENT?x-4:y-4;
        uint8_t *p=active->data+y*64+x*bpp;
        unsigned bg[3]={p[2],p[1],p[0]},rgb[3];
        if(bpp==2) { unsigned v=p[0]|((unsigned)p[1]<<8);bg[0]=((v>>11)&31)*255/31;bg[1]=((v>>5)&63)*255/63;bg[2]=(v&31)*255/31; }
        for(unsigned c=0;c<3;c++) {
            unsigned shift=16-8*c;
            rgb[c]=(((start>>shift)&255)*(7-step)+((end>>shift)&255)*step)/7;
            if(blend) rgb[c]=(rgb[c]*128+bg[c]*127+127)/255;
        }
        if(corruption==2) rgb[1]=0; /* Detect lost green command step. */
        if(bpp==2) { unsigned v=((rgb[0]>>3)<<11)|((rgb[1]>>2)<<5)|(rgb[2]>>3);p[0]=v;p[1]=v>>8; }
        else { p[0]=rgb[2];p[1]=rgb[1];p[2]=rgb[0];if(bpp==4) p[3]=255; }
    }
    if(corruption==3) active->data[0]=0; /* Crop guard. */
    return LV_RESULT_OK;
}
int main(void)
{
    assert(lv_aic_native_fill_test_run()==0 && calls==24 && allocs==frees);
    for(corruption=1;corruption<=3;corruption++) {
        assert(lv_aic_native_fill_test_run()<0 && allocs==frees && !active);
    }
    corruption=4;
    assert(lv_aic_native_fill_test_run()<0 && fault && active && allocs==frees+1);
    unsigned before=calls;assert(lv_aic_native_fill_test_run()<0 && calls==before);
    fault=false;lv_mpp_image_free(active); /* Mock has no DMA. */
    assert(allocs==frees);return 0;
}
