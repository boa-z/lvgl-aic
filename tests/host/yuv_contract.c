/* SPDX-License-Identifier: Apache-2.0 */
#include "lv_aic_yuv.h"
#include <assert.h>
#include <math.h>
#include <string.h>

static int rounded(double v)
{
    if (v < 0) return 0;
    if (v > 255) return 255;
    return (int)floor(v + 0.5);
}
static void reference(int y, int u, int v, int space, int rgb[3])
{
    bool limited = space < LV_AIC_YUV_BT601_FULL;
    bool bt709 = space == LV_AIC_YUV_BT709_LIMITED || space == LV_AIC_YUV_BT709_FULL;
    double kr = bt709 ? .2126 : .299, kb = bt709 ? .0722 : .114;
    double yy = limited ? (y-16)*255.0/219.0 : y;
    double cb = (u-128) * (limited ? 255.0/224.0 : 1.0);
    double cr = (v-128) * (limited ? 255.0/224.0 : 1.0);
    rgb[2] = rounded(yy + 2*(1-kr)*cr);
    rgb[0] = rounded(yy + 2*(1-kb)*cb);
    rgb[1] = rounded(yy - 2*kb*(1-kb)/(1-kr-kb)*cb - 2*kr*(1-kr)/(1-kr-kb)*cr);
}
static unsigned luma(unsigned x, unsigned y, unsigned seed) { return (x*17+y*29+seed)&255; }
static unsigned uval(unsigned x, unsigned y, unsigned seed) { return (x*81+y*37+seed*59)&255; }
static unsigned vval(unsigned x, unsigned y, unsigned seed) { return (x*13+y*97+seed*31)&255; }

