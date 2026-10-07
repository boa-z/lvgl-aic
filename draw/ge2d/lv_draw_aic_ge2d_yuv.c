/* SPDX-License-Identifier: Apache-2.0 */
#define AIC_LVGL_USE_PRIVATE_API 1
#include "lv_draw_aic_ge2d_yuv.h"
#include "lv_draw_aic_ge2d_utils.h"
#include "lv_draw_aic_ge2d_rotate.h"
#include "lv_draw_aic_ge2d_scale.h"
#include "lv_draw_aic_ge2d_stripes.h"
#include "lv_aic_yuv_mpp.h"
#include "lv_aic_yuv_image_private.h"
#include "lv_aic_yuv_layer_private.h"
#include "lv_aic_pixel_format.h"
#if AIC_LVGL_USE_GE2D && AIC_LVGL_BSP_MPP
#include "lvgl_aic_private.h"
#include <mpp_ge.h>
#include <aic_core.h>
#include <limits.h>
#include "lv_draw_aic_ge2d_alpha.h"
#ifndef CACHE_LINE_SIZE
#define CACHE_LINE_SIZE 32U
#endif

/* Sync failure does not prove DMA has stopped. Retain the source lease and
 * optional ARGB surface until reboot; do not recycle producer storage. */
static lv_aic_yuv_image_t *quarantined;
static lv_aic_yuv_layer_t *quarantined_layer;
static lv_draw_buf_t alpha_surface;
/* Armed only while the opaque staging pass runs: every crop the engine returns
 * from is then normalized to an opaque alpha lane (see the helper header). */
static const lv_draw_buf_t *staging_target;
bool lv_draw_aic_ge2d_yuv_faulted(void) { return quarantined != NULL || quarantined_layer != NULL; }

/* Keep both filter channels' adjacent taps at partial-refresh boundaries.
 * Clamp only against real frame storage, never an intermediate clip edge. */
static int32_t filter_extent(uint32_t size, int32_t start, int32_t outputs,
                             int32_t phase, int32_t step, bool subsampled)
{
    int64_t last=phase+(int64_t)(outputs-1)*step;
    int64_t needed=(last+65535)/65536+2;
    if(subsampled) {
        int64_t chroma=(((last/2)+65535)/65536+2)*2;
        if(needed<chroma) needed=chroma;
        needed=(needed+1)&~INT64_C(1);
    }
    int64_t remaining=(int64_t)size-start;
    return (int32_t)LV_MIN(needed,remaining);
}

