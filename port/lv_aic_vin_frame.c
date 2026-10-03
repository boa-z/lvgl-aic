/* SPDX-License-Identifier: Apache-2.0 */
#include "lvgl_aic_feature_config.h"
#if defined(AIC_LVGL_USE_VIN) && AIC_LVGL_USE_VIN
#include "lv_aic_vin_frame.h"
bool lv_aic_vin_frame_view(const lv_aic_vin_session_t *s, uint32_t index,
                           lv_aic_yuv_color_space_t space, lv_aic_yuv_frame_t *frame)
{
    if (!s || !frame || !s->opened || !s->pool || !s->streaming || s->faulted ||
        index>=s->buffers.num_buffers || index>=VIN_MAX_BUF_NUM ||
        !(s->held&(1U<<index)) || s->buffers.num_planes!=2) return false;
    lv_aic_yuv_frame_t result={0};
    const struct vin_video_fmt *fmt=&s->device.dst_fmt;
    switch(fmt->pixelformat) {
    case MPP_FMT_NV12: result.format=LV_COLOR_FORMAT_NV12; break;
    case MPP_FMT_NV16: result.format=LV_AIC_YUV_NV16; break;
    case MPP_FMT_YUV400: result.format=LV_COLOR_FORMAT_I400; break;
    default: return false;
    }
    result.width=fmt->width; result.height=fmt->height; result.color_space=space;
    unsigned count=result.format==LV_COLOR_FORMAT_I400 ? 1 : 2;
    for (unsigned p=0;p<count;p++) {
        const struct vin_video_plane *plane=&s->buffers.planes[index*2+p];
        if (plane->len<=0 || !plane->buf ||
            (uint64_t)(uint32_t)plane->buf+(uint32_t)plane->len>UINT64_C(0x100000000)) return false;
        result.planes[p]=(lv_aic_yuv_plane_t){
            (const uint8_t *)(uintptr_t)(uint32_t)plane->buf,
            fmt->plane_fmt[p].bytesperline,(uint32_t)plane->len};
    }
    if (!lv_aic_yuv_validate(&result)) return false;
    *frame=result;
    return true;
}
#endif
