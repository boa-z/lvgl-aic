/* SPDX-License-Identifier: Apache-2.0 */
#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include "../../tests/manual/lv_aic_native_fill_test.c"
static struct lv_mpp_buf *active;
static unsigned allocs,frees,calls;
static int corruption;
static bool fault;
static unsigned rejections;
bool lv_draw_aic_ge2d_faulted(void) { return fault; }
struct lv_mpp_buf *lv_mpp_image_alloc(int w,int h,enum mpp_pixel_format fmt)
{
    assert(!active && w==16 && (h==16 || h==64));
    active=calloc(1,sizeof(*active));assert(active);
    active->size=64*h;active->data=malloc(active->size);assert(active->data);
    active->buf=(struct mpp_buf){.buf_type=MPP_PHY_ADDR,.format=fmt,.size={w,h},.stride={64},.phy_addr={0x40000000}};
    allocs++;return active;
}
void lv_mpp_image_free(struct lv_mpp_buf *image)
{
    assert(image==active);
    if(fault) return;
    free(active->data);free(active);active=NULL;frees++;
}
static void mock_yuv(struct mpp_buf *buf,unsigned color)
{
    /* Independently tabulated black/white/primary CSC outputs. YUV400 instead
     * mirrors the board-measured raw red-byte fill (2026-10-06). */
    static const unsigned values[4][5][3]={
        {{16,128,128},{235,128,128},{82,90,240},{144,54,34},{41,240,110}},
        {{16,128,128},{235,127,128},{63,102,240},{172,41,26},{32,240,118}},
        {{0,128,128},{255,130,130},{77,86,255},{149,44,22},{29,255,108}},
        {{0,128,128},{254,130,130},{54,100,255},{182,30,13},{18,255,117}}
    };
    unsigned index=color==0xff000000?0:color==0xffffffff?1:color==0xffff0000?2:color==0xff00ff00?3:4;
    const unsigned *v=values[MPP_BUF_COLOR_SPACE_GET(buf->flags)][index];
    enum mpp_pixel_format f=buf->format;
    assert(buf->size.width==16 && buf->size.height==16 && buf->stride[0]==64);
    assert(buf->crop_en && buf->crop.x==4 && buf->crop.y==4 && buf->crop.width==8 && buf->crop.height==8);
    if(f==MPP_FMT_YUV420P || f==MPP_FMT_YUV422P || f==MPP_FMT_YUV444P ||
       f==MPP_FMT_NV12 || f==MPP_FMT_NV21 || f==MPP_FMT_NV16 || f==MPP_FMT_NV61) {
        assert(buf->phy_addr[1]==buf->phy_addr[0]+1024 && buf->stride[1]==64);
        if(f==MPP_FMT_YUV420P || f==MPP_FMT_YUV422P || f==MPP_FMT_YUV444P)
            assert(buf->phy_addr[2]==buf->phy_addr[0]+2048 && buf->stride[2]==64);
    }
    if(f==MPP_FMT_YUYV || f==MPP_FMT_YVYU || f==MPP_FMT_UYVY || f==MPP_FMT_VYUY) {
        for(unsigned y=4;y<12;y++) for(unsigned x=4;x<12;x+=2) {
            uint8_t *p=active->data+y*64+x*2;
            if(f==MPP_FMT_YUYV) { p[0]=v[0];p[1]=v[1];p[2]=v[0];p[3]=v[2]; }
            if(f==MPP_FMT_YVYU) { p[0]=v[0];p[1]=v[2];p[2]=v[0];p[3]=v[1]; }
            if(f==MPP_FMT_UYVY) { p[0]=v[1];p[1]=v[0];p[2]=v[2];p[3]=v[0]; }
            if(f==MPP_FMT_VYUY) { p[0]=v[2];p[1]=v[0];p[2]=v[1];p[3]=v[0]; }
        }
    }
    else {
        unsigned luma=f==MPP_FMT_YUV400?(color>>16)&255:v[0];
        for(unsigned y=4;y<12;y++) memset(active->data+y*64+4,luma,8);
        unsigned sy=f==MPP_FMT_YUV420P || f==MPP_FMT_NV12 || f==MPP_FMT_NV21?2:1;
        if(f==MPP_FMT_YUV420P || f==MPP_FMT_YUV422P || f==MPP_FMT_YUV444P) {
            unsigned sx=f==MPP_FMT_YUV444P?1:2;
            for(unsigned c=1;c<3;c++) for(unsigned y=4/sy;y<12/sy;y++)
                memset(active->data+c*1024+y*64+4/sx,v[c],8/sx);
        }
        else if(f!=MPP_FMT_YUV400) {
            unsigned vu=f==MPP_FMT_NV21 || f==MPP_FMT_NV61;
            for(unsigned y=4/sy;y<12/sy;y++) for(unsigned x=4;x<12;x+=2) {
                active->data[1024+y*64+x]=v[1+vu];
                active->data[1024+y*64+x+1]=v[2-vu];
            }
        }
    }
    if(corruption==2) active->data[4*64+8]=0;
    if(corruption==3) active->data[active->size-1]=0;
    if(corruption==5) active->data[1024+2*64+4]=0; /* Chroma corruption. */
}
static void mock_yuv_gradient(struct mpp_buf *buf,int direction,unsigned first,unsigned last)
{
    /* Floating reference independent of the board probe's integer accumulator.
     * YUV400 mirrors the measured raw path: the ramp interpolates the red
     * byte, not a CSC2 luma (2026-10-06). */
    static const double matrix[4][12]={
        {.2578125,.50390625,.09765625,16,-.1484375,-.2890625,.4375,128,.4375,-.3671875,-.0703125,128},
        {.18359375,.61328125,.0625,16,-.1015625,-.33984375,.4375,128,.4375,-.3984375,-.0390625,128},
        {.30078125,.5859375,.11328125,0,-.1640625,-.328125,.5,128,.5,-.4140625,-.078125,128},
        {.2109375,.71484375,.0703125,0,-.109375,-.3828125,.5,128,.5,-.44921875,-.04296875,128}};
    unsigned space=MPP_BUF_COLOR_SPACE_GET(buf->flags),planes=buf->format==MPP_FMT_YUV444P?3:1;
    bool raw_r=buf->format==MPP_FMT_YUV400;
    for(unsigned c=0;c<planes;c++) for(unsigned y=4;y<12;y++) for(unsigned x=4;x<12;x++) {
        double t=(direction==GE_H_LINEAR_GRADIENT?x-4:y-4)/7.0;
        double value;
        if(raw_r)
            value=((first>>16)&255)*(1-t)+((last>>16)&255)*t;
        else {
            value=matrix[space][4*c+3];
            for(unsigned rgb=0;rgb<3;rgb++) {
                unsigned shift=16-rgb*8;
                value+=matrix[space][4*c+rgb]*(((first>>shift)&255)*(1-t)+((last>>shift)&255)*t);
            }
        }
        if(value<0) value=0;
        if(value>255) value=255;
        active->data[c*1024+y*64+x]=(uint8_t)(value+.5);
    }
    if(corruption==2) active->data[4*64+4]=0;
    if(corruption==3) active->data[active->size-1]=0;
    if(corruption==5) active->data[1024+4*64+4]=0; /* Unused-plane corruption. */
}
int lv_ge_fill(struct mpp_buf *buf,enum ge_fillrect_type type,unsigned start,unsigned end,int blend)
{
    if(fault) return LV_RESULT_INVALID;
#if defined(AIC_GE_DRV_V11)
    /* Mirror lv_ge_fill's GE v1.1 admission: RGB and YUV400 only. Rejected
     * layouts never reach the engine, so they are not submissions. */
    if(!((buf->format>=MPP_FMT_ARGB_8888 && buf->format<=MPP_FMT_BGRA_4444) ||
         buf->format==MPP_FMT_YUV400)) { rejections++;return LV_RESULT_INVALID; }
#endif
    calls++;
    if(corruption==4) { fault=true;return LV_RESULT_INVALID; }
    if(corruption==1) return LV_RESULT_OK; /* Engine no-op must not pass. */
    if(buf->format>=MPP_FMT_YUV420P) {
        if(type==GE_NO_GRADIENT) mock_yuv(buf,start);
        else { assert(!blend);mock_yuv_gradient(buf,type,start,end); }
        return LV_RESULT_OK;
    }
    unsigned bpp=buf->format==MPP_FMT_ARGB_8888?4:buf->format==MPP_FMT_RGB_888?3:2;
    for(unsigned y=4;y<12;y++) for(unsigned x=4;x<12;x++) {
        unsigned step=type==GE_H_LINEAR_GRADIENT?x-4:y-4;
        uint8_t *p=active->data+y*64+x*bpp;
        unsigned bg[3]={p[2],p[1],p[0]},rgb[3];
        if(bpp==2) { unsigned v=p[0]|((unsigned)p[1]<<8);bg[0]=((v>>11)&31)*255/31;bg[1]=((v>>5)&63)*255/63;bg[2]=(v&31)*255/31; }
        unsigned alpha=((start>>24)*(7-step)+(end>>24)*step)/7;
        if(corruption==6) alpha=128;
        for(unsigned c=0;c<3;c++) {
            unsigned shift=16-8*c;
            rgb[c]=(((start>>shift)&255)*(7-step)+((end>>shift)&255)*step)/7;
            if(blend) rgb[c]=(rgb[c]*alpha+bg[c]*(255-alpha)+127)/255;
        }
        if(corruption==2) rgb[1]=0; /* Detect lost green command step. */
        if(bpp==2) { unsigned v=((rgb[0]>>3)<<11)|((rgb[1]>>2)<<5)|(rgb[2]>>3);p[0]=v;p[1]=v>>8; }
        else {
            p[0]=rgb[2];p[1]=rgb[1];p[2]=rgb[0];
            /* The blend unit also writes its straight-alpha OVER result into
             * the alpha channel: A_out=(A*A+255*(255-A)+127)/255 (measured,
             * board-2026-10-06-native-alpha-probe). Corruption 7 writes the
             * raw source alpha instead, which must fail the hard check. */
            if(bpp==4)
                p[3]=!blend?255:(corruption==7?(uint8_t)alpha:
                     (uint8_t)((alpha*alpha+255U*(255U-alpha)+127U)/255U));
        }
    }
    if(corruption==3) active->data[0]=0; /* Crop guard. */
    return LV_RESULT_OK;
}
int main(void)
{
    assert(lv_aic_native_fill_test_run()==0 && calls==72 && rejections==236 && allocs==frees);
    corruption=6;
    assert(gradient_probe(MPP_FMT_ARGB_8888,GE_H_LINEAR_GRADIENT,1,0,1)<0 && allocs==frees && !active);
    /* 7: the engine writes the raw source alpha instead of its blend-unit
     * destination-alpha result. The probe hard-fails the first blended ARGB
     * case and frees every allocation. */
    unsigned native_before=calls;
    corruption=7;
    assert(lv_aic_native_fill_test_run()<0 && calls==native_before+3 && allocs==frees && !active);
    for(corruption=1;corruption<=3;corruption++) {
        assert(lv_aic_native_fill_test_run()<0 && allocs==frees && !active);
    }
    /* Rejected YUV layouts never reach the engine: the rejection counter
     * advances while submissions, cache and buffer contents do not - even
     * with fault injection armed, because admission precedes submission. */
    unsigned reject_before=rejections;
    corruption=4;
    assert(yuv_probe(MPP_FMT_NV12,0,2)==0 && rejections==reject_before+1 && !fault &&
           allocs==frees && !active);
    corruption=0;
    for(corruption=1;corruption<=5;corruption++) {
        if(corruption==4) continue;
        assert(yuv_probe(MPP_FMT_YUV400,0,2)<0 && allocs==frees && !active);
    }
    for(corruption=1;corruption<=5;corruption++) {
        if(corruption==4) continue;
        assert(yuv_gradient_probe(MPP_FMT_YUV400,2,GE_H_LINEAR_GRADIENT,1)<0 && allocs==frees && !active);
    }
    corruption=4;
    assert(yuv_gradient_probe(MPP_FMT_YUV400,0,GE_V_LINEAR_GRADIENT,0)<0 && fault && active);
    fault=false;lv_mpp_image_free(active);
    corruption=4;
    assert(lv_aic_native_fill_test_run()<0 && fault && active && allocs==frees+1);
    unsigned before=calls;assert(lv_aic_native_fill_test_run()<0 && calls==before);
    fault=false;lv_mpp_image_free(active); /* Mock has no DMA. */
    assert(allocs==frees);return 0;
}
