/* SPDX-License-Identifier: Apache-2.0 */
#define AIC_LVGL_USE_PRIVATE_API 1
#include "lv_draw_aic_ge2d_yuv.h"
#include "lv_draw_aic_ge2d_utils.h"
#include "lv_draw_aic_ge2d_rotate.h"
#include "lv_aic_yuv_mpp.h"
#include "lv_aic_yuv_image_private.h"
#include "lv_aic_pixel_format.h"
#if AIC_LVGL_USE_GE2D && AIC_LVGL_BSP_MPP
#include "lvgl_aic_private.h"
#include <mpp_ge.h>
#include <aic_core.h>
#include <limits.h>
#ifndef CACHE_LINE_SIZE
#define CACHE_LINE_SIZE 32U
#endif

/* Sync failure does not prove DMA has stopped. Retain one source lease and
 * fail further YUV attempts until reboot; do not recycle producer storage. */
static lv_aic_yuv_image_t *quarantined;
bool lv_draw_aic_ge2d_yuv_faulted(void) { return quarantined != NULL; }

static int submit(lv_draw_task_t *task, const lv_aic_yuv_frame_t *frame)
{
    const lv_draw_image_dsc_t *d=task->draw_dsc;
    lv_layer_t *layer=task->target_layer;
    lv_draw_buf_t *dst=layer ? layer->draw_buf : NULL;
    struct mpp_ge *ge=lv_draw_aic_ge2d_device();
    struct ge_bitblt blt={0};
    lv_area_t transformed, clip, local, crop, dst_area;
    unsigned flags;
    uint32_t floor=0;
#if defined(AIC_CHIP_D13X) || defined(AIC_CHIP_G73X)
    floor=0x40000000;
#endif
    if (!dst || !ge || d->tile || d->scale_x!=LV_SCALE_NONE || d->scale_y!=LV_SCALE_NONE ||
        d->rotation%900 || d->skew_x || d->skew_y || d->recolor_opa>LV_OPA_MIN ||
        d->bitmap_mask_src || d->clip_radius || d->colorkey || d->blend_mode!=LV_BLEND_MODE_NORMAL ||
        d->pivot.x < -4096 || d->pivot.x > 4096 || d->pivot.y < -4096 || d->pivot.y > 4096 ||
        !lv_draw_aic_ge2d_buf_address_valid(dst) ||
        !lv_aic_pixel_format_is_ge2d_dst(dst->header.cf) ||
        !lv_aic_pixel_format_to_mpp(dst->header.cf,&blt.dst_buf.format) ||
        !lv_aic_yuv_to_mpp(frame,floor,&blt.src_buf)) return 0;
    int64_t lw=(int64_t)layer->buf_area.x2-layer->buf_area.x1+1;
    int64_t lh=(int64_t)layer->buf_area.y2-layer->buf_area.y1+1;
    uint32_t bpp=lv_color_format_get_size(dst->header.cf);
    uintptr_t address=(uintptr_t)dst->data;
    uint64_t size=(uint64_t)dst->header.stride*dst->header.h;
    if (lw<1 || lh<1 || lw>dst->header.w || lh>dst->header.h ||
        (uint64_t)dst->header.w*bpp>dst->header.stride ||
        size>dst->data_size || address>UINT32_MAX ||
        size+CACHE_LINE_SIZE-1>(uint64_t)UINT32_MAX-address) return 0;
    lv_aic_yuv_layout_t layout;
    lv_aic_yuv_layout(frame->format,frame->width,frame->height,&layout);
    for (unsigned i=0;i<layout.planes;i++) {
        uint64_t src=(uintptr_t)frame->planes[i].data;
        uint64_t span=(uint64_t)frame->planes[i].stride*layout.rows[i];
        if (src+span+CACHE_LINE_SIZE-1>UINT32_MAX || (src<address+size && address<src+span)) return 0;
    }
    if (d->opa<=LV_OPA_MIN) return 2;
    lv_image_buf_get_transformed_area(&transformed,frame->width,frame->height,d->rotation,
                                     LV_SCALE_NONE,LV_SCALE_NONE,&d->pivot);
    if ((int64_t)transformed.x1+task->area.x1<INT32_MIN ||
        (int64_t)transformed.y1+task->area.y1<INT32_MIN ||
        (int64_t)transformed.x2+task->area.x1>INT32_MAX ||
        (int64_t)transformed.y2+task->area.y1>INT32_MAX) return 0;
    lv_area_move(&transformed,task->area.x1,task->area.y1);
    if (!lv_area_intersect(&clip,&transformed,&task->clip_area) ||
        !lv_area_intersect(&clip,&clip,&layer->buf_area)) return 2;
    /* Avoid negating INT32_MIN for a far-offscreen task origin. */
    local=(lv_area_t){(int32_t)((int64_t)clip.x1-task->area.x1),
                       (int32_t)((int64_t)clip.y1-task->area.y1),
                       (int32_t)((int64_t)clip.x2-task->area.x1),
                       (int32_t)((int64_t)clip.y2-task->area.y1)};
    if (!lv_aic_ge2d_rotation_crop(frame->width,frame->height,&local,&d->pivot,
                                   d->rotation,&crop,&flags)) return 0;
    int32_t w=lv_area_get_width(&crop), h=lv_area_get_height(&crop);
    bool sub_x=frame->format!=LV_COLOR_FORMAT_I400 && frame->format!=LV_COLOR_FORMAT_I444;
    bool sub_y=frame->format==LV_COLOR_FORMAT_I420 || frame->format==LV_COLOR_FORMAT_NV12 ||
               frame->format==LV_COLOR_FORMAT_NV21;
    if (w<8 || h<8 || lv_area_get_width(&clip)<8 || lv_area_get_height(&clip)<8 ||
        (sub_x && ((crop.x1|w)&1)) || (sub_y && ((crop.y1|h)&1))) return 0;
    dst_area=(lv_area_t){(int32_t)((int64_t)clip.x1-layer->buf_area.x1),
                          (int32_t)((int64_t)clip.y1-layer->buf_area.y1),
                          (int32_t)((int64_t)clip.x2-layer->buf_area.x1),
                          (int32_t)((int64_t)clip.y2-layer->buf_area.y1)};
    blt.src_buf.crop_en=1;
    blt.src_buf.crop=(struct mpp_rect){crop.x1,crop.y1,w,h};
    blt.dst_buf.buf_type=MPP_PHY_ADDR;
    blt.dst_buf.phy_addr[0]=(uint32_t)address;
    blt.dst_buf.stride[0]=dst->header.stride;
    blt.dst_buf.size.width=dst->header.w; blt.dst_buf.size.height=dst->header.h;
    blt.dst_buf.crop_en=1;
    blt.dst_buf.crop=(struct mpp_rect){dst_area.x1,dst_area.y1,
                                      lv_area_get_width(&dst_area),lv_area_get_height(&dst_area)};
    blt.ctrl.flags=flags;
    blt.ctrl.alpha_en=d->opa<LV_OPA_COVER;
    blt.ctrl.alpha_rules=GE_PD_NONE;
    blt.ctrl.src_alpha_mode=d->opa<LV_OPA_COVER ? 2 : 0;
    blt.ctrl.src_global_alpha=d->opa;
    lv_draw_aic_ge2d_prepare_yuv_cache(frame);
    lv_draw_aic_ge2d_prepare_dst_cache(dst,&dst_area);
    if (mpp_ge_bitblt(ge,&blt)<0 || mpp_ge_emit(ge)<0 || mpp_ge_sync(ge)<0) return -1;
    return 1;
}
int lv_draw_aic_ge2d_yuv(lv_draw_task_t *task)
{
    if (!task || task->type!=LV_DRAW_TASK_TYPE_IMAGE || !task->draw_dsc) return 0;
    const lv_draw_image_dsc_t *d=task->draw_dsc;
    const lv_aic_yuv_frame_t *frame;
    lv_aic_yuv_image_t *lease=lv_aic_yuv_image_acquire(d->src,&frame);
    if (!lease) return 0;
    if (quarantined) { lv_aic_yuv_image_release_lease(lease); return -1; }
    int result=submit(task,frame);
    if (result<0) {
        quarantined=lease;
        LV_LOG_ERROR("YUV GE failure: retaining source until reboot");
    }
    else lv_aic_yuv_image_release_lease(lease);
    return result;
}
#endif
