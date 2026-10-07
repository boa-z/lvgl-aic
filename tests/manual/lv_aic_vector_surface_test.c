/* SPDX-License-Identifier: Apache-2.0 */
#define AIC_LVGL_USE_PRIVATE_API 1
#define LOG_TAG "lvgl.vector.surface"
#define LOG_LVL LOG_LVL_INFO
#include "lv_aic_manual_test.h"
#if LV_USE_VECTOR_GRAPHIC && AIC_LVGL_BSP_RTTHREAD
#include "lvgl_aic_private.h"
#include "lv_aic_test_log.h"

int lv_aic_vector_surface_test_run(void)
{
    const lv_color_format_t formats[]={LV_COLOR_FORMAT_ARGB8888,LV_COLOR_FORMAT_RGB565,
        LV_COLOR_FORMAT_RGB888,LV_COLOR_FORMAT_XRGB8888,LV_COLOR_FORMAT_ARGB8888_PREMULTIPLIED,LV_COLOR_FORMAT_ARGB8888,LV_COLOR_FORMAT_RGB565A8};
    const lv_vector_blend_t modes[]={LV_VECTOR_BLEND_SRC_OVER,LV_VECTOR_BLEND_MULTIPLY,
        LV_VECTOR_BLEND_SCREEN,LV_VECTOR_BLEND_DST_OVER,LV_VECTOR_BLEND_ADDITIVE,LV_VECTOR_BLEND_SUBTRACTIVE,LV_VECTOR_BLEND_SRC_IN,LV_VECTOR_BLEND_DST_IN,LV_VECTOR_BLEND_NONE};
    lv_obj_t *canvas=NULL;int result=-1;
    lv_draw_buf_t *buffer=NULL;
    uint8_t *before=NULL;
    for(unsigned kind=0;kind<7;kind++) for(unsigned mode=0;mode<9;mode++) for(unsigned paint=0;paint<5;paint++) {
        if((paint==3 && mode!=5) || (paint==4 && mode<6)) continue;
        canvas=lv_canvas_create(lv_screen_active());if(!canvas) goto done;
        lv_obj_set_hidden(canvas,true);
        buffer=lv_draw_buf_create(32,32,formats[kind],0);
        if(!buffer) goto done;
        if(kind==5) buffer->header.flags|=LV_IMAGE_FLAGS_PREMULTIPLIED;
        lv_canvas_set_draw_buf(canvas,buffer);
        before=lv_malloc(buffer->data_size);if(!before) goto done;
        unsigned bytes=(kind==1 || kind==6)?2:kind==2?3:4;
        const unsigned bg[]={48,96,160},source[]={32,64,192};
        unsigned da=(kind==0 || kind>=4)?128:255;
        lv_memset(buffer->data,0xa5,buffer->data_size);
        for(unsigned y=0;y<32;y++) for(unsigned x=0;x<32;x++) {
            uint8_t *p=buffer->data+y*buffer->header.stride+x*bytes;
            if(kind==1 || kind==6) {uint16_t packed=(160>>3)<<11|(96>>2)<<5|(48>>3);lv_memcpy(p,&packed,2);}
            else {
                for(unsigned c=0;c<3;c++) p[c]=(kind==4 || kind==5)?(bg[c]*da+127)/255:bg[c];
                if(bytes==4) p[3]=kind==3?0:da;
            }
        }
        if(kind==6) lv_memset(buffer->data+32*buffer->header.stride,da,16*buffer->header.stride);
        lv_memcpy(before,buffer->data,buffer->data_size);
        lv_layer_t layer;lv_canvas_init_layer(canvas,&layer);
        layer._clip_area=(lv_area_t){6,7,24,25};
        lv_draw_vector_dsc_t *d=lv_draw_vector_dsc_create(&layer);
        lv_vector_path_t *p=lv_vector_path_create(LV_VECTOR_PATH_QUALITY_HIGH);
        if(!d || !p) {lv_draw_vector_dsc_delete(d);if(p) lv_vector_path_delete(p);goto done;}
        lv_draw_vector_dsc_set_fill_color(d,lv_color_hex(0xc04020));lv_draw_vector_dsc_set_fill_opa(d,paint==3?255:128);
        lv_draw_vector_dsc_set_blend_mode(d,modes[mode]);
        if(paint==1 || paint==2) {
            const lv_grad_stop_t stops[2]={{.color=lv_color_hex(0xc04020),.opa=128,.frac=0},
                                           {.color=lv_color_hex(0xc04020),.opa=128,.frac=255}};
            if(paint==1) lv_draw_vector_dsc_set_fill_linear_gradient(d,4,4,28,4);
            else lv_draw_vector_dsc_set_fill_radial_gradient(d,16,16,16);
            lv_draw_vector_dsc_set_fill_gradient_color_stops(d,stops,2);
        }
        lv_vector_path_append_rectangle(p,4,4,24,24,0,0);
        if(paint==4) {
            lv_draw_vector_dsc_set_fill_color32(d,lv_color_to_32(lv_color_hex(0xc04020),0));
            lv_draw_vector_dsc_set_fill_rule(d,LV_VECTOR_FILL_EVENODD);
            lv_vector_path_append_rectangle(p,14,14,4,4,0,0);
        }
        lv_draw_vector_dsc_add_path(d,p);lv_draw_vector(d);
        lv_vector_path_delete(p);lv_draw_vector_dsc_delete(d);lv_canvas_finish_layer(canvas,&layer);
        for(unsigned y=0;y<32;y++) for(unsigned x=0;x<32;x++)
            if((x<6 || x>24 || y<7 || y>25) &&
               lv_memcmp(buffer->data+y*buffer->header.stride+x*bytes,before+y*buffer->header.stride+x*bytes,bytes)) goto done;
        lv_color32_t sample=lv_canvas_get_px(canvas,12,12);
        const uint8_t *sample_data=buffer->data+12*buffer->header.stride+12*bytes;
        if(kind==4) lv_memcpy(&sample,sample_data,4);
        if(kind==6) {
            uint16_t packed;lv_memcpy(&packed,sample_data,2);
            sample.blue=(packed&31)*255/31;sample.green=((packed>>5)&63)*255/63;
            sample.red=(packed>>11)*255/31;
            sample.alpha=buffer->data[32*buffer->header.stride+12*buffer->header.stride/2+12];
            for(unsigned y=0;y<32;y++) for(unsigned x=0;x<32;x++)
                if((x<6 || x>24 || y<7 || y>25) &&
                   buffer->data[32*buffer->header.stride+y*buffer->header.stride/2+x]!=da) goto done;
        }
        if(paint==4) {
            unsigned offset=16*buffer->header.stride+16*bytes;
            if(lv_memcmp(buffer->data+offset,before+offset,bytes) ||
               (kind==6 && buffer->data[32*buffer->header.stride+16*buffer->header.stride/2+16]!=da)) {
                AIC_TEST_E("FAIL vector hole preservation kind=%u mode=%u",kind,mode);goto done;
            }
        }
        if((paint==3 || paint==4) && (sample.red || sample.green || sample.blue ||
            ((kind==0 || kind>=4) && sample.alpha))) {
            AIC_TEST_E("FAIL opaque vector erase kind=%u",kind);goto done;
        }
        unsigned channels[]={sample.blue,sample.green,sample.red,sample.alpha};
        unsigned sa=paint==4?0:paint==3?255:paint?64:128;
        unsigned out_alpha=sa+(da*(255-sa)+127)/255;
        if(mode==4) out_alpha=LV_MIN(255,sa+da);
        if(mode==5) out_alpha=(da*(255-sa)+127)/255;
        if(mode==6 || mode==7) out_alpha=(da*sa+127)/255;
        if(mode==8) out_alpha=sa;
        for(unsigned c=0;c<4;c++) {
            if(c==3 && (kind==1 || kind==2 || kind==3)) continue;
            unsigned expected=out_alpha;
            if(c<3) {
                unsigned blended=mode==1?(source[c]*bg[c]+127)/255:
                    mode==2?255-((255-source[c])*(255-bg[c])+127)/255:source[c];
                unsigned weighted=source[c]*sa*(255-da)+bg[c]*da*(255-sa)+blended*sa*da;
                if(mode==3) weighted=bg[c]*da*255+source[c]*sa*(255-da);
                if(mode==4) weighted=(bg[c]*da+source[c]*sa)*255;
                if(mode==5) weighted=bg[c]*da*(255-sa);
                if(mode==6) weighted=source[c]*sa*da;
                if(mode==7) weighted=bg[c]*da*sa;
                if(mode==8) weighted=source[c]*sa*255;
                expected=(kind>=1 && kind<=5)?LV_MIN(255,(weighted+32512)/65025):
                    out_alpha?(weighted+255*out_alpha/2)/(255*out_alpha):0;
            }
            int error=(int)channels[c]-(int)expected;
            if(error < -((kind==1 || kind==6)?9:4) || error > ((kind==1 || kind==6)?9:4)) {
                AIC_TEST_E("FAIL vector surface kind=%u blend=%u paint=%u channel=%u error=%d",kind,mode,paint,c,error);goto done;
            }
        }
        AIC_TEST_I("PASS vector surface kind=%u blend=%u paint=%u alpha=%u",kind,mode,paint,sample.alpha);
        lv_free(before);before=NULL;
        lv_obj_delete(canvas);canvas=NULL;lv_draw_buf_destroy(buffer);buffer=NULL;
    }
    AIC_TEST_I("PASS 217 vector surface probes; hardware timing remains separate");result=0;
done:
    if(before) lv_free(before);
    if(canvas) lv_obj_delete(canvas);
    if(buffer) lv_draw_buf_destroy(buffer);
    if(result) AIC_TEST_E("FAIL vector surface probes");
    return result;
}
#endif
