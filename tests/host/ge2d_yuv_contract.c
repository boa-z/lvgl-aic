/* SPDX-License-Identifier: Apache-2.0 */
#define AIC_LVGL_USE_PRIVATE_API 1
#include <assert.h>
#include <stdint.h>
#include <math.h>
#include <stdio.h>
#include <string.h>
#include "../../draw/ge2d/lv_draw_aic_ge2d_yuv.c"
#include "../../include/lv_aic_yuv_layer.h"

#ifdef _WIN32
#include <windows.h>
#else
#include <sys/mman.h>
#endif
static uint8_t *arena;
static uint32_t *alpha_target;
static uint32_t alpha_before[128*128];
static bool alpha_model, yuv_mask_model, allocated, fail_alloc, fail_address;
static unsigned allocations,frees,invalidates,target_cleans,staging_cleans;
static int live;
static int calls, emits, syncs, src_caches, dst_caches, fail_at;
void *aicos_malloc_align(unsigned int type,size_t bytes,size_t align)
{
    assert(alpha_model && type==MEM_CMA && align==64 && bytes<=131072 && !allocated);
    allocations++;if(fail_alloc) return NULL;allocated=true;return arena+131072;
}
void aicos_free_align(unsigned int type,void *p)
{ assert(type==MEM_CMA && allocated && p==arena+131072);allocated=false;frees++; }
void aicos_dcache_invalid_range(unsigned long *p,unsigned long size)
{
    assert(alpha_model && (void *)p==alpha_surface.data && size==alpha_surface.data_size);
    assert(!memcmp(alpha_target,alpha_before,sizeof alpha_before));invalidates++;
}
void lv_draw_aic_ge2d_prepare_src_cache(const lv_draw_buf_t *b,const lv_area_t *a)
{
    /* Board run 9: every engine crop is published, re-marked opaque and written
     * back before the CPU tail, so each submission adds one staging invalidate.
     * Striped and tiled submissions share the one marked staging surface. */
    if(alpha_model && b->data==alpha_surface.data) { assert(a && live==1);staging_cleans++;return; }
    assert(alpha_model && b->data==(uint8_t *)alpha_target && a && live==1 &&
         invalidates==staging_cleans+target_cleans+(yuv_mask_model?2:1));target_cleans++;
}
static struct ge_bitblt captured;
static struct ge_bitblt history[32];
static int fail_submission,fail_event,event;
static lv_aic_yuv_image_t *retire_on_sync;
static bool pixel_model;
static uint32_t rendered[128*128],whole[128*128];
static unsigned pixel_scenes,pixel_count;
static int clamp_color(double value) { return (int)fmax(0,fmin(255,lround(value))); }
static uint32_t composite(double yy,double u,double v)
{
    double rgb[]={yy+1.8556*(u-128),yy-0.187324*(u-128)-0.468124*(v-128),yy+1.5748*(v-128)};
    uint32_t out=0;
    for(unsigned c=0;c<3;c++) out|=(uint32_t)((clamp_color(rgb[c])*128+165*127+127)/255)<<(c*8);
    return out;
}
static uint32_t raw_color(double yy,double u,double v)
{
    double rgb[]={yy+1.8556*(u-128),yy-0.187324*(u-128)-0.468124*(v-128),yy+1.5748*(v-128)};
    uint32_t out=0xff000000;for(unsigned c=0;c<3;c++) out|=(uint32_t)clamp_color(rgb[c])<<(c*8);
    return out;
}
/* Board run 9: the v1.1 bitblt leaves the source Y sample in the destination
 * alpha lane instead of 0xff. The executor must normalize every engine-written
 * crop before any CPU pass reads the staging surface, so the model hands the
 * raw Y byte through and keeps the pixel oracles unchanged. */
static uint32_t board_color(double yy,double u,double v)
{
    return (raw_color(yy,u,v)&0x00ffffffU)|((uint32_t)clamp_color(yy)<<24);
}
static void model(const struct ge_bitblt *b)
{
    bool mono=b->src_buf.format==MPP_FMT_YUV400;
    unsigned subx=!mono && b->src_buf.format!=MPP_FMT_YUV444P;
    unsigned suby=b->src_buf.format==MPP_FMT_YUV420P || b->src_buf.format==MPP_FMT_NV12 || b->src_buf.format==MPP_FMT_NV21;
    int w=b->dst_buf.crop.width,h=b->dst_buf.crop.height,r=MPP_ROTATION_GET(b->ctrl.flags);
    for(int y=0;y<h;y++) for(int x=0;x<w;x++) {
        int u=r==0?x:r==1?y:r==2?w-1-x:h-1-y;
        int v=r==0?y:r==1?w-1-x:r==2?h-1-y:x;
        double coords[2][2];
        for(int ch=0;ch<(mono?1:2);ch++) {
            unsigned sx=ch?subx:0,sy=ch?suby:0;
            double xx=(b->src_buf.crop.x>>sx)+(b->scale_phase.h_phase_16[ch]+(int64_t)u*b->scale_phase.dx_16[ch])/65536.0;
            double yy=(b->src_buf.crop.y>>sy)+(b->scale_phase.v_phase_16[ch]+(int64_t)v*b->scale_phase.dy_16[ch])/65536.0;
            if(!b->scale_phase.scale_phase_en) {
                xx=(b->src_buf.crop.x+u)>>sx;yy=(b->src_buf.crop.y+v)>>sy;
            }
            coords[ch][0]=fmin(xx,((b->src_buf.crop.x+b->src_buf.crop.width)>>sx)-1);
            coords[ch][1]=fmin(yy,((b->src_buf.crop.y+b->src_buf.crop.height)>>sy)-1);
        }
        double yy=40+2*coords[0][0]+coords[0][1];
        double uu=mono?128:80+2*coords[1][0]+3*coords[1][1];
        double vv=mono?128:150+coords[1][0]-2*coords[1][1];
        int pos=(b->dst_buf.crop.y+y)*128+b->dst_buf.crop.x+x;
        if(alpha_model) {
            assert(!b->ctrl.alpha_en && b->ctrl.src_global_alpha==255 && b->dst_buf.format==MPP_FMT_ARGB_8888);
            uint32_t *row=(uint32_t *)((uintptr_t)b->dst_buf.phy_addr[0]+(b->dst_buf.crop.y+y)*b->dst_buf.stride[0]);
            row[b->dst_buf.crop.x+x]=board_color(yy,uu,vv);
        } else { assert(rendered[pos]==0xa5a5a5);rendered[pos]=composite(yy,uu,vv); }
    }
}
struct mpp_ge *lv_draw_aic_ge2d_device(void) { return (void *)(uintptr_t)1; }
bool lv_draw_aic_ge2d_buf_address_valid(const lv_draw_buf_t *b)
{ return b && b->data && (uintptr_t)b->data>=0x40000000 &&
    !(fail_address && b->data==arena+131072); }
void lv_draw_aic_ge2d_prepare_yuv_cache(const lv_aic_yuv_frame_t *frame)
{ assert(lv_aic_yuv_validate(frame)); src_caches++; }
void lv_draw_aic_ge2d_prepare_dst_cache(const lv_draw_buf_t *buffer, const lv_area_t *area)
{ assert(buffer && area->x1>=0 && area->y1>=0); dst_caches++;
  if(alpha_model && buffer->data==(uint8_t *)alpha_target) memcpy(alpha_before,alpha_target,sizeof alpha_before); }
