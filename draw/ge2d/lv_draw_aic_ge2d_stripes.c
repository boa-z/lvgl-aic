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

/* SDK lv_draw_ge2d_img_scale.c uses a special split for 59392 < dx <
 * 65536 and scaler-X output >=32. Keep each explicit RGB command below
 * that width. MPP itself does not perform the LVGL adapter's split. */
static unsigned count_for(const struct ge_bitblt *b)
{
    struct mpp_rect d=crop(&b->dst_buf);
    bool vertical=MPP_ROTATION_GET(b->ctrl.flags)&1;
    int width=vertical?d.height:d.width;
    if(!b->scale_phase.scale_phase_en || !b->scale_phase.scaler_en ||
       !lv_aic_ge2d_scale_split_risk(b->scale_phase.dx_16[0],width)) return 1;
    if(width>4096 || b->scale_phase.channel_num!=1 || (b->ctrl.flags&~3U) ||
       b->scale_phase.h_phase_16[0]<0 || b->scale_phase.h_phase_16[0]>65535) return 0;
    switch(b->src_buf.format) {
    case MPP_FMT_RGB_565: case MPP_FMT_RGB_888: case MPP_FMT_XRGB_8888: case MPP_FMT_ARGB_8888: break;
    default: return 0;
    }
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
        int64_t advance=first/65536,phase=first%65536;
        int64_t last=phase+(n-1)*(int64_t)b->scale_phase.dx_16[0];
        int64_t remaining=(int64_t)src.width-advance;
        if(remaining<4 || last>(remaining-1)*65536) return false;
        int64_t needed=(last+65535)/65536+2;
        if(needed>remaining) needed=remaining;
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
