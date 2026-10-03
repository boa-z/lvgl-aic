/* SPDX-License-Identifier: Apache-2.0 */
#include "lvgl_aic_feature_config.h"
#if defined(AIC_LVGL_USE_APNG) && AIC_LVGL_USE_APNG
#include "lv_aic_apng_decoder.h"
#include "lv_aic_apng.h"
#include "lv_aic_player_allocator.h"
#include <mpp_decoder.h>
#include <limits.h>
#include <stdlib.h>
#include <string.h>
struct lv_aic_apng_decoder {
    lv_aic_player_allocator_t *allocator;
    struct mpp_decoder *decoder;
    struct mpp_frame frame;
    size_t packet_limit;
    uint64_t ticket;
    bool held;
};
static bool span(const void *p,size_t n)
{ return p && n && n-1<=UINTPTR_MAX-(uintptr_t)p; }
static bool overlap(const void *a,size_t an,const void *b,size_t bn)
{
    uintptr_t x=(uintptr_t)a,y=(uintptr_t)b;
    return x<=y?y-x<an:x-y<bn;
}
lv_aic_apng_decoder_t *lv_aic_apng_decoder_create(size_t budget,size_t limit)
{
    if(!budget || limit<256 || limit>INT_MAX-255U) return NULL;
    lv_aic_apng_decoder_t *d=calloc(1,sizeof(*d));
    if(!d) return NULL;
    d->allocator=lv_aic_player_allocator_create(budget);
    if(!d->allocator) { free(d); return NULL; }
    d->packet_limit=limit; return d;
}
bool lv_aic_apng_decoder_close(lv_aic_apng_decoder_t *d)
{
    if(!d) return true;
    if(d->ticket) {
        if(!lv_aic_player_allocator_release(d->allocator,d->ticket)) return false;
        d->ticket=0;
    }
    if(d->held) {
        if(mpp_decoder_put_frame(d->decoder,&d->frame)) return false;
        d->held=false;
    }
    if(d->decoder) { mpp_decoder_destory(d->decoder); d->decoder=NULL; }
    return true;
}
bool lv_aic_apng_decoder_destroy(lv_aic_apng_decoder_t *d)
{
    if(!d) return true;
    if(!lv_aic_apng_decoder_close(d) || !lv_aic_player_allocator_destroy(d->allocator)) return false;
    free(d); return true;
}
bool lv_aic_apng_decoder_decode(lv_aic_apng_decoder_t *d,const void *png,size_t bytes,
    uint32_t width,uint32_t height,void *rgba,size_t stride,size_t capacity)
{
    if(!d || d->decoder || !width || width>4096 || !height || height>4096 ||
       bytes>d->packet_limit || bytes>INT_MAX-255U || !span(png,bytes) ||
       stride<(size_t)width*4 ||
       (height>1 && stride>(SIZE_MAX-(size_t)width*4)/(height-1)) ||
       (size_t)(height-1)*stride+(size_t)width*4>capacity ||
       !span(rgba,capacity) || overlap(png,bytes,rgba,capacity)) return false;
    size_t packet_bytes=(bytes+255)&~(size_t)255;
    if(packet_bytes>d->packet_limit) return false;
    lv_aic_apng_limits_t limits={d->packet_limit,d->packet_limit,4096U*4096U,1};
    lv_aic_apng_t doc;
    if(!lv_aic_apng_open(png,bytes,&limits,&doc) || doc.animated ||
       doc.width!=width || doc.height!=height) return false;
    d->decoder=mpp_decoder_create(MPP_CODEC_VIDEO_DECODER_PNG);
    if(!d->decoder) return false;
    struct decode_config config={.pix_fmt=MPP_FMT_ARGB_8888,
        .bitstream_buffer_size=(int)packet_bytes,.packet_count=1,.extra_frame_num=0};
    struct mpp_packet packet={0};
    bool copied=false;
    if(mpp_decoder_control(d->decoder,MPP_DEC_INIT_CMD_SET_EXT_FRAME_ALLOCATOR,
            lv_aic_player_allocator_sdk(d->allocator)) ||
       mpp_decoder_init(d->decoder,&config) ||
       mpp_decoder_get_packet(d->decoder,&packet,(int)bytes)) goto done;
    if(!packet.data || packet.size<(int)bytes) goto done;
    memcpy(packet.data,png,bytes);
    packet.size=packet.len=(int)bytes; packet.flag=PACKET_FLAG_EOS;
    if(mpp_decoder_put_packet(d->decoder,&packet) || mpp_decoder_decode(d->decoder) ||
       mpp_decoder_get_frame(d->decoder,&d->frame)) goto done;
    d->held=true;
    const struct mpp_buf *b=&d->frame.buf;
    size_t sizes[3];
    if((d->frame.flags&FRAME_FLAG_ERROR) || b->format!=MPP_FMT_ARGB_8888 ||
       b->size.width!=(int)width || b->size.height!=(int)height ||
       (b->crop_en && (b->crop.x || b->crop.y || b->crop.width!=(int)width ||
                      b->crop.height!=(int)height)) ||
       !lv_aic_player_allocator_acquire(d->allocator,&d->frame,sizes,&d->ticket)) goto done;
    const uint8_t *source=(const void *)(uintptr_t)b->phy_addr[0];
    uint64_t used=(uint64_t)(height-1)*b->stride[0]+(uint64_t)width*4;
    if(b->stride[0]<width*4 || used>sizes[0] || !span(source,sizes[0]) ||
       overlap(source,sizes[0],rgba,capacity)) goto done;
    /* Existing AIC/LVGL ARGB contract: little-endian native BGRA bytes.
     * Convert explicitly into the compositor's portable RGBA byte order. */
    for(uint32_t y=0;y<height;y++) {
        const uint8_t *s=source+(size_t)y*b->stride[0];
        uint8_t *out=(uint8_t *)rgba+(size_t)y*stride;
        for(uint32_t x=0;x<width;x++,s+=4,out+=4) {
            out[0]=s[2]; out[1]=s[1]; out[2]=s[0]; out[3]=s[3];
        }
    }
    copied=true;
done:
    return lv_aic_apng_decoder_close(d) && copied;
}
#endif