int mpp_ge_bitblt(struct mpp_ge *ge, struct ge_bitblt *blt)
{
    (void)ge;if(alpha_model) assert(!memcmp(alpha_target,alpha_before,sizeof alpha_before));
    captured=*blt;
    if(calls<32) history[calls]=*blt;
    calls++;
    if(++event==fail_event) return -1;
    if(fail_at==1 || calls==fail_submission) return -1;
    if(pixel_model || alpha_model) model(blt);
    return 0;
}
int mpp_ge_emit(struct mpp_ge *ge)
{ (void)ge; emits++; if(++event==fail_event) return -1; return fail_at==2 ? -1 : 0; }
int mpp_ge_sync(struct mpp_ge *ge)
{
    (void)ge; syncs++;
    if(++event==fail_event) return -1;
    if (retire_on_sync) {
        lv_aic_yuv_image_destroy(retire_on_sync); retire_on_sync=NULL;
        assert(live==1);
    }
    return fail_at==3 ? -1 : 0;
}
static bool retain(void *context) { (void)context; live++; return true; }
static void release(void *context) { (void)context; live--; }
static uint32_t background(unsigned i)
{
    const uint32_t colors[]={0x00204060,0x40406020,0x80206040,0xff305010};
    return colors[((i%128)/16+(i/128)/8)%4];
}
static void reset(void) { calls=emits=syncs=src_caches=dst_caches=event=0;
    assert(!allocated);allocations=frees=invalidates=target_cleans=staging_cleans=0;
    if(alpha_model) for(unsigned i=0;i<128*128;i++) alpha_target[i]=background(i);
    for(unsigned i=0;i<128*128;i++) rendered[i]=0xa5a5a5; }
