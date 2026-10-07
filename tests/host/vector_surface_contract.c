/* SPDX-License-Identifier: Apache-2.0 */
#include "lvgl.h"
#include "lvgl_private.h"
#include "src/draw/sw/lv_draw_sw.h"
#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "src/libs/thorvg/thorvg_capi.h"
static unsigned fail_buffer, buffers, released, fail_tvg, fail_nth, tvg_calls, fail_decoder, decoder_calls;
static lv_draw_buf_t *owned[64];
static unsigned paint_style, task_opacity=255;
static bool ordered;
static uint32_t pattern_pixels[32*32];
static lv_image_dsc_t pattern;
Tvg_Canvas *__real_tvg_swcanvas_create(void);
Tvg_Result __real_tvg_swcanvas_set_target(Tvg_Canvas *,uint32_t *,uint32_t,uint32_t,uint32_t,Tvg_Colorspace);
Tvg_Result __real_tvg_canvas_draw(Tvg_Canvas *);
Tvg_Result __real_tvg_canvas_sync(Tvg_Canvas *);
Tvg_Result __real_tvg_canvas_clear(Tvg_Canvas *,bool);
Tvg_Canvas *__wrap_tvg_swcanvas_create(void) {if(fail_tvg==1 && ++tvg_calls==fail_nth) {fail_tvg=0;return NULL;}return __real_tvg_swcanvas_create();}
Tvg_Result __wrap_tvg_swcanvas_set_target(Tvg_Canvas *c,uint32_t *p,uint32_t s,uint32_t w,uint32_t h,Tvg_Colorspace cs)
{if(fail_tvg==2 && ++tvg_calls==fail_nth) {fail_tvg=0;return TVG_RESULT_INSUFFICIENT_CONDITION;}return __real_tvg_swcanvas_set_target(c,p,s,w,h,cs);}
Tvg_Result __wrap_tvg_canvas_draw(Tvg_Canvas *c)
{Tvg_Result r=__real_tvg_canvas_draw(c);if(fail_tvg==3 && ++tvg_calls==fail_nth) {fail_tvg=0;return TVG_RESULT_INSUFFICIENT_CONDITION;}return r;}
Tvg_Result __wrap_tvg_canvas_sync(Tvg_Canvas *c)
{Tvg_Result r=__real_tvg_canvas_sync(c);if(fail_tvg==4 && ++tvg_calls==fail_nth) {fail_tvg=0;return TVG_RESULT_INSUFFICIENT_CONDITION;}return r;}
Tvg_Result __wrap_tvg_canvas_clear(Tvg_Canvas *c,bool release)
{Tvg_Result r=__real_tvg_canvas_clear(c,release);if(fail_tvg==5 && ++tvg_calls==fail_nth) {fail_tvg=0;return TVG_RESULT_INSUFFICIENT_CONDITION;}return r;}

Tvg_Result __real_tvg_picture_load_raw(Tvg_Paint *,const uint32_t *,uint32_t,uint32_t,bool);
Tvg_Paint *__real_tvg_paint_duplicate(Tvg_Paint *);
Tvg_Result __real_tvg_canvas_push(Tvg_Canvas *,Tvg_Paint *);
Tvg_Result __wrap_tvg_picture_load_raw(Tvg_Paint *p,const uint32_t *pixels,uint32_t w,uint32_t h,bool copy)
{if(fail_tvg==6 && ++tvg_calls==fail_nth) {fail_tvg=0;return TVG_RESULT_INSUFFICIENT_CONDITION;}return __real_tvg_picture_load_raw(p,pixels,w,h,copy);}
Tvg_Paint *__wrap_tvg_paint_duplicate(Tvg_Paint *p)
{if(fail_tvg==7 && ++tvg_calls==fail_nth) {fail_tvg=0;return NULL;}return __real_tvg_paint_duplicate(p);}
Tvg_Result __wrap_tvg_canvas_push(Tvg_Canvas *c,Tvg_Paint *p)
{
    /* The pinned C API takes ownership on both failure paths: immediate
     * rejection destroys the paint; a failed update retains it in the canvas. */
    if(fail_tvg==8 && ++tvg_calls==fail_nth) {fail_tvg=0;tvg_paint_del(p);return TVG_RESULT_INSUFFICIENT_CONDITION;}
    Tvg_Result r=__real_tvg_canvas_push(c,p);
    if(fail_tvg==9 && ++tvg_calls==fail_nth) {fail_tvg=0;return TVG_RESULT_INSUFFICIENT_CONDITION;}
    return r;
}
Tvg_Result __real_tvg_paint_set_composite_method(Tvg_Paint *,Tvg_Paint *,Tvg_Composite_Method);
Tvg_Result __wrap_tvg_paint_set_composite_method(Tvg_Paint *p,Tvg_Paint *target,Tvg_Composite_Method method)
{
    if(fail_tvg==10 && ++tvg_calls==fail_nth) {fail_tvg=0;tvg_paint_del(target);return TVG_RESULT_INSUFFICIENT_CONDITION;}
    Tvg_Result r=__real_tvg_paint_set_composite_method(p,target,method);
    if(fail_tvg==11 && ++tvg_calls==fail_nth) {fail_tvg=0;return TVG_RESULT_INSUFFICIENT_CONDITION;}
    return r;
}
lv_result_t __real_lv_image_decoder_open(lv_image_decoder_dsc_t *,const void *,const lv_image_decoder_args_t *);
lv_result_t __wrap_lv_image_decoder_open(lv_image_decoder_dsc_t *d,const void *src,const lv_image_decoder_args_t *args)
{if(fail_decoder && ++decoder_calls==fail_decoder) {fail_decoder=0;return LV_RESULT_INVALID;}return __real_lv_image_decoder_open(d,src,args);}