static int submit(lv_draw_task_t *task, const lv_aic_yuv_frame_t *frame, bool validate_only)
{
    const lv_draw_image_dsc_t *d=task->draw_dsc;
    lv_layer_t *layer=task->target_layer;
    lv_draw_buf_t *dst=layer ? layer->draw_buf : NULL;
    struct mpp_ge *ge=lv_draw_aic_ge2d_device();
    struct ge_bitblt blt={0};
    lv_area_t transformed, clip, local, crop, dst_area;
    unsigned flags;
    uint32_t floor=0;
    bool scaled=d->scale_x!=LV_SCALE_NONE || d->scale_y!=LV_SCALE_NONE;
#if defined(AIC_CHIP_D13X) || defined(AIC_CHIP_G73X)
    floor=0x40000000;
#endif
    if (!dst || !ge || d->tile ||
        d->scale_x<16 || d->scale_x>4096 || d->scale_y<16 || d->scale_y>4096 ||
        d->rotation%900 || d->skew_x || d->skew_y || d->recolor_opa>LV_OPA_MIN ||
        d->bitmap_mask_src || d->clip_radius || d->colorkey || d->blend_mode!=LV_BLEND_MODE_NORMAL ||
        d->pivot.x < -4096 || d->pivot.x > 4096 || d->pivot.y < -4096 || d->pivot.y > 4096 ||
        (dst->header.flags&LV_IMAGE_FLAGS_PREMULTIPLIED) ||
        !lv_draw_aic_ge2d_buf_address_valid(dst) ||
        !lv_aic_pixel_format_is_ge2d_dst(dst->header.cf) ||
        !lv_aic_pixel_format_to_mpp(dst->header.cf,&blt.dst_buf.format) ||
        !lv_aic_yuv_to_mpp(frame,floor,&blt.src_buf)) return 0;
#if defined(AIC_GE_DRV_V11)
    /* GE v1.1 bitblt admits only an RGB destination and a YUV400 YUV source
     * (SDK gate in packages/artinchip/mpp/ge/cmdq_ops.c). Decline every other
     * YUV layout here, before any cache handoff or submission: the engine
     * never sees the command, so the caller can fall back instead of treating
     * an admission rejection as an uncertain DMA fault. */
    if (blt.src_buf.format != MPP_FMT_YUV400 ||
        !(blt.dst_buf.format >= MPP_FMT_ARGB_8888 &&
          blt.dst_buf.format <= MPP_FMT_BGRA_4444)) return 0;
#endif
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
                                     d->scale_x,d->scale_y,&d->pivot);
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
    if (scaled) {
        lv_aic_ge2d_scale_axis_t x,y;
        if (d->rotation) {
            if (!lv_aic_ge2d_rotation_scale_crop(frame->width,frame->height,&local,&d->pivot,
                    d->rotation,d->scale_x,d->scale_y,&crop,&flags,&x.phase_16,&y.phase_16)) return 0;
            x.step_16=16777216/d->scale_x;
            y.step_16=16777216/d->scale_y;
        }
        else {
            if (!lv_aic_ge2d_scale_axis(frame->width,local.x1,lv_area_get_width(&clip),
                                     d->pivot.x,d->scale_x,&x) ||
                !lv_aic_ge2d_scale_axis(frame->height,local.y1,lv_area_get_height(&clip),
                                     d->pivot.y,d->scale_y,&y)) return 0;
            crop=(lv_area_t){x.crop,y.crop,x.crop+x.extent-1,y.crop+y.extent-1};
            flags=MPP_ROTATION_0;
        }
        blt.scale_phase.scale_phase_en=1;
        blt.scale_phase.scaler_en=1;
        blt.scale_phase.dx_16[0]=x.step_16;
        blt.scale_phase.dy_16[0]=y.step_16;
        blt.scale_phase.h_phase_16[0]=x.phase_16;
        blt.scale_phase.v_phase_16[0]=y.phase_16;
    }
    else if (!lv_aic_ge2d_rotation_crop(frame->width,frame->height,&local,&d->pivot,
                                        d->rotation,&crop,&flags)) return 0;
    int32_t w=lv_area_get_width(&crop), h=lv_area_get_height(&crop);
    bool sub_x=frame->format!=LV_COLOR_FORMAT_I400 && frame->format!=LV_COLOR_FORMAT_I444;
    bool sub_y=frame->format==LV_COLOR_FORMAT_I420 || frame->format==LV_COLOR_FORMAT_NV12 ||
               frame->format==LV_COLOR_FORMAT_NV21;
    if (scaled) {
        blt.scale_phase.channel_num=frame->format==LV_COLOR_FORMAT_I400 ? 1 : 2;
        if (sub_x) {
            /* Back the crop up to a full chroma sample and retain the original
             * first sample in the phase (which can include one whole pixel). */
            if(crop.x1&1) { crop.x1--;blt.scale_phase.h_phase_16[0]+=65536; }
            blt.scale_phase.dx_16[0]&=~1;
            blt.scale_phase.h_phase_16[0]&=~1;
        }
        if (sub_y) {
            if(crop.y1&1) { crop.y1--;blt.scale_phase.v_phase_16[0]+=65536; }
            blt.scale_phase.dy_16[0]&=~1;
            blt.scale_phase.v_phase_16[0]&=~1;
        }
        int32_t out_x=d->rotation%1800 ? lv_area_get_height(&clip) : lv_area_get_width(&clip);
        int32_t out_y=d->rotation%1800 ? lv_area_get_width(&clip) : lv_area_get_height(&clip);
        w=filter_extent(frame->width,crop.x1,out_x,blt.scale_phase.h_phase_16[0],blt.scale_phase.dx_16[0],sub_x);
        h=filter_extent(frame->height,crop.y1,out_y,blt.scale_phase.v_phase_16[0],blt.scale_phase.dy_16[0],sub_y);
        if (blt.scale_phase.channel_num==2) {
            blt.scale_phase.in_w_ch1=w>>sub_x;
            blt.scale_phase.in_h_ch1=h>>sub_y;
            blt.scale_phase.dx_16[1]=blt.scale_phase.dx_16[0]>>sub_x;
            blt.scale_phase.dy_16[1]=blt.scale_phase.dy_16[0]>>sub_y;
            blt.scale_phase.h_phase_16[1]=blt.scale_phase.h_phase_16[0]>>sub_x;
            blt.scale_phase.v_phase_16[1]=blt.scale_phase.v_phase_16[0]>>sub_y;
        }
    }
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
    unsigned commands;
    if(!lv_aic_ge2d_stripe_count(&blt,&commands)) return 0;
    if (validate_only) return 1;
    lv_draw_aic_ge2d_prepare_yuv_cache(frame);
    lv_draw_aic_ge2d_prepare_dst_cache(dst,&dst_area);
    if(lv_aic_ge2d_stripe_run(ge,&blt)!=1) return -1;
    if(staging_target && staging_target->data==dst->data) {
        /* The engine wrote this crop. Publish that DMA, then force the crop's
         * alpha lane opaque: the v1.1 bitblt leaves the Y sample there and no
         * CPU pass may read it. Gaps outside every crop stay zero-alpha. */
        aicos_dcache_invalid_range((unsigned long *)dst->data,dst->data_size);
        lv_aic_ge2d_alpha_mark_opaque(dst,&dst_area);
        lv_draw_aic_ge2d_prepare_src_cache(dst,&dst_area);
    }
    return 1;
}