static void alpha_contract(void);
int main(void)
{
    const lv_aic_yuv_format_t formats[]={LV_AIC_YUV_YVYU,LV_AIC_YUV_VYUY,LV_AIC_YUV_NV16,LV_AIC_YUV_NV61,LV_COLOR_FORMAT_I420,LV_COLOR_FORMAT_I422,
        LV_COLOR_FORMAT_I444,LV_COLOR_FORMAT_I400,LV_COLOR_FORMAT_NV12,LV_COLOR_FORMAT_NV21,
        LV_COLOR_FORMAT_YUY2,LV_COLOR_FORMAT_UYVY};
    const unsigned rotations[]={MPP_ROTATION_0,MPP_ROTATION_90,MPP_ROTATION_180,MPP_ROTATION_270};
    lv_aic_yuv_frame_t frame={0};
    frame.width=32; frame.height=16; frame.color_space=LV_AIC_YUV_BT709_FULL;
    for (unsigned p=0;p<3;p++)
        frame.planes[p]=(lv_aic_yuv_plane_t){(void *)(uintptr_t)(0x40001000+p*4096),64,2048};
    lv_init(); assert(lv_aic_yuv_image_decoder_init());
    lv_draw_buf_t buffer;
    assert(lv_draw_buf_init(&buffer,128,128,LV_COLOR_FORMAT_RGB888,384,
                            (void *)(uintptr_t)0x41000000,128*384)==LV_RESULT_OK);
    lv_layer_t layer={0};
    layer.draw_buf=&buffer; layer.buf_area=(lv_area_t){10,20,137,147};
    lv_draw_image_dsc_t d;
    lv_draw_image_dsc_init(&d); d.opa=128; d.pivot=(lv_point_t){0,0};
    lv_draw_task_t task={0};
    task.type=LV_DRAW_TASK_TYPE_IMAGE; task.draw_dsc=&d; task.target_layer=&layer;
    task.area=(lv_area_t){64,64,95,79}; task.clip_area=layer.buf_area;
    lv_aic_yuv_image_t *image;
    for (unsigned f=0;f<sizeof(formats)/sizeof(formats[0]);f++) {
        frame.format=formats[f];
        image=lv_aic_yuv_image_create(&frame,retain,release,NULL); assert(image);
        d.src=lv_aic_yuv_image_source(image);
        for (unsigned a=0;a<4;a++) {
            d.rotation=a*900; reset();
            assert(lv_draw_aic_ge2d_yuv(&task)==1);
            assert(calls==1 && emits==1 && syncs==1 && src_caches==1 && dst_caches==1);
            assert(captured.src_buf.phy_addr[0]==0x40001000 && captured.src_buf.stride[0]==64);
            if(frame.format==LV_AIC_YUV_YVYU) assert(captured.src_buf.format==MPP_FMT_YVYU);
            if(frame.format==LV_AIC_YUV_VYUY) assert(captured.src_buf.format==MPP_FMT_VYUY);
            assert(captured.src_buf.flags==MPP_COLOR_SPACE_BT709_FULL_RANGE);
            assert(captured.src_buf.crop.x==0 && captured.src_buf.crop.y==0);
            assert(captured.src_buf.crop.width==32 && captured.src_buf.crop.height==16);
            assert(captured.ctrl.flags==rotations[a] && captured.ctrl.src_global_alpha==128);
            assert(captured.dst_buf.crop.width==(a%2 ? 16 : 32));
            assert(captured.dst_buf.crop.height==(a%2 ? 32 : 16));
            assert(live==1);
            int x=captured.dst_buf.crop.x+10, y=captured.dst_buf.crop.y+20;
            int w=captured.dst_buf.crop.width, h=captured.dst_buf.crop.height;
            task.clip_area=(lv_area_t){x+2,y+2,x+w-3,y+h-3};
            reset();
            assert(lv_draw_aic_ge2d_yuv(&task)==1 && calls==1);
            assert(captured.src_buf.crop.x==2 && captured.src_buf.crop.y==2);
            assert(captured.src_buf.crop.width==28 && captured.src_buf.crop.height==12);
            task.clip_area=layer.buf_area;
        }
        d.rotation=0;
        const unsigned scales[]={128,384,512};
        for (unsigned s=0;s<3;s++) {
            d.scale_x=d.scale_y=scales[s];
            task.clip_area=(lv_area_t){64,64,87,75};
            reset(); assert(lv_draw_aic_ge2d_yuv(&task)==1);
            assert(calls==1 && emits==1 && syncs==1);
            bool sub_x=formats[f]!=LV_COLOR_FORMAT_I400 && formats[f]!=LV_COLOR_FORMAT_I444;
            bool sub_y=formats[f]==LV_COLOR_FORMAT_I420 || formats[f]==LV_COLOR_FORMAT_NV12 ||
                       formats[f]==LV_COLOR_FORMAT_NV21;
            int step=16777216/scales[s];
            assert(captured.scale_phase.scale_phase_en && captured.scale_phase.scaler_en);
            assert(captured.scale_phase.channel_num==(formats[f]==LV_COLOR_FORMAT_I400 ? 1 : 2));
            assert(captured.scale_phase.dx_16[0]==(sub_x ? step&~1 : step));
            assert(captured.scale_phase.dy_16[0]==(sub_y ? step&~1 : step));
            assert(captured.scale_phase.h_phase_16[0]==0 && captured.scale_phase.v_phase_16[0]==0);
            if (formats[f]!=LV_COLOR_FORMAT_I400) {
                assert(captured.scale_phase.dx_16[1]==(step>>sub_x));
                assert(captured.scale_phase.dy_16[1]==(step>>sub_y));
                assert(captured.scale_phase.in_w_ch1==(captured.src_buf.crop.width>>sub_x));
                assert(captured.scale_phase.in_h_ch1==(captured.src_buf.crop.height>>sub_y));
            }
            assert(captured.src_buf.crop.x+captured.src_buf.crop.width<=32);
            assert(captured.src_buf.crop.y+captured.src_buf.crop.height<=16);
            assert(!sub_x || !(captured.src_buf.crop.width&1));
            assert(!sub_y || !(captured.src_buf.crop.height&1));
        }
        d.scale_x=d.scale_y=384;
        task.clip_area=(lv_area_t){68,68,91,79};
        reset(); assert(lv_draw_aic_ge2d_yuv(&task)==1);
        assert(captured.src_buf.crop.x==2 && captured.src_buf.crop.y==2);
        /* 4 / 1.5 = 2 + 2/3: independent expected Q16 fractional phase. */
        assert(captured.scale_phase.h_phase_16[0]==43688);
        assert(captured.scale_phase.v_phase_16[0]==43688);
        d.scale_x=384; d.scale_y=512; d.pivot=(lv_point_t){16,8};
        for (unsigned angle=1;angle<4;angle++) {
            lv_point_t p[4]={{2,2},{26,2},{2,12},{26,12}};
            d.rotation=angle*900;
            lv_point_array_transform(p,4,d.rotation,d.scale_x,d.scale_y,&d.pivot,true);
            lv_area_t clip={p[0].x,p[0].y,p[0].x,p[0].y};
            for (unsigned i=1;i<4;i++) {
                if(p[i].x<clip.x1) clip.x1=p[i].x;
                if(p[i].x>clip.x2) clip.x2=p[i].x;
                if(p[i].y<clip.y1) clip.y1=p[i].y;
                if(p[i].y>clip.y2) clip.y2=p[i].y;
            }
            lv_area_move(&clip,64,64); task.clip_area=clip;
            reset(); assert(lv_draw_aic_ge2d_yuv(&task)==1);
            assert(calls==1 && emits==1 && syncs==1);
            assert(captured.ctrl.flags==rotations[angle]);
            assert(captured.src_buf.crop.x==2 && captured.src_buf.crop.y==2);
            assert(captured.scale_phase.dx_16[0]==43690 && captured.scale_phase.dy_16[0]==32768);
            assert(captured.scale_phase.h_phase_16[0]==0 && captured.scale_phase.v_phase_16[0]==0);
        }
        d.pivot=(lv_point_t){0,0};d.scale_x=264;d.scale_y=256;
        const lv_area_t stripe_clips[]={{64,64,95,79},{49,64,64,95},{33,49,64,64},{64,33,79,64}};
        for(unsigned a=0;a<4;a++) {
            pixel_model=true;
            d.rotation=a*900;task.clip_area=stripe_clips[a];reset();
            assert(lv_draw_aic_ge2d_yuv(&task)==1 && calls==2 && syncs==2 && src_caches==1 && dst_caches==1);
            bool sub_x=formats[f]!=LV_COLOR_FORMAT_I400 && formats[f]!=LV_COLOR_FORMAT_I444;
            for(unsigned strip=0;strip<2;strip++) {
                assert(history[strip].ctrl.flags==rotations[a]);
                assert(!sub_x || !(history[strip].src_buf.crop.x&1));
            }
            bool sub_y=formats[f]==LV_COLOR_FORMAT_I420 || formats[f]==LV_COLOR_FORMAT_NV12 || formats[f]==LV_COLOR_FORMAT_NV21;
            for(int y=0;y<128;y++) for(int x=0;x<128;x++) {
                int xx=x+10,yy=y+20;
                if(xx<task.clip_area.x1 || xx>task.clip_area.x2 || yy<task.clip_area.y1 || yy>task.clip_area.y2) {
                    assert(rendered[y*128+x]==0xa5a5a5);continue;
                }
                int dx=xx-64,dy=yy-64;
                double u=(a==0?dx:a==1?dy:a==2?-dx:-dy)*256.0/264;
                double v=a==0?dy:a==1?-dx:a==2?-dy:dx;
                double cx=fmin(u/(sub_x?2:1),(32>>sub_x)-1),cy=fmin(v/(sub_y?2:1),(16>>sub_y)-1);
                bool mono=formats[f]==LV_COLOR_FORMAT_I400;
                uint32_t expected=composite(40+2*u+v,mono?128:80+2*cx+3*cy,mono?128:150+cx-2*cy);
                for(unsigned c=0;c<3;c++)
                    assert(abs((int)((rendered[y*128+x]>>(c*8))&255)-(int)((expected>>(c*8))&255))<=1);
                pixel_count++;
            }
            memcpy(whole,rendered,sizeof whole);reset();
            /* Split along the unscaled source-Y axis at an aligned row. */
            for(unsigned half=0;half<2;half++) {
                task.clip_area=stripe_clips[a];
                if(a&1) {
                    int middle=(task.clip_area.x1+task.clip_area.x2)/2;
                    if(half) task.clip_area.x1=middle+1;else task.clip_area.x2=middle;
                } else {
                    int middle=(task.clip_area.y1+task.clip_area.y2)/2;
                    if(half) task.clip_area.y1=middle+1;else task.clip_area.y2=middle;
                }
                assert(lv_draw_aic_ge2d_yuv(&task)==1);
            }
            assert(!memcmp(whole,rendered,sizeof whole));
            /* Source-X split begins inside a chroma pair; retain that sample
             * by backing the crop up and carrying a whole luma pixel in phase. */
            reset();
            for(unsigned half=0;half<2;half++) {
                task.clip_area=stripe_clips[a];
                if(a&1) {
                    int middle=(task.clip_area.y1+task.clip_area.y2)/2;
                    if(half) task.clip_area.y1=middle+1;else task.clip_area.y2=middle;
                } else {
                    int middle=(task.clip_area.x1+task.clip_area.x2)/2;
                    if(half) task.clip_area.x1=middle+1;else task.clip_area.x2=middle;
                }
                assert(lv_draw_aic_ge2d_yuv(&task)==1);
            }
            for(unsigned i=0;i<128*128;i++) for(unsigned c=0;c<3;c++)
                assert(abs((int)((whole[i]>>(c*8))&255)-(int)((rendered[i]>>(c*8))&255))<=1);
            reset();task.clip_area=stripe_clips[a];
            if(a&1) { task.clip_area.x1++;task.clip_area.x2--; }
            else { task.clip_area.y1++;task.clip_area.y2--; }
            assert(lv_draw_aic_ge2d_yuv(&task)==1);
            for(int y=0;y<128;y++) for(int x=0;x<128;x++) {
                bool inside=x+10>=task.clip_area.x1 && x+10<=task.clip_area.x2 &&
                            y+20>=task.clip_area.y1 && y+20<=task.clip_area.y2;
                if(!inside) assert(rendered[y*128+x]==0xa5a5a5);
                else for(unsigned c=0;c<3;c++)
                    assert(abs((int)((whole[y*128+x]>>(c*8))&255)-(int)((rendered[y*128+x]>>(c*8))&255))<=1);
            }
            pixel_scenes++;pixel_model=false;
        }
        d.rotation=0;d.tile=1;task.area=(lv_area_t){64,64,127,95};task.clip_area=layer.buf_area;
        reset();assert(lv_draw_aic_ge2d_yuv(&task)==1 && calls==8 && syncs==8 && src_caches==4);
        task.area.x2=128;reset();assert(lv_draw_aic_ge2d_yuv(&task)==0 && !calls && !src_caches);
        task.area=(lv_area_t){64,64,95,79};d.tile=0;
        d.rotation=0; d.pivot=(lv_point_t){0,0};
        d.scale_x=d.scale_y=256; task.clip_area=layer.buf_area;
        d.tile=1; task.area=(lv_area_t){64,64,127,95};
        reset(); assert(lv_draw_aic_ge2d_yuv(&task)==1 && calls==4);
        d.scale_x=d.scale_y=512;
        reset(); assert(lv_draw_aic_ge2d_yuv(&task)==1 && calls==4);
        for(unsigned i=0;i<4;i++) {
            assert(history[i].dst_buf.crop.x==(int)(54+(i%2)*32));
            assert(history[i].dst_buf.crop.y==(int)(44+(i/2)*16));
            assert(history[i].dst_buf.crop.width==32 && history[i].dst_buf.crop.height==16);
            assert(history[i].scale_phase.dx_16[0]==32768 && history[i].scale_phase.dy_16[0]==32768);
        }
        d.rotation=900; d.pivot=(lv_point_t){16,8};
        reset(); assert(lv_draw_aic_ge2d_yuv(&task)==1 && calls==4);
        for(unsigned i=0;i<4;i++) {
            assert(history[i].ctrl.flags==MPP_ROTATION_90);
            assert(history[i].dst_buf.crop.x==(int)(56+(i%2)*32));
            assert(history[i].dst_buf.crop.width==30 && history[i].dst_buf.crop.height==16);
        }
        d.rotation=0; d.pivot=(lv_point_t){0,0};
        d.scale_x=d.scale_y=256; d.tile=0; task.area=(lv_area_t){64,64,95,79};
        lv_aic_yuv_image_destroy(image); assert(live==0);
    }
    frame.format=LV_COLOR_FORMAT_I420; d.rotation=0;
    image=lv_aic_yuv_image_create(&frame,retain,release,NULL); d.src=lv_aic_yuv_image_source(image);
    task.clip_area=(lv_area_t){65,64,95,79}; reset();
    assert(lv_draw_aic_ge2d_yuv(&task)==0 && !calls && !src_caches && !dst_caches);
    task.clip_area=(lv_area_t){66,66,89,77};
    assert(lv_draw_aic_ge2d_yuv(&task)==1);
    assert(captured.src_buf.crop.x==2 && captured.src_buf.crop.y==2);
    assert(captured.src_buf.crop.width==24 && captured.src_buf.crop.height==12);
    d.scale_x=384; reset(); assert(lv_draw_aic_ge2d_yuv(&task)==1 && calls==1);
    d.scale_x=256; d.tile=1;
    task.area=(lv_area_t){64,64,127,95}; task.clip_area=layer.buf_area;
    reset(); assert(lv_draw_aic_ge2d_yuv(&task)==1);
    assert(calls==4 && emits==4 && syncs==4 && src_caches==4 && dst_caches==4);
    for(int i=0;i<4;i++) {
        assert(history[i].dst_buf.crop.x==54+(i%2)*32);
        assert(history[i].dst_buf.crop.y==44+(i/2)*16);
        assert(history[i].src_buf.crop.x==0 && history[i].src_buf.crop.y==0);
        assert(history[i].src_buf.crop.width==32 && history[i].src_buf.crop.height==16);
    }
    /* An invalid last column must prevent writes by every earlier tile. */
    task.area.x2=128; reset();
    assert(lv_draw_aic_ge2d_yuv(&task)==0 && !calls && !src_caches && !dst_caches);
    task.area.x2=127; task.clip_area=(lv_area_t){66,66,125,93};
    reset(); assert(lv_draw_aic_ge2d_yuv(&task)==1 && calls==4);
    assert(history[0].src_buf.crop.x==2 && history[0].src_buf.crop.y==2);
    assert(history[3].src_buf.crop.width==30 && history[3].src_buf.crop.height==14);
    /* Skip a long invisible prefix without changing the repeat origin. */
    d.image_area=(lv_area_t){-640000,-640000,-639969,-639985};
    task.clip_area=layer.buf_area; reset();
    assert(lv_draw_aic_ge2d_yuv(&task)==1 && calls==4);
    d.image_area.x2=LV_COORD_MIN;
    d.scale_x=384; reset(); assert(lv_draw_aic_ge2d_yuv(&task)==1 && calls==4);
    /* Empty first transformed cell must not hide a later visible cell. */
    d.scale_x=d.scale_y=128; task.clip_area=(lv_area_t){81,64,110,71};
    reset(); assert(lv_draw_aic_ge2d_yuv(&task)==1 && calls==1);
    assert(captured.dst_buf.crop.x==86 && captured.dst_buf.crop.width==15);
    task.clip_area.x2=95; reset();
    assert(lv_draw_aic_ge2d_yuv(&task)==2 && !calls && !src_caches && !dst_caches);
    d.scale_x=d.scale_y=256; d.tile=0; task.area=(lv_area_t){64,64,95,79};
    d.rotation=450; assert(lv_draw_aic_ge2d_yuv(&task)==0); d.rotation=0;
    task.clip_area=(lv_area_t){0,0,1,1}; assert(lv_draw_aic_ge2d_yuv(&task)==2);
    task.clip_area=layer.buf_area; d.opa=0; assert(lv_draw_aic_ge2d_yuv(&task)==2); d.opa=128;
    lv_aic_yuv_image_destroy(image); assert(live==0);
    frame.planes[0].data=buffer.data;
    image=lv_aic_yuv_image_create(&frame,retain,release,NULL); d.src=lv_aic_yuv_image_source(image);
    reset(); assert(lv_draw_aic_ge2d_yuv(&task)==0 && !calls && !src_caches);
    lv_aic_yuv_image_destroy(image);
    frame.planes[0].data=(void *)(uintptr_t)0x30001000;
    image=lv_aic_yuv_image_create(&frame,retain,release,NULL); d.src=lv_aic_yuv_image_source(image);
    assert(lv_draw_aic_ge2d_yuv(&task)==0 && !calls && !src_caches);
    lv_aic_yuv_image_destroy(image);
    frame.planes[0].data=(void *)(uintptr_t)0x40001000;
    image=lv_aic_yuv_image_create(&frame,retain,release,NULL); d.src=lv_aic_yuv_image_source(image);
    retire_on_sync=image;
    d.tile=1; task.area=(lv_area_t){64,64,127,95}; reset();
    assert(lv_draw_aic_ge2d_yuv(&task)==1 && calls==4 && live==0);
    /* A fault after a completed first tile must retain the same frame lease
     * and stop submission, never restart the task as a software blend. */
    image=lv_aic_yuv_image_create(&frame,retain,release,NULL); d.src=lv_aic_yuv_image_source(image);
    d.tile=1; task.area=(lv_area_t){64,64,127,95}; fail_submission=2;
    reset(); assert(lv_draw_aic_ge2d_yuv(&task)==-1 && calls==2 && syncs==1);
    assert(lv_draw_aic_ge2d_yuv(&task)==-1 && calls==2);
    lv_aic_yuv_image_destroy(image); assert(live==1);
    lv_aic_yuv_image_release_lease(quarantined); quarantined=NULL;
    assert(live==0); fail_submission=0; d.tile=0; task.area=(lv_area_t){64,64,95,79};
    /* Owner retirement during the first strip must not release its planes.
     * Every failure position also retains the same lease across all strips. */
    d.scale_x=264;task.clip_area=task.area;
    image=lv_aic_yuv_image_create(&frame,retain,release,NULL);d.src=lv_aic_yuv_image_source(image);
    retire_on_sync=image;reset();assert(lv_draw_aic_ge2d_yuv(&task)==1 && calls==2 && live==0);
    for(fail_event=1;fail_event<=6;fail_event++) {
        image=lv_aic_yuv_image_create(&frame,retain,release,NULL);d.src=lv_aic_yuv_image_source(image);
        reset();assert(lv_draw_aic_ge2d_yuv(&task)==-1 && event==fail_event);
        assert(lv_draw_aic_ge2d_yuv(&task)==-1 && event==fail_event);
        lv_aic_yuv_image_destroy(image);assert(live==1);
        lv_aic_yuv_image_release_lease(quarantined);quarantined=NULL;assert(live==0);
    }
    fail_event=0;d.scale_x=256;task.clip_area=layer.buf_area;
    for (fail_at=1;fail_at<=3;fail_at++) {
        image=lv_aic_yuv_image_create(&frame,retain,release,NULL); d.src=lv_aic_yuv_image_source(image);
        reset();
        assert(lv_draw_aic_ge2d_yuv(&task)==-1 && lv_draw_aic_ge2d_yuv_faulted());
        assert(calls==1 && emits==(fail_at>=2) && syncs==(fail_at>=3));
        assert(lv_draw_aic_ge2d_yuv(&task)==-1 && calls==1);
        lv_aic_yuv_image_destroy(image);
        assert(live==1 && !lv_aic_yuv_image_decoder_deinit());
        /* Only this synchronous mock has proven there is no pending DMA.
         * Production intentionally exposes no reset/unpin without reboot. */
        lv_aic_yuv_image_release_lease(quarantined); quarantined=NULL;
        assert(live==0);
    }
    /* A YUV layer is admitted only through the explicit application adapter.
     * The child has no generic LVGL draw buffer: the immutable planes are the
     * source contract consumed by the GE path. */
    {
        lv_layer_t child={0};
        child.color_format=LV_COLOR_FORMAT_I420;
        child.buf_area=(lv_area_t){0,0,31,15};
        frame.format=LV_COLOR_FORMAT_I420;
        assert(lv_aic_yuv_layer_attach(&child,&frame,retain,release,NULL));
        d.src=&child; d.rotation=0; d.scale_x=d.scale_y=LV_SCALE_NONE; d.opa=128;
        task.type=LV_DRAW_TASK_TYPE_LAYER; task.draw_dsc=&d; task.area=(lv_area_t){64,64,95,79};
        task.clip_area=layer.buf_area; reset();
        assert(lv_draw_aic_ge2d_yuv(&task)==1 && calls==1);
        assert(captured.src_buf.format==MPP_FMT_YUV420P && captured.src_buf.phy_addr[0]==0x40001000);
        assert(lv_aic_yuv_layer_detach(&child) && live==0);
        child.color_format=LV_COLOR_FORMAT_I422;
        assert(!lv_aic_yuv_layer_attach(&child,&frame,retain,release,NULL));
    }
    fail_at=0;alpha_contract();
    assert(lv_aic_yuv_image_decoder_deinit()); lv_deinit();
    printf("PASS %u YUV colored stripe scenes, %u inverse-mapped pixels, full/partial pixel checks and retained frame leases\n",pixel_scenes,pixel_count);
    return 0;
}

