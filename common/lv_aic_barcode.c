/* SPDX-License-Identifier: Apache-2.0 */
#include "lv_aic_barcode.h"
#if defined(AIC_LVGL_USE_BARCODE) && AIC_LVGL_USE_BARCODE
#include <yydecoder.h>
#include <string.h>
static volatile unsigned busy;
static bool initialized;
lv_aic_barcode_result_t lv_aic_barcode_decode(const lv_aic_yuv_frame_t *frame,
    uint8_t *output,size_t capacity,size_t *length)
{
    if(length) *length=0;
    if(!length || !output || !capacity || !lv_aic_yuv_validate(frame) ||
       (frame->format!=LV_COLOR_FORMAT_I400 && frame->format!=LV_COLOR_FORMAT_NV12 &&
        frame->format!=LV_AIC_YUV_NV16) || (uint64_t)frame->width*frame->height>2U*1024U*1024U)
        return LV_AIC_BARCODE_INVALID;
    uintptr_t begin=(uintptr_t)output;
    if(capacity>UINTPTR_MAX-begin) return LV_AIC_BARCODE_INVALID;
    lv_aic_yuv_layout_t layout;
    lv_aic_yuv_layout(frame->format,frame->width,frame->height,&layout);
    for(unsigned i=0;i<layout.planes;i++) {
        uintptr_t source=(uintptr_t)frame->planes[i].data;
        size_t span=(size_t)(layout.rows[i]-1)*frame->planes[i].stride+layout.row_bytes[i];
        if(begin<source+span && source<begin+capacity) return LV_AIC_BARCODE_INVALID;
    }
    if(__sync_lock_test_and_set(&busy,1)) return LV_AIC_BARCODE_BUSY;
    lv_aic_barcode_result_t status=LV_AIC_BARCODE_NOMEM;
    uint8_t *gray=lv_malloc((size_t)frame->width*frame->height),*result=NULL;
    if(!gray) goto done;
    /* SDK takes tightly packed Y, while VIN rows may include padding. */
    for(uint32_t y=0;y<frame->height;y++)
        memcpy(gray+(size_t)y*frame->width,frame->planes[0].data+(size_t)y*frame->planes[0].stride,frame->width);
    if(!initialized) {
        if(Initial_Decoder()!=1) { status=LV_AIC_BARCODE_INIT_FAILED;goto done; }
        for(int type=0;type<14;type++) Set_Donfig_Decoder(type,1);
        initialized=true;
    }
    /* The SDK consumer uses the reported result length, not the undocumented
     * Decoding_Image return code, as its result-availability contract. */
    Decoding_Image(gray,frame->width,frame->height);
    unsigned count=GetResultLength();
    if(!count) { status=LV_AIC_BARCODE_EMPTY;goto done; }
    if(count>4096 || count>capacity) { status=LV_AIC_BARCODE_TOO_LONG;goto done; }
    result=lv_malloc((size_t)count+1); /* Also reserve a possible SDK terminator. */
    if(!result) goto done;
    GetDecoderResult(result);
    memcpy(output,result,count);*length=count;status=LV_AIC_BARCODE_OK;
done:
    lv_free(result);lv_free(gray);__sync_lock_release(&busy);return status;
}
#endif