int main(void)
{
    const struct { lv_aic_yuv_format_t cf; unsigned sx, sy, planes; } formats[] = {
        {LV_AIC_YUV_YVYU,2,1,1}, {LV_AIC_YUV_VYUY,2,1,1},
        {LV_AIC_YUV_NV16,2,1,2}, {LV_AIC_YUV_NV61,2,1,2},
        {LV_COLOR_FORMAT_I420,2,2,3}, {LV_COLOR_FORMAT_I422,2,1,3},
        {LV_COLOR_FORMAT_I444,1,1,3}, {LV_COLOR_FORMAT_I400,1,1,1},
        {LV_COLOR_FORMAT_NV12,2,2,2}, {LV_COLOR_FORMAT_NV21,2,2,2},
        {LV_COLOR_FORMAT_YUY2,2,1,1}, {LV_COLOR_FORMAT_UYVY,2,1,1}
    };
    uint8_t planes[3][256], output[256];
    lv_aic_yuv_frame_t frame = {0};
    frame.width = 5; frame.height = 3;
    for (unsigned f = 0; f < sizeof(formats)/sizeof(formats[0]); f++) {
        frame.format = formats[f].cf;
        lv_aic_yuv_layout_t layout;
        assert(lv_aic_yuv_layout(frame.format,5,3,&layout));
        assert(layout.planes == formats[f].planes);
        for (unsigned i = 0; i < 3; i++) frame.planes[i] = (lv_aic_yuv_plane_t){planes[i],24,sizeof(planes[i])};
        for (unsigned seed = 0; seed < 256; seed++) {
            memset(planes, 0xcc, sizeof(planes));
            for (unsigned y = 0; y < 3; y++) {
                for (unsigned x = 0; x < 6; x++) {
                    unsigned yy = luma(x,y,seed), u = uval(x/formats[f].sx,y/formats[f].sy,seed);
                    unsigned v = vval(x/formats[f].sx,y/formats[f].sy,seed);
                    if (frame.format == LV_COLOR_FORMAT_YUY2 || frame.format == LV_COLOR_FORMAT_UYVY ||
                        frame.format == LV_AIC_YUV_YVYU || frame.format == LV_AIC_YUV_VYUY) {
                        uint8_t *p = planes[0] + y*24 + (x/2)*4;
                        if (frame.format == LV_COLOR_FORMAT_YUY2) { p[(x%2)*2]=yy; p[1]=u; p[3]=v; }
                        else if(frame.format == LV_COLOR_FORMAT_UYVY) { p[(x%2)*2+1]=yy; p[0]=u; p[2]=v; }
                        else if(frame.format == LV_AIC_YUV_YVYU) { p[(x%2)*2]=yy; p[1]=v; p[3]=u; }
                        else { p[(x%2)*2+1]=yy; p[0]=v; p[2]=u; }
                    }
                    else {
                        planes[0][y*24+x] = yy;
                        unsigned cx=x/formats[f].sx, cy=y/formats[f].sy;
                        if (formats[f].planes == 3) { planes[1][cy*24+cx]=u; planes[2][cy*24+cx]=v; }
                        if (formats[f].planes == 2) {
                            bool vu = frame.format == LV_COLOR_FORMAT_NV21 || frame.format == LV_AIC_YUV_NV61;
                            planes[1][cy*24+cx*2]=vu?v:u;
                            planes[1][cy*24+cx*2+1]=vu?u:v;
                        }
                    }
                }
            }
            for (int space = 0; space < 4; space++) {
                frame.color_space = space;
                memset(output,0xa5,sizeof(output));
                assert(lv_aic_yuv_to_rgb888(&frame,output,24,sizeof(output)));
                for (unsigned byte = 0; byte < sizeof(output); byte++) {
                    unsigned y = byte/24, x = (byte%24)/3, channel = byte%3;
                    if (y < 3 && x < 5) {
                        unsigned u = uval(x/formats[f].sx,y/formats[f].sy,seed);
                        unsigned v = vval(x/formats[f].sx,y/formats[f].sy,seed);
                        if (frame.format == LV_COLOR_FORMAT_I400) u=v=128;
                        int expected[3];
                        reference(luma(x,y,seed),u,v,space,expected);
                        int error = (int)output[byte]-expected[channel];
                        assert(error >= -1 && error <= 1);
                    }
                    else assert(output[byte] == 0xa5);
                }
            }
        }
        /* Require only the used last-row bytes, not nonexistent trailing pad. */
        for (unsigned i=0; i<layout.planes; i++) {
            frame.planes[i].capacity = (layout.rows[i]-1)*24 + layout.row_bytes[i];
            assert(lv_aic_yuv_validate(&frame));
            frame.planes[i].capacity--;
            memset(output,0xa5,sizeof(output));
            assert(!lv_aic_yuv_to_rgb888(&frame,output,24,sizeof(output)));
            for (unsigned j=0;j<sizeof(output);j++) assert(output[j]==0xa5);
            frame.planes[i].capacity++;
            assert(!lv_aic_yuv_to_rgb888(&frame,planes[i],24,sizeof(planes[i])));
        }
        assert(!lv_aic_yuv_to_rgb888(&frame,output,14,sizeof(output)));
        assert(!lv_aic_yuv_to_rgb888(&frame,output,24,62));
        assert(lv_aic_yuv_to_rgb888(&frame,output,24,63));
    }
    assert(!lv_aic_yuv_validate(NULL));
    frame.color_space = 4; assert(!lv_aic_yuv_validate(&frame)); frame.color_space = 0;
    frame.planes[0].data = (void *)(UINTPTR_MAX-10);
    assert(!lv_aic_yuv_validate(&frame));
    lv_aic_yuv_layout_t layout, saved;
    memset(&layout,0xa5,sizeof(layout)); saved=layout;
    assert(!lv_aic_yuv_layout(LV_COLOR_FORMAT_RGB888,32,32,&layout));
    assert(memcmp(&layout,&saved,sizeof(layout))==0);
    assert(!lv_aic_yuv_layout(LV_COLOR_FORMAT_I420,0,3,&layout));
    assert(!lv_aic_yuv_layout(LV_COLOR_FORMAT_I420,4097,3,&layout));
    assert(!lv_aic_yuv_layout(LV_COLOR_FORMAT_I420,4096,2049,&layout));
    assert(lv_aic_yuv_layout(LV_COLOR_FORMAT_I420,4096,2048,&layout));
    assert(lv_aic_yuv_layout(LV_COLOR_FORMAT_I420,1,1,&layout));
    assert(layout.row_bytes[0]==1 && layout.row_bytes[1]==1 && layout.rows[1]==1);
    assert(lv_aic_yuv_layout(LV_COLOR_FORMAT_YUY2,1,1,&layout) && layout.row_bytes[0]==4);
    return 0;
}