static void alpha_contract(void)
{
#ifdef _WIN32
    arena=VirtualAlloc((void *)(uintptr_t)0x48000000,262144,MEM_RESERVE|MEM_COMMIT,PAGE_READWRITE);
#else
    arena=mmap((void *)(uintptr_t)0x48000000,262144,PROT_READ|PROT_WRITE,MAP_PRIVATE|MAP_ANONYMOUS|MAP_FIXED_NOREPLACE,-1,0);
#endif
    assert(arena==(void *)(uintptr_t)0x48000000);alpha_target=(uint32_t *)arena;alpha_model=true;
    const lv_aic_yuv_format_t formats[]={LV_AIC_YUV_YVYU,LV_AIC_YUV_VYUY,LV_AIC_YUV_NV16,LV_AIC_YUV_NV61,
        LV_COLOR_FORMAT_I420,LV_COLOR_FORMAT_I422,LV_COLOR_FORMAT_I444,LV_COLOR_FORMAT_I400,
        LV_COLOR_FORMAT_NV12,LV_COLOR_FORMAT_NV21,LV_COLOR_FORMAT_YUY2,LV_COLOR_FORMAT_UYVY};
    /* 2x joined the matrix after board run 10: the rotated 2x strip is the
     * geometry whose inverse source window approaches the engine's 8x8 YUV
     * minimum, and it must stay admissible with the staged ARGB target. */
    const unsigned ratios[]={256,264,384,512},opacities[]={64,128,255};
    const lv_area_t clips[]={{64,64,95,79},{49,64,64,95},{33,49,64,64},{64,33,79,64}};
    lv_aic_yuv_frame_t frame={.width=32,.height=16,.color_space=LV_AIC_YUV_BT709_FULL};
    for(unsigned p=0;p<3;p++) frame.planes[p]=(lv_aic_yuv_plane_t){(void *)(uintptr_t)(0x40001000+p*4096),64,2048};
    lv_draw_buf_t dst;assert(lv_draw_buf_init(&dst,128,128,LV_COLOR_FORMAT_ARGB8888,512,alpha_target,65536)==LV_RESULT_OK);
    lv_layer_t layer={0};layer.draw_buf=&dst;layer.color_format=LV_COLOR_FORMAT_ARGB8888;layer.buf_area=(lv_area_t){10,20,137,147};
    lv_draw_image_dsc_t d;lv_draw_image_dsc_init(&d);d.pivot=(lv_point_t){0,0};
    lv_draw_task_t task={0};task.type=LV_DRAW_TASK_TYPE_IMAGE;task.draw_dsc=&d;task.target_layer=&layer;
    task.area=(lv_area_t){64,64,95,79};unsigned scenes=0,pixels=0,worst=0,partial_worst=0;
    for(unsigned f=0;f<12;f++) {
        frame.format=formats[f];lv_aic_yuv_image_t *image=lv_aic_yuv_image_create(&frame,retain,release,NULL);assert(image);
        d.src=lv_aic_yuv_image_source(image);
        bool mono=formats[f]==LV_COLOR_FORMAT_I400,subx=!mono && formats[f]!=LV_COLOR_FORMAT_I444;
        bool suby=formats[f]==LV_COLOR_FORMAT_I420 || formats[f]==LV_COLOR_FORMAT_NV12 || formats[f]==LV_COLOR_FORMAT_NV21;
        for(unsigned angle=0;angle<4;angle++)
        for(unsigned zoom=0;zoom<sizeof(ratios)/sizeof(ratios[0]);zoom++)
        for(unsigned opacity=0;opacity<3;opacity++) {
            d.rotation=angle*900;d.scale_x=ratios[zoom];d.opa=opacities[opacity];task.clip_area=clips[angle];
            reset();assert(lv_draw_aic_ge2d_yuv(&task)==1);
            assert(allocations==1 && frees==1 && invalidates==2 && staging_cleans==1 &&
                   target_cleans==1 && live==1);
            for(int y=0;y<128;y++) for(int x=0;x<128;x++) {
                unsigned i=y*128+x;int xx=x+10,yy=y+20;
                if(xx<task.clip_area.x1 || xx>task.clip_area.x2 || yy<task.clip_area.y1 || yy>task.clip_area.y2) {
                    assert(alpha_target[i]==background(i));continue;
                }
                int dx=xx-64,dy=yy-64;
                double u=(angle==0?dx:angle==1?dy:angle==2?-dx:-dy)*256.0/d.scale_x;
                double v=angle==0?dy:angle==1?-dx:angle==2?-dy:dx;
                double cx=fmin(u/(subx?2:1),(32>>subx)-1),cy=fmin(v/(suby?2:1),(16>>suby)-1);
                if(!zoom) { cx=floor(cx);cy=floor(cy); }
                uint32_t foreground=raw_color(40+2*u+v,mono?128:80+2*cx+3*cy,mono?128:150+cx-2*cy),bg=background(i);
                double alpha=d.opa+(bg>>24)*(255.0-d.opa)/255.0;
                for(unsigned c=0;c<4;c++) {
                    unsigned want=c==3?(unsigned)lround(alpha):(unsigned)lround((double)((foreground>>(8*c))&255)*d.opa/alpha+
                        ((bg>>(8*c))&255)*(bg>>24)*(255.0-d.opa)/(255.0*alpha));
                    unsigned error=abs((int)((alpha_target[i]>>(8*c))&255)-(int)want);
                    if(error>worst) worst=error;
                    if(error>3) fprintf(stderr,"ARGB YUV f=%u angle=%u zoom=%u opa=%u xy=%d,%d c=%u error=%u\n",f,angle,zoom,d.opa,x,y,c,error);
                    assert(error<=3);
                }
                pixels++;
            }
            memcpy(whole,alpha_target,sizeof whole);reset();
            for(unsigned half=0;half<2;half++) {
                task.clip_area=clips[angle];
                if(angle&1) { int m=(task.clip_area.x1+task.clip_area.x2)/2;if(half)task.clip_area.x1=m+1;else task.clip_area.x2=m; }
                else { int m=(task.clip_area.y1+task.clip_area.y2)/2;if(half)task.clip_area.y1=m+1;else task.clip_area.y2=m; }
                assert(lv_draw_aic_ge2d_yuv(&task)==1);
            }
            assert(!memcmp(whole,alpha_target,sizeof whole));reset();
            for(unsigned half=0;half<2;half++) {
                task.clip_area=clips[angle];
                if(angle&1) { int m=(task.clip_area.y1+task.clip_area.y2)/2;if(half)task.clip_area.y1=m+1;else task.clip_area.y2=m; }
                else { int m=(task.clip_area.x1+task.clip_area.x2)/2;if(half)task.clip_area.x1=m+1;else task.clip_area.x2=m; }
                assert(lv_draw_aic_ge2d_yuv(&task)==1);
            }
            for(unsigned i=0;i<128*128;i++) for(unsigned c=0;c<4;c++) {
                unsigned error=abs((int)((alpha_target[i]>>(8*c))&255)-(int)((whole[i]>>(8*c))&255));
                if(error>partial_worst) partial_worst=error;
                assert(error<=2);
            }
            scenes++;
        }
        lv_aic_yuv_image_destroy(image);assert(!live);
    }
    frame.format=LV_COLOR_FORMAT_I420;
    lv_aic_yuv_image_t *image=lv_aic_yuv_image_create(&frame,retain,release,NULL);assert(image);
    d.src=lv_aic_yuv_image_source(image);d.tile=true;d.scale_x=d.scale_y=256;d.rotation=900;
    d.pivot=(lv_point_t){16,8};d.opa=128;task.area=task.clip_area=(lv_area_t){64,64,127,95};
    reset();assert(lv_draw_aic_ge2d_yuv(&task)==1 && allocations==1 && frees==1 && calls==4);
    for(unsigned y=0;y<128;y++) for(unsigned x=0;x<128;x++) {
        int xx=x+10,yy=y+20;
        bool in=xx>=64 && xx<=127 && yy>=64 && yy<=95 && (xx-64)%32>=9 && (xx-64)%32<=24;
        if(!in) assert(alpha_target[y*128+x]==background(y*128+x));
    }
    memcpy(whole,alpha_target,sizeof whole);reset();
    for(unsigned half=0;half<2;half++) {
        task.clip_area=(lv_area_t){64+half*32,64,95+half*32,95};
        assert(lv_draw_aic_ge2d_yuv(&task)==1);
    }
    assert(!memcmp(whole,alpha_target,sizeof whole));
    lv_aic_yuv_image_destroy(image);assert(!live);
    /* Bounded YUV mask extension: GE writes opaque CSC pixels into the
     * straight-ARGB surface, then the A8 mask gates only source alpha. */
    {
        uint8_t mask_pixels[32 * 16];
        lv_image_dsc_t mask={0};
        for(unsigned y=0;y<16;y++) for(unsigned x=0;x<32;x++)
            mask_pixels[y*32+x]=(uint8_t)((x&1) ? 255 : 0);
        mask.header.cf=LV_COLOR_FORMAT_A8; mask.header.w=32; mask.header.h=16;
        mask.header.stride=32; mask.data=mask_pixels; mask.data_size=sizeof mask_pixels;
        frame.format=LV_COLOR_FORMAT_I420; yuv_mask_model=true;
        image=lv_aic_yuv_image_create(&frame,retain,release,NULL); assert(image);
        d.src=lv_aic_yuv_image_source(image); d.bitmap_mask_src=&mask;
        d.rotation=0; d.scale_x=d.scale_y=LV_SCALE_NONE; d.tile=false; d.opa=255;
        task.area=task.clip_area=(lv_area_t){64,64,95,79}; reset();
        assert(lv_draw_aic_ge2d_yuv(&task)==1 && calls==1 && allocations==1 && frees==1 &&
               invalidates==3 && staging_cleans==1);
        for(unsigned y=0;y<16;y++) for(unsigned x=0;x<32;x++) {
            unsigned i=(y+44)*128+(x+54);
            if(!(x&1)) assert(alpha_target[i]==background(i));
            else assert(alpha_target[i]==raw_color(40+2*x+y,80+2*(x/2)+3*(y/2),150+(x/2)-2*(y/2)));
        }
        lv_aic_yuv_image_destroy(image); assert(!live); d.bitmap_mask_src=NULL;
        /* Orthogonal rotation uses the same inverse crop as GE, so the mask
         * remains source-aligned after the ARGB staging pass. */
        image=lv_aic_yuv_image_create(&frame,retain,release,NULL); assert(image);
        d.src=lv_aic_yuv_image_source(image); d.bitmap_mask_src=&mask;
        d.rotation=900; d.pivot=(lv_point_t){0,0}; d.tile=false;
        task.area=(lv_area_t){64,64,95,79}; task.clip_area=(lv_area_t){49,64,64,95};
        yuv_mask_model=true; reset();
        assert(lv_draw_aic_ge2d_yuv(&task)==1 && calls==1 && allocations==1 && frees==1);
        assert(alpha_target[44*128+54]==background(44*128+54));
        assert(alpha_target[45*128+54]==raw_color(42,80,150));
        lv_aic_yuv_image_destroy(image); assert(!live); yuv_mask_model=false;
        /* Unscaled repeated tiles use modulo source coordinates for every
         * staged tile while retaining whole-task preflight. */
        image=lv_aic_yuv_image_create(&frame,retain,release,NULL); assert(image);
        d.src=lv_aic_yuv_image_source(image); d.rotation=0; d.tile=true;
        task.area=task.clip_area=(lv_area_t){64,64,127,95}; yuv_mask_model=true; reset();
        assert(lv_draw_aic_ge2d_yuv(&task)==1 && calls==4 && allocations==1 && frees==1);
        assert(alpha_target[44*128+54]==background(44*128+54));
        assert(alpha_target[44*128+86]==background(44*128+86));
        lv_aic_yuv_image_destroy(image); assert(!live); yuv_mask_model=false;
        /* A non-rotated scale reuses the GE Q16 source phase for the mask
         * lookup.  The alternating source mask therefore follows the same
         * integer source selection as the scaled YUV sample. */
        image=lv_aic_yuv_image_create(&frame,retain,release,NULL); assert(image);
        d.src=lv_aic_yuv_image_source(image); d.bitmap_mask_src=&mask;
        d.tile=false; d.scale_x=384; task.area=(lv_area_t){64,64,95,79};
        task.clip_area=task.area; yuv_mask_model=true; reset();
        assert(lv_draw_aic_ge2d_yuv(&task)==1 && calls==1 && allocations==1 && frees==1);
        for(unsigned y=0;y<16;y++) for(unsigned x=0;x<32;x++) {
            unsigned sx=(x*((65536U*256U)/384U))>>16;
            unsigned i=(y+44)*128+(x+54);
            if(!(sx&1)) assert(alpha_target[i]==background(i));
            else assert(alpha_target[i]!=background(i));
        }
        lv_aic_yuv_image_destroy(image); d.bitmap_mask_src=NULL; d.scale_x=LV_SCALE_NONE; yuv_mask_model=false;
        const lv_area_t scaled_rotation_clips[] = {
            {64,64,95,79}, {49,64,64,95}, {33,49,64,64}, {64,33,79,64}
        };
        for(unsigned angle=0; angle<4; angle++) {
            image=lv_aic_yuv_image_create(&frame,retain,release,NULL); assert(image);
            d.src=lv_aic_yuv_image_source(image); d.bitmap_mask_src=&mask;
            d.scale_x=384; d.scale_y=256; d.rotation=(int32_t)angle*900;
            d.pivot=(lv_point_t){0,0}; task.area=(lv_area_t){64,64,95,79};
            task.clip_area=scaled_rotation_clips[angle]; yuv_mask_model=true; reset();
            assert(lv_draw_aic_ge2d_yuv(&task)==1 && calls==1 && allocations==1 && frees==1);
            for(int yy=task.clip_area.y1; yy<=task.clip_area.y2; yy++)
                for(int xx=task.clip_area.x1; xx<=task.clip_area.x2; xx++) {
                    int64_t dx=(int64_t)(xx-64)*256,dy=(int64_t)(yy-64)*256;
                    int32_t sx=-1,sy=-1;
                    double qx,qy;
                    switch(angle) {
                    case 0:
                        qx=(xx-64)*(65536*256/384);
                        qy=(yy-64)*(65536*256/256); break;
                    case 1:
                        qx=floor((double)dy*65536.0/384.0);
                        qy=floor((double)-dx*65536.0/256.0); break;
                    case 2:
                        qx=floor((double)-dx*65536.0/384.0);
                        qy=floor((double)-dy*65536.0/256.0); break;
                    default:
                        qx=floor((double)-dy*65536.0/384.0);
                        qy=floor((double)dx*65536.0/256.0); break;
                    }
                    if(qx>=0 && qy>=0) { sx=(int32_t)(qx/65536.0); sy=(int32_t)(qy/65536.0); }
                    unsigned i=(unsigned)(yy-20)*128U+(unsigned)(xx-10);
                    if((unsigned)sx<32U && (unsigned)sy<16U && !(sx&1))
                        assert(alpha_target[i]==background(i));
                    else if((unsigned)sx<32U && (unsigned)sy<16U)
                        assert(alpha_target[i]!=background(i));
                    else assert(alpha_target[i]==background(i));
                }
            lv_aic_yuv_image_destroy(image);
        }
        /* A non-zero pivot must use the same signed inverse phase as the GE
         * crop helper. This catches accidental use of absolute destination
         * coordinates in the mask tail. */
        image=lv_aic_yuv_image_create(&frame,retain,release,NULL); assert(image);
        d.src=lv_aic_yuv_image_source(image); d.bitmap_mask_src=&mask;
        d.scale_x=384; d.scale_y=256; d.rotation=900; d.pivot=(lv_point_t){4,2};
        task.area=(lv_area_t){64,64,95,79};
        lv_area_t pivot_transformed;
        lv_image_buf_get_transformed_area(&pivot_transformed,frame.width,frame.height,
                                          d.rotation,d.scale_x,d.scale_y,&d.pivot);
        lv_area_move(&pivot_transformed,task.area.x1,task.area.y1);
        task.clip_area=pivot_transformed; yuv_mask_model=true; reset();
        assert(lv_draw_aic_ge2d_yuv(&task)==1 && calls==1 && allocations==1 && frees==1);
        for(int yy=task.clip_area.y1; yy<=task.clip_area.y2; yy++)
            for(int xx=task.clip_area.x1; xx<=task.clip_area.x2; xx++) {
                int64_t dx=((int64_t)(xx-task.area.x1)-d.pivot.x)*256;
                int64_t dy=((int64_t)(yy-task.area.y1)-d.pivot.y)*256;
                int64_t qx=(int64_t)d.pivot.x*65536+(int64_t)floor((double)dy*65536.0/d.scale_x);
                int64_t qy=(int64_t)d.pivot.y*65536+(int64_t)floor((double)-dx*65536.0/d.scale_y);
                int32_t sx=qx<0 ? -1 : (int32_t)(qx>>16);
                int32_t sy=qy<0 ? -1 : (int32_t)(qy>>16);
                unsigned i=(unsigned)(yy-20)*128U+(unsigned)(xx-10);
                if((unsigned)sx<32U && (unsigned)sy<16U && !(sx&1))
                    assert(alpha_target[i]==background(i));
                else if((unsigned)sx<32U && (unsigned)sy<16U)
                    assert(alpha_target[i]!=background(i));
                else assert(alpha_target[i]==background(i));
            }
        lv_aic_yuv_image_destroy(image);
        /* Transformed repeated tiles share the GE cell geometry.  Compare a
         * masked pass with its opaque reference so overlaps and rotation
         * holes are checked against the final tile draw order. */
        const unsigned tile_angles[]={0,900,1800,2700};
        for(unsigned ti=0; ti<4; ti++) {
            image=lv_aic_yuv_image_create(&frame,retain,release,NULL); assert(image);
            d.src=lv_aic_yuv_image_source(image); d.bitmap_mask_src=NULL;
            d.tile=true; d.scale_x=384; d.scale_y=256; d.rotation=tile_angles[ti];
            d.pivot=(lv_point_t){16,8}; d.opa=255;
            task.area=task.clip_area=(lv_area_t){64,64,127,95};
            yuv_mask_model=false; reset();
            assert(lv_draw_aic_ge2d_yuv(&task)==1 && calls>=1 && allocations==1 && frees==1);
            memcpy(whole,alpha_target,sizeof whole);
            d.bitmap_mask_src=&mask; yuv_mask_model=true; reset();
            assert(lv_draw_aic_ge2d_yuv(&task)==1 && calls>=1 && allocations==1 && frees==1 &&
                   staging_cleans>=1 && invalidates==staging_cleans+2);
            for(int yy=task.area.y1; yy<=task.area.y2; yy++)
                for(int xx=task.area.x1; xx<=task.area.x2; xx++) {
                    int32_t sx=-1,sy=-1;
                    bool source=yuv_mask_source_at(&task,&d,&frame,xx,yy,&sx,&sy);
                    unsigned i=(unsigned)(yy-20)*128U+(unsigned)(xx-10);
                    if(source && (sx&1)) assert(alpha_target[i]==whole[i]);
                    else assert(alpha_target[i]==background(i));
                }
            lv_aic_yuv_image_destroy(image); assert(!live);
        }
        d.bitmap_mask_src=NULL;
        d.scale_x=d.scale_y=LV_SCALE_NONE; yuv_mask_model=false;
        d.rotation=0;
    }
    /* YUV color-key extension: GE produces CSC pixels in the ARGB staging
     * surface, then the CPU tail applies LVGL's inclusive RGB range before
     * the native opaque-coverage blend.  The keyed source pixel must preserve
     * the destination, while a neighboring pixel is still rendered. */
    {
        uint32_t sample=raw_color(40,80,150);
        lv_color_t match=lv_color_make((sample>>16)&255U,(sample>>8)&255U,sample&255U);
        lv_image_colorkey_t key={.low=match,.high=match};
        frame.format=LV_COLOR_FORMAT_I420;
        image=lv_aic_yuv_image_create(&frame,retain,release,NULL); assert(image);
        d.src=lv_aic_yuv_image_source(image); d.colorkey=&key; d.opa=255;
        d.rotation=0; d.scale_x=d.scale_y=LV_SCALE_NONE; d.tile=false;
        task.area=task.clip_area=(lv_area_t){64,64,95,79}; reset();
        assert(lv_draw_aic_ge2d_yuv(&task)==1 && calls==1 && allocations==1 && frees==1);
        assert(alpha_target[44*128+54]==background(44*128+54));
        assert(alpha_target[44*128+55]==raw_color(42,80,150));
        /* Repeated tiles use the same post-CSC key semantics per tile. */
        d.tile=true; task.area=task.clip_area=(lv_area_t){64,64,127,95}; reset();
        assert(lv_draw_aic_ge2d_yuv(&task)==1 && calls==4 && allocations==1 && frees==1);
        assert(alpha_target[44*128+54]==background(44*128+54));
        assert(alpha_target[44*128+86]==background(44*128+86));
        lv_aic_yuv_image_destroy(image); assert(!live); d.colorkey=NULL; d.tile=false;
    }
    d.tile=false;d.rotation=0;d.pivot=(lv_point_t){0,0};d.scale_x=264;d.scale_y=256;d.opa=128;
    task.area=task.clip_area=(lv_area_t){64,64,95,79};
    image=lv_aic_yuv_image_create(&frame,retain,release,NULL);d.src=lv_aic_yuv_image_source(image);
    for(unsigned failure=0;failure<2;failure++) {
        reset();fail_alloc=failure==0;fail_address=failure==1;
        assert(lv_draw_aic_ge2d_yuv(&task)==0 && allocations==1 && !allocated && !calls && !src_caches && !dst_caches);
        for(unsigned i=0;i<128*128;i++) assert(alpha_target[i]==background(i));
        fail_alloc=fail_address=false;
    }
    reset();dst.header.flags|=LV_IMAGE_FLAGS_PREMULTIPLIED;
    assert(lv_draw_aic_ge2d_yuv(&task)==0 && !allocations && !calls);dst.header.flags&=~LV_IMAGE_FLAGS_PREMULTIPLIED;
    d.tile=true;task.area=task.clip_area=(lv_area_t){64,64,134,95};reset();
    assert(lv_draw_aic_ge2d_yuv(&task)==0 && !allocations && !calls && !dst_caches); /* last 7-pixel tile */
    d.tile=false;task.area=task.clip_area=(lv_area_t){64,64,95,79};
    retire_on_sync=image;reset();assert(lv_draw_aic_ge2d_yuv(&task)==1 && !live && target_cleans==1);
    for(int failure=1;failure<=7;failure++) {
        image=lv_aic_yuv_image_create(&frame,retain,release,NULL);d.src=lv_aic_yuv_image_source(image);
        d.tile=failure==7;task.area=task.clip_area=failure==7?(lv_area_t){64,64,127,95}:(lv_area_t){64,64,95,79};
        reset();fail_event=failure;
        assert(lv_draw_aic_ge2d_yuv(&task)==-1 && allocated && alpha_surface.data && event==failure);
        /* Earlier tiles and stripes of the failed draw may already have marked
         * their crops; the target stays untouched and each staging invalidate
         * pairs exactly one staging clean. The failed draw never finishes. */
        assert(invalidates==staging_cleans && !target_cleans && !frees);
        for(unsigned i=0;i<128*128;i++) assert(alpha_target[i]==background(i));
        assert(lv_draw_aic_ge2d_yuv(&task)==-1 && event==failure);
        lv_aic_yuv_image_destroy(image);assert(live==1);
        /* Only the CPU descriptor model has no pending DMA. */
        lv_aic_yuv_image_release_lease(quarantined);quarantined=NULL;
        lv_aic_ge2d_alpha_release(&alpha_surface);assert(!allocated && !live);fail_event=0;
    }
    printf("PASS ARGB YUV source retirement, allocation/address failure, late tile preflight, 7 DMA failures, retained planes/surface and untouched target\n");
    printf("PASS YUV inclusive color-key composition for direct and repeated-tile ARGB targets\n");
    printf("PASS %u straight-ARGB YUV scenes, %u analytic RGBA pixels, max error %u, cache handoff, exact source-Y split and source-X split max %u\n",scenes,pixels,worst,partial_worst);
    alpha_model=false;
#ifdef _WIN32
    assert(VirtualFree(arena,0,MEM_RELEASE));
#else
    assert(!munmap(arena,262144));
#endif
    arena=NULL;alpha_target=NULL;
}
