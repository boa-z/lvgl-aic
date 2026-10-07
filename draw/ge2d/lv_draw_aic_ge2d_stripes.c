/* SPDX-License-Identifier: Apache-2.0 */
#include "lv_draw_aic_ge2d_stripes.h"
#include "lv_draw_aic_ge2d_scale.h"
#if AIC_LVGL_USE_GE2D && AIC_LVGL_BSP_MPP

static struct mpp_rect crop(const struct mpp_buf *b)
{
    return b->crop_en ? b->crop : (struct mpp_rect){0,0,b->size.width,b->size.height};
}

static bool bounded(const struct mpp_rect *r, const struct mpp_buf *b)
{
    return r->x>=0 && r->y>=0 && r->width>=4 && r->height>=4 &&
           (int64_t)r->x+r->width<=b->size.width && (int64_t)r->y+r->height<=b->size.height;
}

/* Chroma uses the same output grid as luma. Its input coordinates are
 * subsampled only along source axes, independent of destination rotation. */
static bool layout(enum mpp_pixel_format format, unsigned *channels,
                   unsigned *sub_x, unsigned *sub_y, unsigned *minimum)
{
    *sub_x=*sub_y=0;*channels=1;*minimum=8;
    switch(format) {
    case MPP_FMT_RGB_565: case MPP_FMT_RGB_888: case MPP_FMT_XRGB_8888: case MPP_FMT_ARGB_8888:
        *minimum=4;return true;
    case MPP_FMT_YUV400: return true;
    case MPP_FMT_YUV444P: *channels=2;return true;
    case MPP_FMT_YUV420P: case MPP_FMT_NV12: case MPP_FMT_NV21:
        *sub_y=1;break;
    case MPP_FMT_YUV422P: case MPP_FMT_NV16: case MPP_FMT_NV61:
    case MPP_FMT_YUYV: case MPP_FMT_YVYU: case MPP_FMT_UYVY: case MPP_FMT_VYUY: break;
    default:return false;
    }
    *sub_x=1;*channels=2;return true;
}

/* SDK lv_draw_ge2d_img_scale.c uses a special split for 59392 < dx <
 * 65536 and scaler-X output >=32. Keep each explicit command below
 * that width. MPP itself does not perform the LVGL adapter's split. */
static unsigned count_for(const struct ge_bitblt *b)
{
    struct mpp_rect d=crop(&b->dst_buf);
    bool vertical=MPP_ROTATION_GET(b->ctrl.flags)&1;
    int width=vertical?d.height:d.width;
    if(!b->scale_phase.scale_phase_en || !b->scale_phase.scaler_en ||
       !lv_aic_ge2d_scale_split_risk(b->scale_phase.dx_16[0],width)) return 1;
    unsigned channels,sub_x,sub_y,minimum;
    if(width>4096 || (b->ctrl.flags&~3U) ||
       !layout(b->src_buf.format,&channels,&sub_x,&sub_y,&minimum) ||
       b->scale_phase.channel_num!=(int)channels || b->scale_phase.h_phase_16[0]<0 ||
       b->scale_phase.h_phase_16[0]>=(65536<<sub_x)) return 0;
    struct mpp_rect src=crop(&b->src_buf);
    if((sub_x && ((src.x|src.width|b->scale_phase.dx_16[0]|b->scale_phase.h_phase_16[0])&1)) ||
       (sub_y && ((src.y|src.height|b->scale_phase.dy_16[0]|b->scale_phase.v_phase_16[0])&1))) return 0;
    if(channels==2 && (b->scale_phase.in_w_ch1!=(src.width>>sub_x) ||
       b->scale_phase.in_h_ch1!=(src.height>>sub_y) ||
       b->scale_phase.dx_16[1]!=(b->scale_phase.dx_16[0]>>sub_x) ||
       b->scale_phase.dy_16[1]!=(b->scale_phase.dy_16[0]>>sub_y) ||
       b->scale_phase.h_phase_16[1]!=(b->scale_phase.h_phase_16[0]>>sub_x) ||
       b->scale_phase.v_phase_16[1]!=(b->scale_phase.v_phase_16[0]>>sub_y))) return 0;
    return ((unsigned)width+30)/31;
}