lv_draw_buf_t *__real_lv_draw_buf_create(uint32_t,uint32_t,lv_color_format_t,uint32_t);
void __real_lv_draw_buf_destroy(lv_draw_buf_t *);
lv_draw_buf_t *__wrap_lv_draw_buf_create(uint32_t w,uint32_t h,lv_color_format_t cf,uint32_t stride)
{
    if(fail_buffer && !--fail_buffer) return NULL;
    lv_draw_buf_t *b=__real_lv_draw_buf_create(w,h,cf,stride);if(b) {
        unsigned i=0;while(i<64 && owned[i]) i++;assert(i<64);owned[i]=b;buffers++;
    }return b;
}
void __wrap_lv_draw_buf_destroy(lv_draw_buf_t *b)
{
    /* Decoder copies can use create_ex internally; count only our wrapped
     * create allocations, not unrelated cache-owned draw buffers. */
    if(b) for(unsigned i=0;i<64;i++) if(owned[i]==b) {owned[i]=NULL;released++;break;}
    __real_lv_draw_buf_destroy(b);
}
static void flush(lv_display_t *d,const lv_area_t *a,uint8_t *p) {(void)a;(void)p;lv_display_flush_ready(d);}
static const lv_color_format_t formats[]={LV_COLOR_FORMAT_ARGB8888,LV_COLOR_FORMAT_RGB565,
    LV_COLOR_FORMAT_RGB888,LV_COLOR_FORMAT_XRGB8888,LV_COLOR_FORMAT_ARGB8888_PREMULTIPLIED,LV_COLOR_FORMAT_ARGB8888,LV_COLOR_FORMAT_RGB565A8};