/* Retain the published frame across both passes. A later unsupported tile
 * must decline the whole task before any cache operation or destination write. */
static int tiles(lv_draw_task_t *task, const lv_aic_yuv_frame_t *frame,bool validate_only)
{
    const lv_draw_image_dsc_t *d=task->draw_dsc;
    lv_area_t visible;
    int32_t w=frame->width, h=frame->height;
    if (w<8 || h<8 || !task->target_layer || !task->target_layer->draw_buf) return 0;
    if (d->opa<=LV_OPA_MIN || !lv_area_intersect(&visible,&task->area,&task->clip_area) ||
        !lv_area_intersect(&visible,&visible,&task->target_layer->buf_area)) return 2;
    const lv_area_t *anchor=d->image_area.x2==LV_COORD_MIN ? &task->area : &d->image_area;
    int64_t x0=anchor->x1,y0=anchor->y1;
    if (x0+w-1<visible.x1) x0+=((visible.x1-x0)/w)*w;
    if (y0+h-1<visible.y1) y0+=((visible.y1-y0)/h)*h;
    if (x0>visible.x2 || y0>visible.y2) return 2;
    lv_draw_image_dsc_t tile_dsc=*d;
    lv_draw_task_t tile_task=*task;
    tile_dsc.tile=0; tile_task.draw_dsc=&tile_dsc;
    bool drawn=false;
    for (int pass=0;pass<(validate_only?1:2);pass++) {
        for (int64_t y=y0;y<=visible.y2;y+=h) {
            for (int64_t x=x0;x<=visible.x2;x+=w) {
                if (x+w-1>INT32_MAX || y+h-1>INT32_MAX) return pass ? -1 : 0;
                tile_task.area=(lv_area_t){(int32_t)x,(int32_t)y,(int32_t)(x+w-1),(int32_t)(y+h-1)};
                if (!lv_area_intersect(&tile_task.clip_area,&tile_task.area,&visible)) continue;
                int result=submit(&tile_task,frame,pass==0);
                if (result==2) continue; /* A transformed tile can be empty. */
                if (result!=1) return pass ? -1 : result;
                drawn=true;
            }
        }
    }
    return drawn ? 1 : 2;
}

/* YUV has no alpha channel, so a bitmap mask can be applied after GE has
 * converted the frame into the straight ARGB staging surface. The source
 * mask is indexed with the same integer mapping used by the bounded GE
 * geometry. Orthogonal scaled masks reuse the scaler's Q16 source phase;
 * transformed tiles use the same per-cell inverse phase as GE. */
static bool yuv_mask_supported(const lv_draw_image_dsc_t *d, const lv_aic_yuv_frame_t *frame,
                               const lv_area_t *visible)
{
    const lv_image_dsc_t *mask = d->bitmap_mask_src;
    if(!mask || !mask->data || !visible ||
       (d->rotation % 900) ||
       d->skew_x || d->skew_y ||
       (mask->header.cf != LV_COLOR_FORMAT_A8 && mask->header.cf != LV_COLOR_FORMAT_L8) ||
       mask->header.w != frame->width || mask->header.h != frame->height ||
       mask->header.stride < mask->header.w ||
       (uint64_t)mask->header.stride * mask->header.h > mask->data_size) return false;

    if(d->image_area.x2 != LV_COORD_MIN) {
        if(lv_area_get_width(&d->image_area) != (int32_t)frame->width ||
           lv_area_get_height(&d->image_area) != (int32_t)frame->height) return false;
    }
    return true;
}

