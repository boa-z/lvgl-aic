/* SPDX-License-Identifier: Apache-2.0 */
#include "lv_aic_yuv_mpp.h"
#include "lv_aic_pixel_format.h"
#include "lv_aic_mpp_format.h"
#include <assert.h>
#include <string.h>

static void reject(const lv_aic_yuv_frame_t *frame)
{
    struct mpp_buf out, saved;
    memset(&out,0xa5,sizeof(out)); memcpy(&saved,&out,sizeof(out));
    assert(!lv_aic_yuv_to_mpp(frame,0x40000000,&out));
    assert(memcmp(&out,&saved,sizeof(out))==0);
}
static void reject_mpp(const struct mpp_buf *buffer, const size_t capacities[3])
{
    lv_aic_yuv_frame_t out, saved;
    memset(&out, 0xa5, sizeof(out)); memcpy(&saved, &out, sizeof(out));
    assert(!lv_aic_yuv_from_mpp(buffer, capacities, LV_AIC_YUV_BT601_LIMITED, &out));
    assert(memcmp(&out, &saved, sizeof(out)) == 0);
}
int main(void)
{
    const struct { lv_aic_yuv_format_t lv; enum mpp_pixel_format mpp; unsigned planes; } formats[] = {
        {LV_AIC_YUV_NV16,MPP_FMT_NV16,2}, {LV_AIC_YUV_NV61,MPP_FMT_NV61,2},
        {LV_COLOR_FORMAT_I420,MPP_FMT_YUV420P,3}, {LV_COLOR_FORMAT_I422,MPP_FMT_YUV422P,3},
        {LV_COLOR_FORMAT_I444,MPP_FMT_YUV444P,3}, {LV_COLOR_FORMAT_I400,MPP_FMT_YUV400,1},
        {LV_COLOR_FORMAT_NV12,MPP_FMT_NV12,2}, {LV_COLOR_FORMAT_NV21,MPP_FMT_NV21,2},
        {LV_COLOR_FORMAT_YUY2,MPP_FMT_YUYV,1}, {LV_COLOR_FORMAT_UYVY,MPP_FMT_UYVY,1}
    };
    const uint32_t spaces[] = {MPP_COLOR_SPACE_BT601,MPP_COLOR_SPACE_BT709,
                              MPP_COLOR_SPACE_BT601_FULL_RANGE,MPP_COLOR_SPACE_BT709_FULL_RANGE};
    lv_aic_yuv_frame_t frame = {0};
    frame.width = 32; frame.height = 16;
    for (unsigned i=0;i<3;i++)
        frame.planes[i]=(lv_aic_yuv_plane_t){(void *)(uintptr_t)(0x40001000+i*0x1000),96,1536};
    struct mpp_buf out;
    for (unsigned f=0;f<sizeof(formats)/sizeof(formats[0]);f++) {
        frame.format=formats[f].lv;
        for (unsigned s=0;s<4;s++) {
            frame.color_space=s;
            assert(lv_aic_yuv_to_mpp(&frame,0x40000000,&out));
            assert(out.format==formats[f].mpp && out.buf_type==MPP_PHY_ADDR);
            assert(out.size.width==32 && out.size.height==16 && !out.crop_en && out.flags==spaces[s]);
            for (unsigned p=0;p<3;p++) {
                assert(out.phy_addr[p] == (p < formats[f].planes ? 0x40001000+p*0x1000 : 0));
                assert(out.stride[p] == (p < formats[f].planes ? 96U : 0U));
            }
            size_t capacities[3] = {1536, 1536, 1536};
            lv_aic_yuv_frame_t view;
            assert(lv_aic_yuv_from_mpp(&out, capacities, s, &view));
            assert(view.format == frame.format && view.width == 32 && view.height == 16);
            assert(view.color_space == frame.color_space);
            out.crop_en = 1; out.crop = (struct mpp_rect){2, 2, 15, 7};
            assert(lv_aic_yuv_from_mpp(&out, capacities, s, &view));
            assert(view.width == 15 && view.height == 7);
            bool sub_x = frame.format != LV_COLOR_FORMAT_I400 && frame.format != LV_COLOR_FORMAT_I444;
            bool sub_y = frame.format == LV_COLOR_FORMAT_I420 || frame.format == LV_COLOR_FORMAT_NV12 ||
                         frame.format == LV_COLOR_FORMAT_NV21;
            bool packed = frame.format == LV_COLOR_FORMAT_YUY2 || frame.format == LV_COLOR_FORMAT_UYVY;
            for (unsigned p = 0; p < formats[f].planes; p++) {
                size_t offset = (p && sub_y ? 96 : 192) +
                                (packed ? 4 : p && sub_x && formats[f].planes == 3 ? 1 : 2);
                assert((uintptr_t)view.planes[p].data == out.phy_addr[p] + offset);
                assert(view.planes[p].capacity == capacities[p] - offset);
                assert(view.planes[p].stride == 96);
            }
            out.crop.x = 1;
            if (sub_x) reject_mpp(&out, capacities);
            else assert(lv_aic_yuv_from_mpp(&out, capacities, s, &view));
            out.crop.x = 2; out.crop.y = 1;
            if (sub_y) reject_mpp(&out, capacities);
            else assert(lv_aic_yuv_from_mpp(&out, capacities, s, &view));
            out.crop.y = 2; out.crop.width = 31; reject_mpp(&out, capacities);
            out.crop.width = 15; out.crop.height = 15; reject_mpp(&out, capacities);
            out.crop.height = 7; out.crop.x = -1; reject_mpp(&out, capacities);
            out.crop.x = 2; out.crop.width = 0; reject_mpp(&out, capacities);
            out.crop_en = 0;
            capacities[0] = 1; reject_mpp(&out, capacities); capacities[0] = 1536;
            out.phy_addr[0] = 0xfffffff0; reject_mpp(&out, capacities);
            out.phy_addr[0] = 0x40001000;
            out.buf_type = MPP_DMA_BUF_FD; reject_mpp(&out, capacities); out.buf_type = MPP_PHY_ADDR;
            assert(!lv_aic_yuv_from_mpp(&out, capacities, 99, &view));
            assert(!lv_aic_yuv_from_mpp(&out, NULL, s, &view));
            assert(!lv_aic_yuv_from_mpp(&out, capacities, s, NULL));
            reject_mpp(NULL, capacities);
            lv_color_format_t lv;
            if(frame.format==LV_AIC_YUV_NV16 || frame.format==LV_AIC_YUV_NV61)
                assert(!lv_aic_pixel_format_from_mpp(out.format,&lv));
            else assert(lv_aic_pixel_format_from_mpp(out.format,&lv) && lv==frame.format);
            /* General metadata mapping must not accidentally widen the JPEG/
             * PNG decoder or ordinary RGB GE blit's accepted format policy. */
            assert(!lv_aic_mpp_format_to_lvgl(out.format,&lv));
            assert(!lv_aic_mpp_format_from_lvgl(frame.format,&out.format));
            assert(!lv_aic_pixel_format_is_ge2d_dst(frame.format));
            assert(!lv_aic_pixel_format_is_ge2d_src(frame.format));
        }
    }
    frame.format=LV_COLOR_FORMAT_I420;
    frame.width=31; reject(&frame); frame.width=32;
    frame.height=15; reject(&frame); frame.height=16;
    frame.planes[2].stride=95; reject(&frame); frame.planes[2].stride=96;
    frame.planes[1].data=(void *)(uintptr_t)0x3fffffff; reject(&frame);
    frame.planes[1].data=(void *)(uintptr_t)0x40002000;
    frame.planes[2].capacity=96*8-1;
    assert(lv_aic_yuv_validate(&frame)); /* CPU needs no padding after last row. */
    reject(&frame); frame.planes[2].capacity=1536;
    frame.planes[0].data=(void *)(uintptr_t)0xfffffff0; reject(&frame);
#if UINTPTR_MAX > UINT32_MAX
    frame.planes[0].data=(void *)(uintptr_t)0x100001000ULL; reject(&frame);
#endif
    frame.planes[0].data=(void *)(uintptr_t)0x40001000;
    frame.planes[0].stride=65536; frame.planes[0].capacity=16*65536;
    reject(&frame);
    frame.planes[0].stride=96; frame.planes[0].capacity=1536;
    frame.format=LV_COLOR_FORMAT_I444; frame.width=31; frame.height=15;
    assert(lv_aic_yuv_to_mpp(&frame,0x40000000,&out));
    frame.format=LV_COLOR_FORMAT_I400;
    assert(lv_aic_yuv_to_mpp(&frame,0x40000000,&out));
    assert(!lv_aic_yuv_to_mpp(&frame,0x50000000,&out));
    assert(!lv_aic_yuv_to_mpp(&frame,0,NULL));
    reject(NULL);
    /* Bottom/right crop preserves CPU-visible bytes but not full GE rows. */
    size_t capacities[3] = {32*16, 0, 0};
    memset(&out, 0, sizeof(out));
    out.buf_type = MPP_PHY_ADDR; out.format = MPP_FMT_YUV400;
    out.size = (struct mpp_size){32, 16}; out.stride[0] = 32;
    out.phy_addr[0] = 0x40001000; out.crop_en = 1;
    out.crop = (struct mpp_rect){2, 2, 30, 14};
    assert(lv_aic_yuv_from_mpp(&out, capacities, LV_AIC_YUV_BT709_FULL, &frame));
    assert(lv_aic_yuv_validate(&frame)); reject(&frame);
    capacities[0]--; reject_mpp(&out, capacities);
    out.format = MPP_FMT_RGB_888; reject_mpp(&out, capacities);
    return 0;
}
