/* SPDX-License-Identifier: Apache-2.0
 * GE v1.1 YUV bitblt admission: SDK ge_bitblt() admits an RGB destination and
 * a YUV400 source only, so lv_draw_aic_ge2d_yuv must decline every other YUV
 * layout before cache handoff or submission. The mock mirrors that gate: a
 * command that reaches it with a non-admitted pair counts as a rejection and
 * fails the call, exactly like the board, so a missing local gate would both
 * trip the counter and quarantine the port. */
#define AIC_LVGL_USE_PRIVATE_API 1
#include <assert.h>
#include <stdint.h>
#include <string.h>
#include "../../draw/ge2d/lv_draw_aic_ge2d_yuv.c"
#include "../../include/lv_aic_yuv_layer.h"

static unsigned allocations,frees,retains,releases;
static unsigned submissions,rejections,emits,syncs,src_caches,dst_caches;
static struct ge_bitblt last_blt;

void *aicos_malloc_align(unsigned int type,size_t bytes,size_t align)
{ (void)type;(void)bytes;(void)align;allocations++;return (void *)(uintptr_t)0x48000000; }
void aicos_free_align(unsigned int type,void *p)
{ (void)type;(void)p;frees++; }
void aicos_dcache_invalid_range(unsigned long *p,unsigned long size)
{ (void)p;(void)size; }

struct mpp_ge *lv_draw_aic_ge2d_device(void) { return (void *)(uintptr_t)1; }
bool lv_draw_aic_ge2d_buf_address_valid(const lv_draw_buf_t *b)
{ return b && b->data && (uintptr_t)b->data>=0x40000000; }
void lv_draw_aic_ge2d_prepare_yuv_cache(const lv_aic_yuv_frame_t *frame)
{ (void)frame;src_caches++; }
void lv_draw_aic_ge2d_prepare_dst_cache(const lv_draw_buf_t *b,const lv_area_t *a)
{ (void)b;(void)a;dst_caches++; }
void lv_draw_aic_ge2d_prepare_src_cache(const lv_draw_buf_t *b,const lv_area_t *a)
{ (void)b;(void)a;assert(!"src cache prepare must not run for declined sources"); }

static bool admitted(const struct ge_bitblt *b)
{
    return (b->src_buf.format==MPP_FMT_YUV400 ||
            (b->src_buf.format>=MPP_FMT_ARGB_8888 && b->src_buf.format<=MPP_FMT_BGRA_4444)) &&
           b->dst_buf.format>=MPP_FMT_ARGB_8888 && b->dst_buf.format<=MPP_FMT_BGRA_4444;
}
int mpp_ge_bitblt(struct mpp_ge *ge,struct ge_bitblt *b)
{ (void)ge;if(!admitted(b)) { rejections++;return -1; }last_blt=*b;submissions++;return 0; }
int mpp_ge_emit(struct mpp_ge *ge) { (void)ge;emits++;return 0; }
int mpp_ge_sync(struct mpp_ge *ge) { (void)ge;syncs++;return 0; }

static bool retain(void *context) { (void)context;retains++;return true; }
static void release(void *context) { (void)context;releases++; }
static void reset(void)
{ submissions=rejections=emits=syncs=src_caches=dst_caches=allocations=frees=0; }

