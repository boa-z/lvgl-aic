/* SPDX-License-Identifier: Apache-2.0 */
#define AIC_LVGL_USE_PRIVATE_API 1
#include "lv_aic_rgb_mpp.h"
#include "lv_aic_rgb_image_private.h"
#include "lvgl_aic_private.h"
#include <assert.h>
#include <string.h>
static int retained,released;
static bool retain(void *p) { (void)p; retained++; return true; }
static void release(void *p) { (void)p; released++; }
static void reject(const struct mpp_buf *b,size_t capacity)
{
    lv_aic_rgb_frame_t f,saved; memset(&f,0xa5,sizeof(f)); saved=f;
    assert(!lv_aic_rgb_from_mpp(b,capacity,&f) && !memcmp(&f,&saved,sizeof(f)));
}
int main(void)
{
    const enum mpp_pixel_format formats[]={MPP_FMT_RGB_565,MPP_FMT_RGB_888,MPP_FMT_ARGB_8888,MPP_FMT_XRGB_8888};
    for(unsigned i=0;i<4;i++) {
        unsigned bytes=i==0?2:i==1?3:4;
        struct mpp_buf b={.buf_type=MPP_PHY_ADDR,.format=formats[i],.size={8,4},
            .phy_addr={0x42000000},.stride={64},.crop_en=1,.crop={1,1,7,3}};
        lv_aic_rgb_frame_t f;
        assert(lv_aic_rgb_from_mpp(&b,256,&f));
        assert(f.width==7 && f.height==3 && f.stride==64 && f.capacity==256-64-bytes);
        assert((uintptr_t)f.data==0x42000000+64+bytes);
        reject(&b,192+8*bytes-1);
        b.crop.x=-1; reject(&b,256); b.crop.x=2; reject(&b,256); b.crop.x=1;
        b.phy_addr[0]=0xfffffff0; reject(&b,256); b.phy_addr[0]=0x42000000;
        b.buf_type=MPP_DMA_BUF_FD; reject(&b,256); b.buf_type=MPP_PHY_ADDR;
        b.format=MPP_FMT_BGR_888; reject(&b,256);
    }
    lv_init(); assert(lv_aic_rgb_image_decoder_init());
    uint32_t pixels[12];
    for(unsigned i=0;i<12;i++) pixels[i]=0x803264c8;
    lv_aic_rgb_frame_t f={LV_COLOR_FORMAT_ARGB8888,4,2,24,(const uint8_t *)pixels,sizeof(pixels)};
    lv_aic_rgb_image_t *image=lv_aic_rgb_image_create(&f,retain,release,NULL); assert(image);
    const lv_image_dsc_t *source=lv_aic_rgb_image_source(image);
    lv_image_decoder_args_t args={.stride_align=false,.premultiply=false};
    lv_image_decoder_dsc_t a,b,premul;
    assert(lv_image_decoder_open(&a,source,&args)==LV_RESULT_OK);
    assert(a.decoded->data==(const uint8_t *)pixels && !a.cache_entry);
    assert(lv_image_decoder_open(&b,source,&args)==LV_RESULT_OK && a.decoded==b.decoded);
    args.premultiply=true; args.stride_align=true;
    assert(lv_image_decoder_open(&premul,source,&args)==LV_RESULT_OK);
    assert(premul.decoded->data!=(const uint8_t *)pixels && premul.decoded->data[0]==100);
    assert(pixels[0]==0x803264c8 && (premul.decoded->header.flags&LV_IMAGE_FLAGS_PREMULTIPLIED));
    assert(!(premul.decoded->header.flags&LV_IMAGE_FLAGS_MODIFIABLE));
    const lv_aic_rgb_frame_t *view;
    lv_aic_rgb_image_t *lease=lv_aic_rgb_image_acquire(source,&view); assert(lease && view->capacity==48);
    lv_aic_rgb_image_destroy(image);
    lv_image_decoder_dsc_t fail;
    assert(lv_image_decoder_open(&fail,source,&args)==LV_RESULT_INVALID);
    lv_image_decoder_close(&a); lv_image_decoder_close(&b); lv_image_decoder_close(&premul);
    assert(!released && !lv_aic_rgb_image_decoder_deinit());
    lv_aic_rgb_image_release_lease(lease); assert(released==1);
    /* Last visible row without padding: snapshot never reads past capacity. */
    f.capacity=40; image=lv_aic_rgb_image_create(&f,retain,release,NULL); assert(image);
    args.premultiply=false; args.stride_align=false;
    assert(lv_image_decoder_open(&a,lv_aic_rgb_image_source(image),&args)==LV_RESULT_OK);
    assert(a.decoded->data!=(const uint8_t *)pixels && a.decoded->header.stride==24);
    assert(!memcmp(a.decoded->data,pixels,16) && !memcmp(a.decoded->data+24,pixels+6,16));
    for(unsigned i=16;i<24;i++) assert(a.decoded->data[i]==0 && a.decoded->data[i+24]==0);
    lv_image_decoder_close(&a); lv_aic_rgb_image_destroy(image);
    f.capacity=39; assert(!lv_aic_rgb_image_create(&f,retain,release,NULL));
    f.stride=17; f.capacity=33;
    image=lv_aic_rgb_image_create(&f,retain,release,NULL); assert(image);
    assert(lv_image_decoder_open(&a,lv_aic_rgb_image_source(image),&args)==LV_RESULT_OK);
    assert(a.decoded->data!=(const uint8_t *)pixels && !(a.decoded->header.stride%4));
    lv_image_decoder_close(&a); lv_aic_rgb_image_destroy(image);
    assert(retained==released && lv_aic_rgb_image_decoder_deinit()); lv_deinit(); return 0;
}
