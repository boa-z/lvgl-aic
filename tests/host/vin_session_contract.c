/* SPDX-License-Identifier: Apache-2.0 */
#include <assert.h>
#include <string.h>
#include "lv_aic_vin_session.h"
static int fail_cmd, fail_init, fail_pool, closes, pool_closes, queues, off_calls;
static unsigned next_index, bad_buffers;
static int sequence, off_sequence, free_sequence;
int mpp_vin2_init(struct vin_dev_ctx *ctx)
{ if(fail_init) return -1; ctx->state=VIN_STATE_READY; return 0; }
void mpp_vin2_deinit(struct vin_dev_ctx *ctx)
{ closes++; free_sequence=++sequence; ctx->state=VIN_STATE_INIT; }
int mpp_vin2_vb_init(u32 ch,struct vin_dev_ctx *ctx)
{ (void)ch; (void)ctx; return fail_pool ? -1 : 0; }
void mpp_vin2_vb_deinit(u32 ch,struct vin_dev_ctx *ctx)
{ (void)ch; (void)ctx; pool_closes++; }
int mpp_vin2_ioctl(int cmd,void *arg,u32 ch,struct vin_dev_ctx *ctx)
{
    assert(ch==0);
    if(cmd==(int)VIN_STREAM_OFF) { off_calls++; off_sequence=++sequence; }
    if(cmd==fail_cmd) return -1;
    switch(cmd) {
    case VIN_IN_G_FMT: {
        struct mpp_video_fmt *f=arg; f->width=34; f->height=16; break;
    }
    case VIN_OUT_S_FMT: {
        struct vin_video_fmt *f=arg;
        f->plane_fmt[0].bytesperline=40; f->plane_fmt[0].sizeimage=640;
        f->plane_fmt[1].bytesperline=f->pixelformat==MPP_FMT_YUV400 ? 0 : 40;
        f->plane_fmt[1].sizeimage=f->pixelformat==MPP_FMT_YUV400 ? 0 :
                                   f->pixelformat==MPP_FMT_NV12 ? 320 : 640;
        break;
    }
    case VIN_REQ_BUF: {
        struct vin_video_buf *b=arg;
        uint32_t stride=ctx->dst_fmt.plane_fmt[0].sizeimage+ctx->dst_fmt.plane_fmt[1].sizeimage;
        assert(ctx->dst_fmt.framesize==stride);
        b->num_planes=2;
        for(unsigned i=0;i<b->num_buffers;i++) {
            b->planes[i*2].buf=0x40000000+i*stride;
            b->planes[i*2].len=ctx->dst_fmt.plane_fmt[0].sizeimage;
            b->planes[i*2+1].buf=b->planes[i*2].buf+b->planes[i*2].len;
            b->planes[i*2+1].len=ctx->dst_fmt.plane_fmt[1].sizeimage;
        }
        if(bad_buffers) b->planes[0].len=1;
        break;
    }
    case VIN_Q_BUF: assert((uintptr_t)arg<3); queues++; break;
    case VIN_DQ_BUF: *(uint32_t *)arg=next_index; break;
    default: break;
    }
    return 0;
}
static void opened(lv_aic_vin_session_t *s)
{ assert(lv_aic_vin_open(s,"camera",0,MPP_FMT_NV12,3)); }
int main(void)
{
    lv_aic_vin_session_t s={0};
    assert(!lv_aic_vin_open(&s,"camera",1,MPP_FMT_NV12,3));
    assert(!lv_aic_vin_open(&s,"camera",0,MPP_FMT_NV12,9));
    assert(!lv_aic_vin_open(&s,"camera",0,MPP_FMT_RGB_888,3));
    const enum mpp_pixel_format formats[]={MPP_FMT_NV12,MPP_FMT_NV16,MPP_FMT_YUV400};
    for(unsigned f=0;f<3;f++) {
        assert(lv_aic_vin_open(&s,"camera",0,formats[f],3));
        lv_aic_vin_session_t other={0};
        assert(!lv_aic_vin_open(&other,"camera",0,formats[f],3));
        assert(!lv_aic_vin_open(&s,"camera",0,formats[f],3));
        assert(lv_aic_vin_start(&s));
        uint32_t index=99;
        assert(lv_aic_vin_acquire(&s,&index) && index==0);
        int before=closes;
        assert(!lv_aic_vin_close(&s) && closes==before);
        assert(!lv_aic_vin_stop(&s));
        assert(lv_aic_vin_pause(&s));
        assert(!lv_aic_vin_acquire(&s,&index));
        assert(lv_aic_vin_resume(&s));
        assert(lv_aic_vin_release(&s,index));
        assert(!lv_aic_vin_release(&s,index));
        assert(lv_aic_vin_stop(&s));
        assert(lv_aic_vin_start(&s));
        assert(lv_aic_vin_close(&s) && free_sequence>off_sequence);
    }
    const int setup_failures[]={VIN_IN_G_FMT,VIN_IN_S_FMT,VIN_OUT_S_FMT,VIN_REQ_BUF};
    for(unsigned i=0;i<4;i++) {
        fail_cmd=setup_failures[i]; int before=closes;
        assert(!lv_aic_vin_open(&s,"camera",0,MPP_FMT_NV12,3));
        assert(!s.opened && closes==before+1);
    }
    fail_cmd=0; bad_buffers=1;
    assert(!lv_aic_vin_open(&s,"camera",0,MPP_FMT_NV12,3) && !s.opened);
    bad_buffers=0; fail_init=1;
    assert(!lv_aic_vin_open(&s,"camera",0,MPP_FMT_NV12,3)); fail_init=0;
    fail_pool=1; assert(!lv_aic_vin_open(&s,"camera",0,MPP_FMT_NV12,3)); fail_pool=0;
    opened(&s); fail_cmd=VIN_STREAM_ON;
    assert(!lv_aic_vin_start(&s) && s.streaming && s.faulted);
    fail_cmd=VIN_STREAM_OFF; int before=closes;
    assert(!lv_aic_vin_close(&s) && closes==before && s.opened);
    fail_cmd=0; assert(lv_aic_vin_close(&s));
    opened(&s); assert(lv_aic_vin_start(&s)); uint32_t index=99;
    fail_cmd=VIN_DQ_BUF; assert(!lv_aic_vin_acquire(&s,&index) && index==99 && !s.faulted);
    fail_cmd=0; assert(lv_aic_vin_acquire(&s,&index));
    fail_cmd=VIN_Q_BUF; assert(!lv_aic_vin_release(&s,index) && s.held==1);
    assert(!lv_aic_vin_close(&s));
    fail_cmd=0; assert(lv_aic_vin_release(&s,index)); assert(lv_aic_vin_close(&s));
    opened(&s); assert(lv_aic_vin_start(&s));
    next_index=9; assert(!lv_aic_vin_acquire(&s,&index) && s.faulted);
    assert(lv_aic_vin_close(&s));
    opened(&s); assert(lv_aic_vin_start(&s)); next_index=0;
    assert(lv_aic_vin_acquire(&s,&index));
    assert(!lv_aic_vin_acquire(&s,&index) && s.faulted);
    assert(!lv_aic_vin_close(&s));
    assert(lv_aic_vin_release(&s,0)); assert(lv_aic_vin_close(&s));
    assert(pool_closes>0 && off_calls>0 && queues>0);
    opened(&s); fail_cmd=VIN_Q_BUF;
    assert(!lv_aic_vin_start(&s) && s.faulted && !s.streaming);
    fail_cmd=0; assert(lv_aic_vin_close(&s));
    opened(&s); assert(lv_aic_vin_start(&s)); fail_cmd=VIN_STREAM_PAUSE;
    assert(!lv_aic_vin_pause(&s) && s.faulted);
    fail_cmd=0; assert(lv_aic_vin_close(&s));
    opened(&s); assert(lv_aic_vin_start(&s)); assert(lv_aic_vin_pause(&s));
    fail_cmd=VIN_STREAM_RESUME; assert(!lv_aic_vin_resume(&s) && s.faulted);
    fail_cmd=0; assert(lv_aic_vin_close(&s));
    return 0;
}