int main(void)
{
    lv_init();
    assert(lv_aic_yuv_image_decoder_init());
    lv_aic_yuv_frame_t frame={0};
    frame.width=32; frame.height=16; frame.color_space=LV_AIC_YUV_BT709_FULL;
    for(unsigned p=0;p<3;p++)
        frame.planes[p]=(lv_aic_yuv_plane_t){(void *)(uintptr_t)(0x40001000+p*4096),64,2048};
    lv_draw_buf_t dst;
    assert(lv_draw_buf_init(&dst,64,64,LV_COLOR_FORMAT_RGB888,192,
                            (void *)(uintptr_t)0x41000000,64*192)==LV_RESULT_OK);
    lv_layer_t layer={0};
    layer.draw_buf=&dst; layer.buf_area=(lv_area_t){0,0,63,63};
    lv_draw_image_dsc_t d;
    lv_draw_image_dsc_init(&d);
    lv_draw_task_t task={0};
    task.type=LV_DRAW_TASK_TYPE_IMAGE; task.draw_dsc=&d; task.target_layer=&layer;
    task.area=(lv_area_t){16,16,47,31}; task.clip_area=layer.buf_area;
    lv_aic_yuv_image_t *image=NULL;

    /* Planar, semiplanar and packed non-YUV400 layouts all decline with no
     * engine call, no cache prepare, no allocation and no quarantine. */
    const lv_aic_yuv_format_t declined[]={LV_COLOR_FORMAT_I420,LV_COLOR_FORMAT_NV12,
                                           LV_COLOR_FORMAT_YUY2,LV_AIC_YUV_NV16};
    for(unsigned f=0;f<sizeof(declined)/sizeof(declined[0]);f++) {
        frame.format=declined[f];
        image=lv_aic_yuv_image_create(&frame,retain,release,NULL); assert(image);
        d.src=lv_aic_yuv_image_source(image);
        reset();
        assert(lv_draw_aic_ge2d_yuv(&task)==0);
        assert(!submissions && !rejections && !emits && !syncs && !src_caches && !dst_caches);
        assert(!allocations && !lv_draw_aic_ge2d_yuv_faulted() && retains==releases+1);
        lv_aic_yuv_image_destroy(image); image=NULL;
        assert(retains==releases);
    }

    /* The tiled preflight pass must decline too: no partial tile writes. */
    frame.format=LV_COLOR_FORMAT_I420;
    image=lv_aic_yuv_image_create(&frame,retain,release,NULL); assert(image);
    d.src=lv_aic_yuv_image_source(image);
    d.tile=1; task.area=(lv_area_t){16,16,79,47};
    reset();
    assert(lv_draw_aic_ge2d_yuv(&task)==0);
    assert(!submissions && !rejections && !src_caches && !dst_caches &&
           !lv_draw_aic_ge2d_yuv_faulted());
    lv_aic_yuv_image_destroy(image); image=NULL;
    assert(retains==releases);
    d.tile=0; task.area=(lv_area_t){16,16,47,31};

    /* ARGB targets stage through CMA; the decline must precede the alloc. */
    lv_draw_buf_t argb;
    assert(lv_draw_buf_init(&argb,64,64,LV_COLOR_FORMAT_ARGB8888,256,
                            (void *)(uintptr_t)0x42000000,64*256)==LV_RESULT_OK);
    lv_layer_t alayer={0};
    alayer.draw_buf=&argb; alayer.color_format=LV_COLOR_FORMAT_ARGB8888;
    alayer.buf_area=(lv_area_t){0,0,63,63};
    image=lv_aic_yuv_image_create(&frame,retain,release,NULL); assert(image);
    d.src=lv_aic_yuv_image_source(image); d.opa=128; task.target_layer=&alayer;
    reset();
    assert(lv_draw_aic_ge2d_yuv(&task)==0);
    assert(!allocations && !frees && !submissions && !rejections && !src_caches && !dst_caches &&
           !lv_draw_aic_ge2d_yuv_faulted());
    lv_aic_yuv_image_destroy(image); image=NULL;
    assert(retains==releases);
    task.target_layer=&layer; d.opa=LV_OPA_COVER;

    /* YUV400 is what the v1.1 gate admits, so it still reaches the engine. */
    frame.format=LV_COLOR_FORMAT_I400;
    image=lv_aic_yuv_image_create(&frame,retain,release,NULL); assert(image);
    d.src=lv_aic_yuv_image_source(image);
    reset();
    assert(lv_draw_aic_ge2d_yuv(&task)==1);
    assert(submissions==1 && !rejections && emits==1 && syncs==1 && src_caches==1 && dst_caches==1);
    lv_aic_yuv_image_destroy(image); image=NULL;
    assert(retains==releases && !lv_draw_aic_ge2d_yuv_faulted());

    /* Board run 10: the manual I400 alpha probe rotated a narrow strip at 2x.
     * The strip's inverse-mapped source window is 4 rows plus filter taps and
     * the SDK rejects a YUV source crop below 8x8 (check_blit: "the min size
     * of yuv is 8x8"), so the 8-pixel strip must decline before any staging
     * allocation, cache handoff or engine command. The widened strip clears
     * the minimum and must submit with the exact crop and phase the board
     * probe now relies on. */
    image=lv_aic_yuv_image_create(&frame,retain,release,NULL); assert(image);
    d.src=lv_aic_yuv_image_source(image);
    d.pivot=(lv_point_t){16,8}; d.rotation=900; d.scale_x=d.scale_y=512; d.opa=128;
    task.area=(lv_area_t){24,24,55,39};
    task.target_layer=&alayer; task.clip_area=(lv_area_t){36,24,43,39};
    reset();
    assert(lv_draw_aic_ge2d_yuv(&task)==0);
    assert(!allocations && !frees && !submissions && !rejections && !emits && !syncs &&
           !src_caches && !dst_caches && !lv_draw_aic_ge2d_yuv_faulted());
    task.target_layer=&layer; task.clip_area=(lv_area_t){36,24,51,39};
    reset();
    assert(lv_draw_aic_ge2d_yuv(&task)==1);
    assert(submissions==1 && !rejections && emits==1 && syncs==1 && src_caches==1 && dst_caches==1);
    assert(last_blt.ctrl.flags==MPP_ROTATION_90 && last_blt.ctrl.src_global_alpha==128);
    assert(last_blt.src_buf.crop.x==12 && last_blt.src_buf.crop.y==2 &&
           last_blt.src_buf.crop.width==10 && last_blt.src_buf.crop.height==10);
    assert(last_blt.dst_buf.crop.x==36 && last_blt.dst_buf.crop.y==24 &&
           last_blt.dst_buf.crop.width==16 && last_blt.dst_buf.crop.height==16);
    assert(last_blt.scale_phase.dx_16[0]==32768 && last_blt.scale_phase.dy_16[0]==32768);
    assert(last_blt.scale_phase.h_phase_16[0]==0 && last_blt.scale_phase.v_phase_16[0]==32768);
    lv_aic_yuv_image_destroy(image); image=NULL;
    assert(retains==releases && !lv_draw_aic_ge2d_yuv_faulted());

    assert(lv_aic_yuv_image_decoder_deinit());
    lv_deinit();
    return 0;
}