static int64_t yuv_floor_div_pos(int64_t numerator, uint32_t denominator)
{
    if(numerator >= 0) return numerator/(int64_t)denominator;
    return -(((-numerator)+(int64_t)denominator-1)/(int64_t)denominator);
}

static bool yuv_mask_source_local(const lv_draw_image_dsc_t *d,
                                  const lv_aic_yuv_frame_t *frame,
                                  int64_t px, int64_t py,
                                  int32_t *sx, int32_t *sy)
{
    int32_t angle = d->rotation % 3600;
    if(angle < 0) angle += 3600;
    if(d->scale_x != LV_SCALE_NONE || d->scale_y != LV_SCALE_NONE) {
        uint32_t scale_x=d->scale_x == LV_SCALE_NONE ? 256U : (uint32_t)d->scale_x;
        uint32_t scale_y=d->scale_y == LV_SCALE_NONE ? 256U : (uint32_t)d->scale_y;
        int64_t u=px*256, v=py*256;
        int64_t dx=u-(int64_t)d->pivot.x*256;
        int64_t dy=v-(int64_t)d->pivot.y*256;
        int64_t qx, qy;
        /* Match lv_aic_ge2d_rotation_scale_crop(): C division truncates
         * negative values toward zero, but GE phases use mathematical floor. */
        switch(angle) {
        case 0:
            qx=(int64_t)d->pivot.x*65536+(px-(int64_t)d->pivot.x)*((INT64_C(65536)*256U)/scale_x);
            qy=(int64_t)d->pivot.y*65536+(py-(int64_t)d->pivot.y)*((INT64_C(65536)*256U)/scale_y);
            break;
        case 900:
            qx=(int64_t)d->pivot.x*65536+yuv_floor_div_pos(dy*65536,scale_x);
            qy=(int64_t)d->pivot.y*65536+yuv_floor_div_pos(-dx*65536,scale_y);
            break;
        case 1800:
            qx=(int64_t)d->pivot.x*65536+yuv_floor_div_pos(-dx*65536,scale_x);
            qy=(int64_t)d->pivot.y*65536+yuv_floor_div_pos(-dy*65536,scale_y);
            break;
        case 2700:
            qx=(int64_t)d->pivot.x*65536+yuv_floor_div_pos(-dy*65536,scale_x);
            qy=(int64_t)d->pivot.y*65536+yuv_floor_div_pos(dx*65536,scale_y);
            break;
        default:
            return false;
        }
        px=qx; py=qy;
        if(px<0 || py<0 || px>=((int64_t)frame->width<<16) ||
           py>=((int64_t)frame->height<<16)) return false;
        *sx=(int32_t)(px>>16); *sy=(int32_t)(py>>16);
        return true;
    }
    switch(angle) {
    case 0: break;
    case 900: { int64_t t=px; px=py - d->pivot.y + d->pivot.x;
                py=d->pivot.x - t + d->pivot.y; } break;
    case 1800: px=(int64_t)2*d->pivot.x-px;
                py=(int64_t)2*d->pivot.y-py; break;
    case 2700: { int64_t t=px; px=d->pivot.y - py + d->pivot.x;
                 py=t - d->pivot.x + d->pivot.y; } break;
    default: return false;
    }
    if(px < 0 || py < 0 || px >= frame->width || py >= frame->height) return false;
    *sx=(int32_t)px; *sy=(int32_t)py;
    return true;
}

static int64_t yuv_floor_div(int64_t numerator, int64_t denominator)
{
    if(numerator >= 0) return numerator / denominator;
    return -(((-numerator) + denominator - 1) / denominator);
}

