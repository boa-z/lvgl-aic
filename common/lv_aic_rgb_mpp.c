/* SPDX-License-Identifier: Apache-2.0 */
#include "lv_aic_rgb_mpp.h"
#include "lv_aic_pixel_format.h"
#if AIC_LVGL_BSP_MPP
bool lv_aic_rgb_from_mpp(const struct mpp_buf *b,size_t capacity,lv_aic_rgb_frame_t *out)
{
    lv_aic_rgb_frame_t f={0};
    if(!b || !out || b->buf_type!=MPP_PHY_ADDR || b->size.width<=0 || b->size.height<=0 ||
       !lv_aic_pixel_format_from_mpp(b->format,&f.format)) return false;
    f.width=b->size.width; f.height=b->size.height; f.stride=b->stride[0];
    f.data=(const uint8_t *)(uintptr_t)b->phy_addr[0]; f.capacity=capacity;
    if(!lv_aic_rgb_frame_validate(&f)) return false;
    unsigned bytes=f.format==LV_COLOR_FORMAT_RGB565?2:f.format==LV_COLOR_FORMAT_RGB888?3:4;
    uint64_t span=(uint64_t)(f.height-1)*f.stride+(uint64_t)f.width*bytes;
    if(span>(uint64_t)UINT32_MAX-b->phy_addr[0]+1) return false;
    if(b->crop_en) {
        const struct mpp_rect *c=&b->crop;
        if(c->x<0 || c->y<0 || c->width<=0 || c->height<=0 ||
           (uint64_t)c->x+c->width>f.width || (uint64_t)c->y+c->height>f.height) return false;
        size_t offset=(size_t)c->y*f.stride+(size_t)c->x*bytes;
        if(offset>=f.capacity) return false;
        f.data=(const uint8_t *)((uintptr_t)f.data+offset); f.capacity-=offset;
        f.width=c->width; f.height=c->height;
        if(!lv_aic_rgb_frame_validate(&f)) return false;
    }
    *out=f; return true;
}
#endif