static unsigned pixel_size(unsigned kind) {return (kind==1 || kind==6)?2:kind==2?3:4;}
static bool premult(unsigned kind) {return kind==4 || kind==5;}
static void put(uint8_t *p,unsigned kind,unsigned a)
{
    const unsigned c[]={48,96,160};
    if(kind==1 || kind==6) {uint16_t v=(160>>3)<<11|(96>>2)<<5|(48>>3);memcpy(p,&v,2);}
    else {for(unsigned k=0;k<3;k++) p[k]=premult(kind)?(c[k]*a+127)/255:c[k];
        if(pixel_size(kind)==4) p[3]=kind==3?0:a;}
}
static void get(const uint8_t *p,unsigned kind,double *out)
{
    if(kind==1 || kind==6) {uint16_t v;memcpy(&v,p,2);out[0]=(v&31)*255.0/31;out[1]=((v>>5)&63)*255.0/63;out[2]=(v>>11)*255.0/31;out[3]=255;}
    else {for(unsigned k=0;k<3;k++) out[k]=p[k];out[3]=(kind==2 || kind==3)?255:p[3];}
}
static void path(lv_draw_vector_dsc_t *d,lv_area_t clip,uint32_t rgb,unsigned opacity,lv_vector_blend_t blend)
{
    d->ctx->scissor_area=clip;
    lv_draw_vector_dsc_set_fill_color(d,lv_color_hex(rgb));lv_draw_vector_dsc_set_fill_opa(d,opacity);
    lv_draw_vector_dsc_set_blend_mode(d,blend);
    if(paint_style>=1 && paint_style<=4) {
        lv_grad_stop_t stops[2]={{.color=lv_color_hex(rgb),.opa=128,.frac=0},
                                 {.color=lv_color_hex(rgb),.opa=128,.frac=255}};
        if(paint_style<=2) {
            if(paint_style==1) lv_draw_vector_dsc_set_fill_linear_gradient(d,104,204,128,204);
            else lv_draw_vector_dsc_set_fill_radial_gradient(d,116,216,16);
            lv_draw_vector_dsc_set_fill_gradient_color_stops(d,stops,2);
        }
        else {
            lv_draw_vector_dsc_set_fill_opa(d,0);
            lv_draw_vector_dsc_set_stroke_opa(d,opacity);
            lv_draw_vector_dsc_set_stroke_width(d,48);
            if(paint_style==3) lv_draw_vector_dsc_set_stroke_linear_gradient(d,104,204,128,204);
            else lv_draw_vector_dsc_set_stroke_radial_gradient(d,116,216,16);
            lv_draw_vector_dsc_set_stroke_gradient_color_stops(d,stops,2);
        }
    }
    else if(paint_style==5) {
        lv_draw_image_dsc_t image;lv_draw_image_dsc_init(&image);image.src=&pattern;image.opa=128;
        lv_draw_vector_dsc_set_fill_image(d,&image);
        lv_draw_vector_dsc_set_fill_units(d,LV_VECTOR_FILL_UNITS_USER_SPACE_ON_USE);
        lv_matrix_t matrix;lv_matrix_identity(&matrix);lv_matrix_translate(&matrix,100,200);
        lv_draw_vector_dsc_set_fill_transform(d,&matrix);
    }
    lv_vector_path_t *p=lv_vector_path_create(LV_VECTOR_PATH_QUALITY_HIGH);assert(p);
    lv_vector_path_append_rectangle(p,104,204,24,24,0,0);
    lv_draw_vector_dsc_add_path(d,p);lv_vector_path_delete(p);
}
static void reference_coverage(double *dst,const double *src,double sa,unsigned blend,bool associated,double coverage)
{
    double da=dst[3]/255.0,oa=sa+da*(1-sa);
    if(blend==4) oa=fmin(1,sa+da);
    if(blend==5) oa=da*(1-sa);
    if(blend==6 || blend==7) oa=da*(sa+1-coverage);
    if(blend==8) oa=sa+da*(1-coverage);
    for(unsigned c=0;c<3;c++) {
        double dc=associated?(da?dst[c]/da:0):dst[c];
        double bc=blend==1?src[c]*dc/255:blend==2?255-(255-src[c])*(255-dc)/255:src[c];
        double value=src[c]*sa*(1-da)+dc*da*(1-sa)+bc*sa*da;
        if(blend==3) value=dc*da+src[c]*sa*(1-da);
        if(blend==4) value=fmin(255,dc*da+src[c]*sa);
        if(blend==5) value=dc*da*(1-sa);
        if(blend==6) value=src[c]*sa*da+dc*da*(1-coverage);
        if(blend==7) value=dc*da*(sa+1-coverage);
        if(blend==8) value=src[c]*sa+dc*da*(1-coverage);
        dst[c]=!associated && oa?value/oa:value;
    }
    dst[3]=oa*255;
}
static void reference(double *dst,const double *src,double sa,unsigned blend,bool associated)
{reference_coverage(dst,src,sa,blend,associated,1);}
static const lv_vector_blend_t modes[]={LV_VECTOR_BLEND_SRC_OVER,LV_VECTOR_BLEND_MULTIPLY,
    LV_VECTOR_BLEND_SCREEN,LV_VECTOR_BLEND_DST_OVER,LV_VECTOR_BLEND_ADDITIVE,LV_VECTOR_BLEND_SUBTRACTIVE,LV_VECTOR_BLEND_SRC_IN,LV_VECTOR_BLEND_DST_IN,LV_VECTOR_BLEND_NONE};