bool lv_aic_ge2d_stripe(const struct ge_bitblt *b, unsigned index, struct ge_bitblt *out)
{
    if(!b || !out) return false;
    unsigned count=count_for(b);
    if(!count || index>=count) return false;
    struct ge_bitblt command=*b;
    if(count>1) {
        struct mpp_rect src=crop(&b->src_buf),dst=crop(&b->dst_buf);
        if(!bounded(&src,&b->src_buf) || !bounded(&dst,&b->dst_buf)) return false;
        unsigned rotation=MPP_ROTATION_GET(b->ctrl.flags);
        int width=(rotation&1)?dst.height:dst.width;
        /* Balance the tail: every strip is 16..31 pixels, avoiding a
         * final 1..3-pixel command outside the hardware dimension policy. */
        unsigned base=(unsigned)width/count,remainder=(unsigned)width%count;
        unsigned offset=index*base+LV_MIN(index,remainder),n=base+(index<remainder);
        /* Carry the original Q16 coordinate into each cropped input.
         * Recomputing a ratio from each strip would introduce visible seams. */
        int64_t first=b->scale_phase.h_phase_16[0]+(int64_t)offset*b->scale_phase.dx_16[0];
        unsigned channels,sub_x,sub_y,minimum;
        if(!layout(b->src_buf.format,&channels,&sub_x,&sub_y,&minimum) ||
           src.width<(int)minimum || src.height<(int)minimum ||
           dst.width<(int)minimum || dst.height<(int)minimum) return false;
        /* Align only the crop, not the absolute sampling coordinate. A luma
         * phase may retain one whole pixel when the aligned crop backs up. */
        int64_t advance=(first/65536)&~(int64_t)((1U<<sub_x)-1);
        int64_t phase=first-advance*65536;
        int64_t last=phase+(n-1)*(int64_t)b->scale_phase.dx_16[0];
        int64_t remaining=(int64_t)src.width-advance;
        if(remaining<minimum || last>(remaining-1)*65536) return false;
        int64_t needed=(last+65535)/65536+2;
        if(channels==2) {
            int64_t chroma_first=b->scale_phase.h_phase_16[1]+
                (int64_t)offset*b->scale_phase.dx_16[1]-(advance>>sub_x)*65536;
            int64_t chroma_last=chroma_first+(n-1)*(int64_t)b->scale_phase.dx_16[1];
            int64_t chroma_needed=((chroma_last+65535)/65536+2)<<sub_x;
            if(needed<chroma_needed) needed=chroma_needed;
            command.scale_phase.h_phase_16[1]=(int32_t)chroma_first;
        }
        needed=(needed+((1U<<sub_x)-1))&~(int64_t)((1U<<sub_x)-1);
        if(needed>remaining) needed=remaining;
        /* The final strip retains the original right boundary and its edge
         * clamp. Interior strips retain two taps in BOTH scaler channels. */
        if(channels==2) command.scale_phase.in_w_ch1=(int32_t)(needed>>sub_x);
        command.src_buf.crop_en=1;
        command.src_buf.crop=(struct mpp_rect){src.x+(int32_t)advance,src.y,(int32_t)needed,src.height};
        command.dst_buf.crop_en=1;
        command.dst_buf.crop=dst;
        if(rotation&1) {
            command.dst_buf.crop.y+=rotation==MPP_ROTATION_90?(int)offset:width-(int)offset-(int)n;
            command.dst_buf.crop.height=n;
        }
        else {
            command.dst_buf.crop.x+=rotation==MPP_ROTATION_0?(int)offset:width-(int)offset-(int)n;
            command.dst_buf.crop.width=n;
        }
        command.scale_phase.h_phase_16[0]=(int32_t)phase;
    }
    *out=command;
    return true;
}

bool lv_aic_ge2d_stripe_count(const struct ge_bitblt *b, unsigned *count)
{
    if(!b || !count) return false;
    unsigned n=count_for(b);
    struct ge_bitblt command;
    if(!n) return false;
    for(unsigned i=0;i<n;i++) if(!lv_aic_ge2d_stripe(b,i,&command)) return false;
    *count=n;
    return true;
}

int lv_aic_ge2d_stripe_run(struct mpp_ge *ge, const struct ge_bitblt *blt)
{
    unsigned count;
    if(!ge || !lv_aic_ge2d_stripe_count(blt,&count)) return 0;
    for(unsigned i=0;i<count;i++) {
        struct ge_bitblt command;
        if(!lv_aic_ge2d_stripe(blt,i,&command)) return -1;
        if(mpp_ge_bitblt(ge,&command)<0 || mpp_ge_emit(ge)<0 || mpp_ge_sync(ge)<0) return -1;
    }
    return 1;
}
#endif
