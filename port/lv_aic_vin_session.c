/* SPDX-License-Identifier: Apache-2.0 */
#include "lvgl_aic_feature_config.h"
#if defined(AIC_LVGL_USE_VIN) && AIC_LVGL_USE_VIN
#include "lv_aic_vin_session.h"
#include <string.h>
#include <limits.h>
/* SDK DVP configuration/queue are global even when contexts are distinct.
 * The owning application serializes this API and must exclude external VIN users. */
static lv_aic_vin_session_t *owner;

static int command(lv_aic_vin_session_t *s, int cmd, void *arg)
{ return mpp_vin2_ioctl(cmd,arg,s->channel,&s->device); }

bool lv_aic_vin_stop(lv_aic_vin_session_t *s)
{
    if (!s || s->held) return false;
    if (!s->streaming) return true;
    if (command(s,VIN_STREAM_OFF,NULL)<0) { s->faulted=true; return false; }
    s->streaming=false; s->paused=false;
    return true;
}
bool lv_aic_vin_close(lv_aic_vin_session_t *s)
{
    if (!s || !lv_aic_vin_stop(s)) return false;
    if (s->pool) mpp_vin2_vb_deinit(s->channel,&s->device);
    if (s->opened) mpp_vin2_deinit(&s->device);
    if (owner==s) owner=NULL;
    memset(s,0,sizeof(*s));
    return true;
}
bool lv_aic_vin_open(lv_aic_vin_session_t *s, const char *camera,
                     uint32_t channel, enum mpp_pixel_format format, uint32_t count)
{
    if (!s || owner || s->opened || s->pool || s->held || s->faulted || !camera || !camera[0] ||
        strlen(camera)>=sizeof(s->device.camera) || channel>=VIN_MAX_CHANNELS ||
        count<2 || count>VIN_MAX_BUF_NUM ||
        (format!=MPP_FMT_NV12 && format!=MPP_FMT_NV16 && format!=MPP_FMT_YUV400)) return false;
#ifndef AIC_DVP_SUPPORT_DEMUX
    if (channel!=0) return false;
#endif
    memset(s,0,sizeof(*s)); s->channel=channel;
    memcpy(s->device.camera,camera,strlen(camera)+1); s->device.type=VIN_DEV_DVP;
    owner=s;
    if (mpp_vin2_init(&s->device)<0) { owner=NULL; return false; }
    s->opened=true;
    struct mpp_video_fmt input={0};
    if (command(s,VIN_IN_G_FMT,&input)<0 || !input.width || !input.height ||
        input.width>4096 || input.height>4096 || (uint64_t)input.width*input.height>8U*1024U*1024U ||
        command(s,VIN_IN_S_FMT,&input)<0) goto fail;
    s->device.src_fmt=input;
    struct vin_video_fmt output={0};
    output.width=input.width; output.height=input.height; output.pixelformat=format;
    output.num_planes=2; output.stitch_mode=MPP_STITCH_INVALID;
    if (command(s,VIN_OUT_S_FMT,&output)<0 || output.num_planes!=2 ||
        output.width!=input.width || output.height!=input.height || output.pixelformat!=(uint32_t)format ||
        (format!=MPP_FMT_YUV400 && (output.width&1)) ||
        (format==MPP_FMT_NV12 && (output.height&1))) goto fail;
    uint64_t bytes=0;
    for (unsigned p=0;p<2;p++) {
        uint32_t rows=p && format==MPP_FMT_NV12 ? output.height/2 : output.height;
        if (p && format==MPP_FMT_YUV400) rows=0;
        uint64_t minimum=(uint64_t)output.plane_fmt[p].bytesperline*rows;
        if (rows && (output.plane_fmt[p].bytesperline<output.width ||
                     minimum>output.plane_fmt[p].sizeimage)) goto fail;
        bytes+=output.plane_fmt[p].sizeimage;
    }
    /* mpp_vin2_req_buf multiplies framesize by count in u32. Derive size
     * from negotiated padded planes, not width*height or caller estimates. */
    if (!bytes || bytes*count>UINT32_MAX-4096U) goto fail;
    output.framesize=(uint32_t)bytes; s->device.dst_fmt=output;
    if (mpp_vin2_vb_init(channel,&s->device)<0) goto fail;
    s->pool=true; s->buffers.num_buffers=count;
    if (command(s,VIN_REQ_BUF,&s->buffers)<0 || s->buffers.num_planes!=2 ||
        s->buffers.num_buffers<2 || s->buffers.num_buffers>count) goto fail;
    for (unsigned i=0;i<s->buffers.num_buffers*2;i++) {
        uint32_t needed=output.plane_fmt[i%2].sizeimage;
        struct vin_video_plane *p=&s->buffers.planes[i];
        if (p->len<0 || (uint32_t)p->len<needed ||
            (needed && (!(uint32_t)p->buf || (uint64_t)(uint32_t)p->buf+needed>UINT64_C(0x100000000)))) goto fail;
    }
    return true;
fail:
    lv_aic_vin_close(s);
    return false;
}
bool lv_aic_vin_start(lv_aic_vin_session_t *s)
{
    if (!s || !s->pool || s->held || s->faulted || s->streaming) return false;
    for (unsigned i=0;i<s->buffers.num_buffers;i++) {
        if (command(s,VIN_Q_BUF,(void *)(uintptr_t)i)<0) { s->faulted=true; return false; }
    }
    /* STREAM_ON can start the sensor before reporting a DVP failure. */
    s->streaming=true;
    if (command(s,VIN_STREAM_ON,NULL)<0) { s->faulted=true; return false; }
    return true;
}
bool lv_aic_vin_pause(lv_aic_vin_session_t *s)
{
    if (!s || !s->streaming || s->paused || s->faulted) return false;
    if (command(s,VIN_STREAM_PAUSE,NULL)<0) { s->faulted=true; return false; }
    s->paused=true; return true;
}
bool lv_aic_vin_resume(lv_aic_vin_session_t *s)
{
    if (!s || !s->streaming || !s->paused || s->faulted) return false;
    if (command(s,VIN_STREAM_RESUME,NULL)<0) { s->faulted=true; return false; }
    s->paused=false; return true;
}
bool lv_aic_vin_acquire(lv_aic_vin_session_t *s, uint32_t *index)
{
    uint32_t next=UINT32_MAX;
    if (!s || !index || !s->streaming || s->paused || s->faulted) return false;
    if (command(s,VIN_DQ_BUF,&next)<0) return false; /* No frame / timeout. */
    if (next>=s->buffers.num_buffers || (s->held&(1U<<next))) { s->faulted=true; return false; }
    s->held|=1U<<next; *index=next; return true;
}
bool lv_aic_vin_release(lv_aic_vin_session_t *s, uint32_t index)
{
    if (!s || index>=s->buffers.num_buffers || !(s->held&(1U<<index))) return false;
    if (command(s,VIN_Q_BUF,(void *)(uintptr_t)index)<0) { s->faulted=true; return false; }
    s->held&=~(1U<<index); return true;
}
#endif