static unsigned checks,faults;
static void scene(unsigned kind,unsigned alpha,unsigned opacity,unsigned blend,bool split,unsigned failure)
{
    enum {STRIDE=160};uint8_t storage[48*STRIDE+128],before[sizeof storage];
    memset(storage,0xa5,sizeof storage);uint8_t *pixels=storage+64;
    for(unsigned y=0;y<32;y++) for(unsigned x=0;x<32;x++) put(pixels+y*STRIDE+x*pixel_size(kind),kind,alpha);
    if(kind==6) memset(pixels+32*STRIDE,alpha,16*STRIDE);
    memcpy(before,storage,sizeof storage);
    lv_draw_buf_t buf;assert(lv_draw_buf_init(&buf,32,32,formats[kind],STRIDE,pixels,(kind==6?48:32)*STRIDE)==LV_RESULT_OK);
    if(kind==5) buf.header.flags|=LV_IMAGE_FLAGS_PREMULTIPLIED;
    lv_layer_t layer={0};layer.draw_buf=&buf;layer.buf_area=(lv_area_t){100,200,131,231};layer._clip_area=layer.buf_area;
    layer.color_format=formats[kind];
    lv_draw_vector_dsc_t *d=lv_draw_vector_dsc_create(&layer);assert(d);
    lv_area_t clip={106,207,124,225};
    if(ordered) path(d,layer.buf_area,0x2040c0,64,LV_VECTOR_BLEND_SRC_OVER);
    path(d,split?(lv_area_t){100,200,114,231}:layer.buf_area,0xc04020,opacity,
         modes[blend]);
    if(split) path(d,(lv_area_t){115,200,131,231},0xc04020,opacity,
        modes[blend]);
    if(ordered) path(d,layer.buf_area,0x40c020,128,LV_VECTOR_BLEND_SRC_OVER);
    lv_draw_task_t t={0};t.target_layer=&layer;t.clip_area=clip;t.opa=task_opacity;
    fail_buffer=failure==1?1:failure==7?2:failure==10?3:failure==14?4:0;
    fail_tvg=failure>=2 && failure<=6?failure-1:failure==8 || failure==9?2:
        failure>=11 && failure<=13?failure-8:
        failure==17 || failure==18?6:failure==19 || failure==20?7:failure>=21 && failure<=24?8:failure>=26 && failure<=29?9:failure==30 || failure==31?10:failure>=32?11:0;
    fail_nth=failure==8 || (failure>=11 && failure<=13) || failure==18 || failure==20?2:
        failure==9?3:failure>=30?(failure%2?2:1):failure>=26?failure-25:failure>=21?failure-20:1;
    fail_decoder=failure==15?1:failure==16?2:0;tvg_calls=decoder_calls=0;
    lv_draw_sw_vector(&t,d);assert(!d->task_list);lv_draw_vector_dsc_delete(d);
    if(failure) {if(failure!=25) faults++;assert(!fail_buffer && !fail_tvg && !fail_decoder && !memcmp(storage,before,sizeof storage));return;}
    const double src[]={32,64,192};
    for(unsigned y=0;y<32;y++) for(unsigned x=0;x<32;x++) {
        uint8_t *actual=pixels+y*STRIDE+x*pixel_size(kind);
        const uint8_t *old=before+64+y*STRIDE+x*pixel_size(kind);
        bool touched=x>=6 && x<=24 && y>=7 && y<=25;
        if(!touched) {assert(!memcmp(actual,old,pixel_size(kind)));
            if(kind==6) assert(pixels[32*STRIDE+y*STRIDE/2+x]==alpha);
            continue;
        }
        double dst[4],got[4];get(old,kind,dst);get(actual,kind,got);
        if(kind==6) {dst[3]=alpha;got[3]=pixels[32*STRIDE+y*STRIDE/2+x];}
        if(blend==5 && opacity==255 && task_opacity==255 && !paint_style && !ordered && alpha) {
            assert(!got[0] && !got[1] && !got[2]);
            if(kind==0 || kind>=4) assert(!got[3]);
        }
        double sa=opacity/255.0*task_opacity/255.0;
        if(paint_style) sa*=128.0/255;
        if(paint_style==5) sa*=128.0/255;
        const double first[]={192,64,32},last[]={32,192,64};
        if(ordered) reference(dst,first,64.0/255,0,premult(kind));
        reference(dst,src,sa,blend,premult(kind));
        if(ordered) reference(dst,last,128.0/255,0,premult(kind));
        double oa=dst[3]/255.0;
        for(unsigned c=0;c<4;c++) {
            double expected=dst[c];
            /* Targets without alpha store the premultiplied result against
             * implicit black; they cannot retain a reduced output alpha. */
            if(kind==1 || kind==2 || kind==3) expected=c==3?255:expected*oa;
            else if(!oa && c<3 && !premult(kind)) {
                /* Hidden RGB is retained for an already transparent target. */
                double hidden[4];get(old,kind,hidden);
                expected=alpha==0?hidden[c]:0;
            }
            /* Extra unpremultiply error is bounded by one premultiplied unit
             * at low output alpha; RGB565 still includes packed quantization. */
            double tolerance=((kind==1 || kind==6)?9:4)+(paint_style && c<3 && !premult(kind) && oa>0?1/oa:0);
            if(fabs(got[c]-expected)>tolerance) fprintf(stderr,"paint=%u task_opa=%u ",paint_style,task_opacity);
            if(fabs(got[c]-expected)>tolerance) fprintf(stderr,"vector kind=%u alpha=%u opa=%u blend=%u split=%u xy=%u,%u c=%u got=%.1f expected=%.1f\n",kind,alpha,opacity,blend,split,x,y,c,got[c],expected);
            assert(fabs(got[c]-expected)<=tolerance);checks++;
        }
        if(kind==3) assert(actual[3]==old[3]);
    }
    for(unsigned y=0;y<32;y++) assert(!memcmp(pixels+y*STRIDE+32*pixel_size(kind),before+64+y*STRIDE+32*pixel_size(kind),STRIDE-32*pixel_size(kind)));
    assert(!memcmp(storage,before,64) && !memcmp(storage+64+48*STRIDE,before+64+48*STRIDE,64));
}
static void geometry_draw(lv_draw_buf_t *buffer,unsigned shape,unsigned mode,bool coverage)
{
    uint32_t opaque[32*32];for(unsigned i=0;i<32*32;i++) opaque[i]=0xffffffff;
    lv_image_dsc_t image_source=pattern;
    if(coverage) image_source.data=(const uint8_t *)opaque;
    lv_layer_t layer={0};layer.draw_buf=buffer;layer.color_format=buffer->header.cf;
    layer.buf_area=layer._clip_area=(lv_area_t){100,200,131,231};
    lv_draw_vector_dsc_t *d=lv_draw_vector_dsc_create(&layer);assert(d);
    lv_vector_path_t *p=lv_vector_path_create(LV_VECTOR_PATH_QUALITY_HIGH);assert(p);
    lv_draw_vector_dsc_set_fill_color(d,lv_color_hex(0xc04020));
    lv_draw_vector_dsc_set_fill_opa(d,coverage?255:200);lv_draw_vector_dsc_set_blend_mode(d,modes[mode]);
    if(shape==0) {
        const lv_fpoint_t q[]={{102.2f,203.4f},{129.6f,209.1f},{112.7f,229.3f}};
        lv_vector_path_move_to(p,&q[0]);lv_vector_path_line_to(p,&q[1]);
        lv_vector_path_line_to(p,&q[2]);lv_vector_path_close(p);
    }
    else {
        lv_vector_path_append_rectangle(p,103.4f,204.2f,24,24,4,4);
        if(shape==1) {
            lv_vector_path_append_rectangle(p,110.5f,211.5f,10,10,2,2);
            lv_draw_vector_dsc_set_fill_rule(d,LV_VECTOR_FILL_EVENODD);
        }
        if(shape==2 || shape==7) {
            const lv_grad_stop_t stops[]={{.color=lv_color_hex(0xff4020),.opa=coverage?255:0,.frac=0},
                {.color=lv_color_hex(0x2040ff),.opa=coverage || shape==2?255:0,.frac=255}};
            lv_draw_vector_dsc_set_fill_linear_gradient(d,103,204,127,204);
            lv_draw_vector_dsc_set_fill_gradient_color_stops(d,stops,2);
        }
        if(shape==4 || shape==8) {
            if(shape==4) lv_draw_vector_dsc_set_fill_opa(d,0);
            lv_draw_vector_dsc_set_stroke_opa(d,coverage?255:180);
            lv_draw_vector_dsc_set_stroke_color(d,lv_color_hex(0xc04020));
            lv_draw_vector_dsc_set_stroke_width(d,5.5f);
            float dash[]={4,3};lv_draw_vector_dsc_set_stroke_dash(d,dash,2);
            lv_draw_vector_dsc_set_stroke_cap(d,LV_VECTOR_STROKE_CAP_ROUND);
        }
        if(shape==6 && !coverage) lv_draw_vector_dsc_set_fill_color32(d,lv_color_to_32(lv_color_hex(0xc04020),0));
        if(shape==3 || shape==5) {
            lv_draw_image_dsc_t image;lv_draw_image_dsc_init(&image);image.src=&image_source;
            if(shape==5) {
                image_source.header.w=image_source.header.h=16;image_source.header.stride=64;image_source.data_size=1024;
            }
            lv_draw_vector_dsc_set_fill_image(d,&image);
            lv_draw_vector_dsc_set_fill_units(d,LV_VECTOR_FILL_UNITS_USER_SPACE_ON_USE);
            lv_matrix_t matrix;lv_matrix_identity(&matrix);lv_matrix_translate(&matrix,shape==5?109:100,shape==5?203:200);
            if(shape==5) lv_matrix_rotate(&matrix,20);
            lv_draw_vector_dsc_set_fill_transform(d,&matrix);
        }
    }
    lv_draw_vector_dsc_add_path(d,p);lv_vector_path_delete(p);
    lv_draw_task_t t={0};t.target_layer=&layer;t.clip_area=(lv_area_t){105,206,125,226};t.opa=255;
    lv_draw_sw_vector(&t,d);assert(!d->task_list);lv_draw_vector_dsc_delete(d);
    lv_image_cache_drop(&image_source);
}
static void geometry_operators(void)
{
    unsigned covered=0,empty=0,transparent_inside=0;const unsigned alphas[]={0,64,128,255};
    for(unsigned i=0;i<32*32;i++) pattern_pixels[i]=(i%7?0x80U:0)<<24|0x00c04020;
    for(unsigned shape=0;shape<9;shape++) for(unsigned mode=0;mode<9;mode++)
    for(unsigned a=0;a<4;a++) {
        uint32_t source[32*32]={0},mask[32*32]={0},target[32*32],before[32*32];
        for(unsigned i=0;i<32*32;i++) put((uint8_t *)&target[i],4,alphas[a]);
        memcpy(before,target,sizeof target);
        lv_draw_buf_t src,dst,cov;
        assert(lv_draw_buf_init(&src,32,32,LV_COLOR_FORMAT_ARGB8888_PREMULTIPLIED,128,source,sizeof source)==LV_RESULT_OK);
        assert(lv_draw_buf_init(&dst,32,32,LV_COLOR_FORMAT_ARGB8888_PREMULTIPLIED,128,target,sizeof target)==LV_RESULT_OK);
        assert(lv_draw_buf_init(&cov,32,32,LV_COLOR_FORMAT_ARGB8888_PREMULTIPLIED,128,mask,sizeof mask)==LV_RESULT_OK);
        geometry_draw(&src,shape,0,false);geometry_draw(&cov,shape,0,true);geometry_draw(&dst,shape,mode,false);
        for(unsigned i=0;i<32*32;i++) {
            const uint8_t *sp=(const uint8_t *)&source[i],*bp=(const uint8_t *)&before[i],*got=(const uint8_t *)&target[i];
            double sa=sp[3]/255.0,colors[3]={0},expected[4]={bp[0],bp[1],bp[2],bp[3]};
            if(sa) {for(unsigned c=0;c<3;c++) colors[c]=sp[c]/sa;covered++;}
            double q=(mask[i]>>24)/255.0;
            if(!q || (mode<6 && !sa)) {empty++;assert(target[i]==before[i]);}
            if(q==1 && !sa && mode>=6) {transparent_inside++;assert(target[i]==0);}
            reference_coverage(expected,colors,sa,mode,true,q);
            /* Native filtered source-over has one additional rounding step. */
            double tolerance=mode==0?2.01:1.01;
            for(unsigned c=0;c<4;c++) {
                if(fabs(got[c]-expected[c])>tolerance) fprintf(stderr,"geometry shape=%u mode=%u alpha=%u pixel=%u channel=%u src=%u coverage=%.2f got=%u expected=%.2f\n",shape,mode,alphas[a],i,c,sp[3],q,got[c],expected[c]);
                assert(fabs(got[c]-expected[c])<=tolerance);checks++;
            }
        }
    }
    assert(covered && empty && transparent_inside);
    lv_image_cache_drop(&pattern);
    for(unsigned i=0;i<32*32;i++) assert(pattern_pixels[i]==(((i%7?0x80U:0)<<24)|0x00c04020));
}
static void combined_budget(void)
{
    /* One 1024x513 surface fits 4 MiB, two do not. The already drawn normal
     * subtask must not reach the destination when the later blend is rejected. */
    for(unsigned which=0;which<2;which++) {
    unsigned height=which?457:513;
    lv_draw_buf_t *b=__real_lv_draw_buf_create(1024,height,LV_COLOR_FORMAT_ARGB8888,0);assert(b);
    memset(b->data,0xa5,b->data_size);
    lv_layer_t layer={0};layer.draw_buf=b;layer.color_format=b->header.cf;
    layer.buf_area=layer._clip_area=(lv_area_t){0,0,1023,height-1};
    lv_draw_vector_dsc_t *d=lv_draw_vector_dsc_create(&layer);assert(d);
    path(d,layer.buf_area,0xff0000,128,LV_VECTOR_BLEND_SRC_OVER);
    path(d,layer.buf_area,0x00ff00,128,which?LV_VECTOR_BLEND_SRC_IN:LV_VECTOR_BLEND_MULTIPLY);
    lv_draw_task_t t={0};t.target_layer=&layer;t.clip_area=layer.buf_area;t.opa=255;
    unsigned prior=buffers;lv_draw_sw_vector(&t,d);
    assert(!d->task_list && buffers==prior+1+which);
    for(unsigned i=0;i<b->data_size;i++) assert(b->data[i]==0xa5);
    lv_draw_vector_dsc_delete(d);__real_lv_draw_buf_destroy(b);
    }
    /* An otherwise valid pattern exceeds the shared budget when its opaque
     * sampling workspace cannot reuse the much smaller clipped source. */
    lv_image_dsc_t saved=pattern;
    uint32_t *large=malloc(1024*1024*4);assert(large);
    for(unsigned i=0;i<1024*1024;i++) large[i]=0x80c04020;
    pattern.header.w=pattern.header.h=1024;pattern.header.stride=4096;
    pattern.data_size=1024*1024*4;pattern.data=(const uint8_t *)large;
    paint_style=5;
    for(unsigned mode=6;mode<9;mode++) scene(0,128,128,mode,false,25);
    lv_image_cache_drop(&pattern);
    for(unsigned i=0;i<1024*1024;i++) assert(large[i]==0x80c04020);
    free(large);pattern=saved;paint_style=0;
}
static void rejected_views(void)
{
    uint32_t pixels[32*32],before[32*32];
    memset(pixels,0xa5,sizeof pixels);memcpy(before,pixels,sizeof pixels);
    for(unsigned invalid=0;invalid<8;invalid++) {
        lv_draw_buf_t b;assert(lv_draw_buf_init(&b,32,32,LV_COLOR_FORMAT_ARGB8888,128,pixels,sizeof pixels)==LV_RESULT_OK);
        lv_layer_t layer={0};layer.draw_buf=&b;layer.color_format=b.header.cf;
        layer.buf_area=layer._clip_area=(lv_area_t){100,200,131,231};
        lv_draw_vector_dsc_t *d=lv_draw_vector_dsc_create(&layer);assert(d);
        path(d,layer.buf_area,0xff0000,128,LV_VECTOR_BLEND_SRC_OVER);
        lv_draw_task_t t={0};t.target_layer=&layer;t.clip_area=layer.buf_area;t.opa=255;
        if(invalid==0) b.data_size=1;
        if(invalid==1) b.header.stride=4;
        if(invalid==2) b.data=NULL;
        if(invalid==3) layer.draw_buf=NULL;
        if(invalid==4) b.header.cf=LV_COLOR_FORMAT_A8;
        if(invalid==5) t.clip_area=(lv_area_t){0,0,10,10};
        if(invalid==6) {
            /* Declared full view deliberately exceeds the staging budget.
             * No source pixel may be read before that rejection. */
            b.header.w=1024;b.header.h=1025;b.header.stride=4096;b.data_size=4096*1025;
            layer.buf_area=layer._clip_area=t.clip_area=(lv_area_t){0,0,1023,1024};
        }
        if(invalid==7) {b.header.cf=LV_COLOR_FORMAT_RGB565A8;b.header.stride=64;b.data_size=2048;}
        unsigned prior=buffers;lv_draw_sw_vector(&t,d);
        assert(!d->task_list && prior==buffers && !memcmp(before,pixels,sizeof pixels));
        lv_draw_vector_dsc_delete(d);
    }
}
int main(int argc,char **argv)
{
    lv_init();lv_display_t *display=lv_display_create(32,32);assert(display);lv_display_set_flush_cb(display,flush);
    /* Regression for the original translucent multiply failure (blue 3 vs 27). */
    if(argc>1 && !strcmp(argv[1],"translucent-blend")) {
        scene(0,255,128,1,false,0);return 0;
    }
    const unsigned alphas[]={0,64,128,255},opacities[]={64,128,255};unsigned scenes=0;
    unsigned first=argc>1?(unsigned)argv[1][0]-'0':0,last=argc>1?first+1:7;
    for(unsigned kind=first;kind<last;kind++) for(unsigned a=0;a<4;a++) for(unsigned o=0;o<3;o++)
    for(unsigned blend=0;blend<9;blend++) for(unsigned split=0;split<2;split++) {scene(kind,alphas[a],opacities[o],blend,split,false);scenes++;}
    for(unsigned kind=0;kind<7;kind++) {for(unsigned b=1;b<9;b++) {scene(kind,255,255,b,false,false);scenes++;}for(unsigned failure=1;failure<=6;failure++) scene(kind,128,128,0,false,failure);}
    for(unsigned i=0;i<32*32;i++) pattern_pixels[i]=0x80c04020;
    pattern=(lv_image_dsc_t){.header={.magic=LV_IMAGE_HEADER_MAGIC,.cf=LV_COLOR_FORMAT_ARGB8888,
        .w=32,.h=32,.stride=128},.data_size=sizeof pattern_pixels,.data=(const uint8_t *)pattern_pixels};
    for(paint_style=1;paint_style<=5;paint_style++)
    for(unsigned kind=0;kind<7;kind++) for(unsigned a=0;a<4;a++)
    for(unsigned o=1;o<3;o++) for(unsigned g=1;g<3;g++) for(unsigned b=0;b<9;b++) {
        task_opacity=opacities[g];scene(kind,alphas[a],opacities[o],b,true,0);scenes++;
    }
    for(unsigned i=0;i<32*32;i++) assert(pattern_pixels[i]==0x80c04020);
    lv_image_cache_drop(&pattern);
    paint_style=0;task_opacity=255;
    for(unsigned kind=0;kind<7;kind++) for(unsigned b=1;b<9;b++)
    for(unsigned failure=1;failure<=9;failure++) scene(kind,128,128,b,true,failure);
    ordered=true;
    for(unsigned kind=0;kind<7;kind++) for(unsigned b=1;b<9;b++) {
        for(unsigned a=0;a<4;a++) {scene(kind,alphas[a],128,b,true,0);scenes++;}
        for(unsigned failure=7;failure<=9;failure++) scene(kind,128,128,b,true,failure);
    }
    ordered=false;task_opacity=0;
    for(unsigned kind=0;kind<7;kind++) for(unsigned b=0;b<9;b++) for(unsigned a=0;a<4;a++) {
        scene(kind,alphas[a],128,b,true,0);scenes++;
    }
    task_opacity=255;
    for(unsigned kind=0;kind<7;kind++) for(unsigned b=6;b<9;b++)
        for(unsigned failure=10;failure<=13;failure++) scene(kind,128,128,b,true,failure);
    paint_style=5;
    for(unsigned kind=0;kind<7;kind++) for(unsigned b=6;b<9;b++)
        for(unsigned failure=14;failure<=24;failure++) scene(kind,128,128,b,true,failure);
    for(unsigned b=0;b<6;b++) {
        const unsigned failures[]={15,17,19,21,22};
        for(unsigned i=0;i<5;i++) scene(0,128,128,b,false,failures[i]);
    }
    for(unsigned b=6;b<9;b++) for(unsigned failure=26;failure<=33;failure++)
        scene(0,128,128,b,true,failure);
    lv_image_cache_drop(&pattern);paint_style=0;
    geometry_operators();combined_budget();
    rejected_views();
    assert(buffers==released);lv_display_delete(display);lv_deinit();
    printf("PASS vector surfaces %u scenes, %u analytic channels including 972 geometric renders, %u allocation/raster failures, 13 rejected views/budgets, clip/stride/guard preservation\n",scenes,checks,faults);return 0;
}