static bool yuv_mask_tile_source_at(const lv_draw_task_t *task, const lv_draw_image_dsc_t *d,
                                    const lv_aic_yuv_frame_t *frame, int32_t x, int32_t y,
                                    int32_t *sx, int32_t *sy)
{
    const lv_area_t *anchor = d->image_area.x2 == LV_COORD_MIN ? &task->area : &d->image_area;
    int32_t w=(int32_t)frame->width, h=(int32_t)frame->height;
    lv_area_t bounds;
    lv_image_buf_get_transformed_area(&bounds,w,h,d->rotation,d->scale_x,d->scale_y,&d->pivot);
    int64_t tx=(int64_t)x-anchor->x1, ty=(int64_t)y-anchor->y1;
    int64_t ix0=yuv_floor_div(tx-bounds.x2,w)-1, ix1=yuv_floor_div(tx-bounds.x1,w)+1;
    int64_t iy0=yuv_floor_div(ty-bounds.y2,h)-1, iy1=yuv_floor_div(ty-bounds.y1,h)+1;
    bool found=false;
    int64_t best_y=INT64_MIN,best_x=INT64_MIN;
    for(int64_t iy=iy0; iy<=iy1; iy++) {
        int64_t oy=(int64_t)anchor->y1+iy*h;
        int64_t local_y=(int64_t)y-oy;
        if(local_y<bounds.y1 || local_y>bounds.y2) continue;
        for(int64_t ix=ix0; ix<=ix1; ix++) {
            int64_t ox=(int64_t)anchor->x1+ix*w;
            int64_t local_x=(int64_t)x-ox;
            if(local_x<bounds.x1 || local_x>bounds.x2) continue;
            if(found && (iy<best_y || (iy==best_y && ix<best_x))) continue;
            int32_t local_sx,local_sy;
            if(!yuv_mask_source_local(d,frame,local_x,local_y,&local_sx,&local_sy)) continue;
            found=true; best_y=iy; best_x=ix; *sx=local_sx; *sy=local_sy;
        }
    }
    return found;
}

static bool yuv_mask_source_at(const lv_draw_task_t *task, const lv_draw_image_dsc_t *d,
                               const lv_aic_yuv_frame_t *frame, int32_t x, int32_t y,
                               int32_t *sx, int32_t *sy)
{
    if(d->tile) {
        return yuv_mask_tile_source_at(task,d,frame,x,y,sx,sy);
    }
    return yuv_mask_source_local(d,frame,(int64_t)x-task->area.x1,
                                 (int64_t)y-task->area.y1,sx,sy);
}

static bool yuv_mask_apply(const lv_draw_task_t *task, const lv_draw_image_dsc_t *d,
                           const lv_aic_yuv_frame_t *frame, const lv_area_t *visible,
                           lv_draw_buf_t *surface)
{
    if(!surface || surface->header.cf != LV_COLOR_FORMAT_ARGB8888 || !surface->data ||
       !yuv_mask_supported(d,frame,visible)) return false;
    const lv_image_dsc_t *mask = d->bitmap_mask_src;

    for(int32_t y=visible->y1; y<=visible->y2; y++) {
        uint8_t *dst = surface->data + (size_t)(y-visible->y1) * surface->header.stride;
        for(int32_t x=visible->x1; x<=visible->x2; x++) {
            int32_t sx, sy;
            uint8_t mask_alpha=0;
            if(yuv_mask_source_at(task,d,frame,x,y,&sx,&sy))
                mask_alpha=mask->data[(size_t)sy*mask->header.stride+(size_t)sx];
            uint8_t *pixel=dst+(x-visible->x1)*4;
            pixel[3]=(uint8_t)(((uint32_t)pixel[3]*mask_alpha+127U)/255U);
        }
    }
    return true;
}

/* YUV has no source alpha, so color-key matching can happen after GE's CSC
 * result is visible in the private ARGB surface.  LVGL keys are inclusive RGB
 * ranges; keep that semantic independent of the GE comparator's packed-color
 * rules and clear only alpha so the existing opaque-coverage tail skips the
 * keyed pixels.  This is deliberately a bounded composition extension: it
 * does not claim a native YUV key operator in the SDK GE block. */
static bool yuv_colorkey_apply(const lv_draw_image_dsc_t *d, lv_draw_buf_t *surface)
{
    if(!d || !d->colorkey || !surface ||
       surface->header.cf != LV_COLOR_FORMAT_ARGB8888 || !surface->data ||
       surface->header.w > 4096 || surface->header.h > 4096 ||
       surface->header.stride < surface->header.w * 4U ||
       (uint64_t)surface->header.stride * surface->header.h > surface->data_size)
        return false;

    for(uint32_t y=0; y<surface->header.h; y++) {
        uint8_t *row = surface->data + (size_t)y * surface->header.stride;
        for(uint32_t x=0; x<surface->header.w; x++) {
            uint8_t *p = row + x * 4U;
            lv_color_t color = lv_color_make(p[2], p[1], p[0]);
            if(lv_color_is_in_range(color, d->colorkey->low, d->colorkey->high))
                p[3] = 0;
        }
    }
    return true;
}

/* Preflight every original tile before allocating or touching caches. YUV
 * has no pixel alpha; raw CSC produces opaque ARGB inside each rendered cell. */
