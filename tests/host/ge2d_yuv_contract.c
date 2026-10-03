/* SPDX-License-Identifier: Apache-2.0 */
#define AIC_LVGL_USE_PRIVATE_API 1
#include <assert.h>
#include <stdint.h>
#include <string.h>
#include "../../draw/ge2d/lv_draw_aic_ge2d_yuv.c"

static struct ge_bitblt captured;
static int calls, emits, syncs, src_caches, dst_caches, fail_at, live;
static lv_aic_yuv_image_t *retire_on_sync;
struct mpp_ge *lv_draw_aic_ge2d_device(void) { return (void *)(uintptr_t)1; }
bool lv_draw_aic_ge2d_buf_address_valid(const lv_draw_buf_t *b)
{ return b && b->data && (uintptr_t)b->data>=0x40000000; }
void lv_draw_aic_ge2d_prepare_yuv_cache(const lv_aic_yuv_frame_t *frame)
{ assert(lv_aic_yuv_validate(frame)); src_caches++; }
void lv_draw_aic_ge2d_prepare_dst_cache(const lv_draw_buf_t *buffer, const lv_area_t *area)
{ assert(buffer && area->x1>=0 && area->y1>=0); dst_caches++; }
int mpp_ge_bitblt(struct mpp_ge *ge, struct ge_bitblt *blt)
{ (void)ge; captured=*blt; calls++; return fail_at==1 ? -1 : 0; }
int mpp_ge_emit(struct mpp_ge *ge)
{ (void)ge; emits++; return fail_at==2 ? -1 : 0; }
int mpp_ge_sync(struct mpp_ge *ge)
{
    (void)ge; syncs++;
    if (retire_on_sync) {
        lv_aic_yuv_image_destroy(retire_on_sync); retire_on_sync=NULL;
        assert(live==1);
    }
    return fail_at==3 ? -1 : 0;
}
static bool retain(void *context) { (void)context; live++; return true; }
static void release(void *context) { (void)context; live--; }
static void reset(void) { calls=emits=syncs=src_caches=dst_caches=0; }
int main(void)
{
    const lv_color_format_t formats[]={LV_COLOR_FORMAT_I420,LV_COLOR_FORMAT_I422,
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
        d.rotation=900; reset();
        assert(lv_draw_aic_ge2d_yuv(&task)==0 && !calls && !src_caches);
        d.rotation=0; d.scale_x=d.scale_y=256; task.clip_area=layer.buf_area;
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
    d.scale_x=384; reset(); assert(lv_draw_aic_ge2d_yuv(&task)==0 && !calls);
    d.scale_x=256; d.tile=1; assert(lv_draw_aic_ge2d_yuv(&task)==0); d.tile=0;
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
    assert(lv_draw_aic_ge2d_yuv(&task)==1 && live==0);
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
    assert(lv_aic_yuv_image_decoder_deinit()); lv_deinit();
    return 0;
}