static int alpha_draw(lv_draw_task_t *task,const lv_aic_yuv_frame_t *frame)
{
    const lv_draw_image_dsc_t *d=task->draw_dsc;
    bool masked = d->bitmap_mask_src != NULL;
    bool keyed = d->colorkey != NULL;
    lv_draw_image_dsc_t opaque = *d;
    lv_draw_task_t preflight = *task;
    if(masked) {
        /* The mask is applied to the staging surface after GE. */
        if(!yuv_mask_supported(d,frame,&task->area)) return 0;
        opaque.bitmap_mask_src = NULL;
    }
    if(keyed) opaque.colorkey = NULL;
    preflight.draw_dsc = &opaque;
    int result=opaque.tile?tiles(&preflight,frame,true):submit(&preflight,frame,true);
    if(result!=1) return result;
    lv_area_t visible=task->area;
    if(!d->tile) {
        lv_image_buf_get_transformed_area(&visible,frame->width,frame->height,d->rotation,
                                         d->scale_x,d->scale_y,&d->pivot);
        /* submit() already checked these additions for signed overflow. */
        lv_area_move(&visible,task->area.x1,task->area.y1);
    }
    if(!lv_area_intersect(&visible,&visible,&task->clip_area)) return 2;
    lv_layer_t layer;lv_draw_task_t copy;
    if(!lv_aic_ge2d_alpha_prepare(task,&visible,&alpha_surface,&layer,&copy)) return 0;
    opaque.opa=LV_OPA_COVER;copy.draw_dsc=&opaque;
    staging_target=&alpha_surface;
    result=d->tile?tiles(&copy,frame,false):submit(&copy,frame,false);
    staging_target=NULL;
    if(result<0) return result; /* retain surface with the caller's frame lease */
    if(result==1) {
        /* GE has completed into CMA memory. The CPU mask pass must invalidate
         * that surface before reading it; alpha_finish repeats the invalidate
         * after the mask write before the native blend. */
        if(masked) aicos_dcache_invalid_range((unsigned long *)alpha_surface.data,
                                               alpha_surface.data_size);
        if(masked && !yuv_mask_apply(task,d,frame,&visible,&alpha_surface)) {
            lv_aic_ge2d_alpha_release(&alpha_surface);
            return 0;
        }
        if(keyed && !yuv_colorkey_apply(d,&alpha_surface)) {
            lv_aic_ge2d_alpha_release(&alpha_surface);
            return 0;
        }
        lv_aic_ge2d_alpha_finish(task,&alpha_surface,&layer,d->opa,LV_COLOR_FORMAT_ARGB8888,true);
    }
    lv_aic_ge2d_alpha_release(&alpha_surface);
    return result;
}

int lv_draw_aic_ge2d_yuv(lv_draw_task_t *task)
{
    if (!task || (task->type != LV_DRAW_TASK_TYPE_IMAGE &&
                  task->type != LV_DRAW_TASK_TYPE_LAYER) || !task->draw_dsc) return 0;
    const lv_draw_image_dsc_t *d=task->draw_dsc;
    const lv_aic_yuv_frame_t *frame;
    lv_aic_yuv_image_t *lease = NULL;
    lv_aic_yuv_layer_t *layer_lease = NULL;
    if (task->type == LV_DRAW_TASK_TYPE_IMAGE)
        lease = lv_aic_yuv_image_acquire(d->src,&frame);
    else
        layer_lease = lv_aic_yuv_layer_acquire((const lv_layer_t *)d->src,&frame);
    if (!lease && !layer_lease) return 0;
    if (quarantined || quarantined_layer) {
        if (lease) lv_aic_yuv_image_release_lease(lease);
        if (layer_lease) lv_aic_yuv_layer_release_lease(layer_lease);
        return -1;
    }
    bool argb=task->target_layer && task->target_layer->draw_buf &&
              task->target_layer->draw_buf->header.cf==LV_COLOR_FORMAT_ARGB8888;
    int result=argb?alpha_draw(task,frame):d->tile?tiles(task,frame,false):submit(task,frame,false);
    if (result<0) {
        quarantined=lease;
        quarantined_layer=layer_lease;
        LV_LOG_ERROR("YUV GE failure: retaining source until reboot");
    }
    else {
        if (lease) lv_aic_yuv_image_release_lease(lease);
        if (layer_lease) lv_aic_yuv_layer_release_lease(layer_lease);
    }
    return result;
}
#endif
