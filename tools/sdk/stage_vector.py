#!/usr/bin/env python3
# SPDX-License-Identifier: Apache-2.0
"""Preserve LVGL target representation and task clipping around pinned ThorVG."""
import argparse
import hashlib
from pathlib import Path
REVIEWED_SHA256 = "c6c29642949ba71b8238f500d3ed0bd31f5c3a3ab26b16b68efff5a6e41cbda6"
def corrected(text):
    text = text.replace("\r\n", "\n")
    if hashlib.sha256(text.encode()).hexdigest() != REVIEWED_SHA256:
        raise ValueError("LVGL software vector source changed; review surface correction")
    text = text.replace('#include "lv_draw_sw.h"', '#include "lv_draw_sw.h"\n#include "../../misc/lv_area_private.h"')
    text = text.replace("    lv_opa_t opa;\n} _tvg_draw_state;", "    lv_opa_t opa;\n    lv_area_t clip_area;\n    bool failed;\n    bool use_blend;\n    lv_draw_buf_t *surface;\n    lv_draw_buf_t **blend;\n    lv_draw_buf_t **coverage;\n} _tvg_draw_state;")
    text = text.replace("static void _set_paint_fill_pattern(",
                        "#ifndef AIC_LVGL_VECTOR_SURFACE_BYTES\n#define AIC_LVGL_VECTOR_SURFACE_BYTES (4U * 1024U * 1024U)\n#endif\n\nstatic void _set_paint_fill_pattern(")
    start = text.index("static void _set_paint_fill_pattern(")
    end = text.index("static void _set_paint_fill(", start)
    text = text[:start] + r'''
static bool _set_paint_fill_pattern(Tvg_Paint *obj, Tvg_Canvas *canvas, const lv_draw_image_dsc_t *p,
                                    const lv_matrix_t *m, lv_opa_t opa, bool coverage, _tvg_draw_state *state)
{
    lv_image_decoder_dsc_t decoder;
    lv_image_decoder_args_t args={0};args.premultiply=1;
    if(lv_image_decoder_open(&decoder,p->src,&args)!=LV_RESULT_OK) return false;
    bool result=false;
    Tvg_Paint *img=NULL,*clip=NULL;
    lv_draw_buf_t *pixels=NULL;
    if(!decoder.decoded || decoder.decoded->header.cf!=LV_COLOR_FORMAT_ARGB8888) goto done;
    const lv_image_header_t *header=&decoder.decoded->header;
    if(!header->w || !header->h) goto done;
    uint32_t stride=header->w*sizeof(uint32_t);
    if(!coverage && header->stride!=stride &&
       lv_draw_buf_adjust_stride((lv_draw_buf_t *)decoder.decoded,stride)!=LV_RESULT_OK) goto done;
    img=tvg_picture_new();
    if(!img) goto done;
    if(coverage) {
        /* Use the same picture sampler for coverage. A geometric rectangle
         * has different filtered edge coverage under rotation. Reuse the
         * not-yet-drawn source surface when it can hold the opaque image. */
        uint64_t bytes=(uint64_t)stride*header->h;
        lv_draw_buf_t *workspace=*state->blend;
        if(bytes>workspace->data_size) {
            uint64_t used=(uint64_t)state->surface->data_size+workspace->data_size+(*state->coverage)->data_size;
            if(bytes>UINT32_MAX || used+bytes>AIC_LVGL_VECTOR_SURFACE_BYTES) goto done;
            pixels=lv_draw_buf_create(header->w,header->h,LV_COLOR_FORMAT_ARGB8888,stride);
            if(!pixels || used+pixels->data_size>AIC_LVGL_VECTOR_SURFACE_BYTES) goto done;
            workspace=pixels;
        }
        lv_memset(workspace->data,255,(uint32_t)bytes);
        Tvg_Result loaded=tvg_picture_load_raw(img,(uint32_t *)workspace->data,header->w,header->h,true);
        if(!pixels) lv_draw_buf_clear(workspace,NULL);
        if(loaded!=TVG_RESULT_SUCCESS) goto done;
    }
    else if(tvg_picture_load_raw(img,(uint32_t *)decoder.decoded->data,header->w,header->h,true)!=TVG_RESULT_SUCCESS)
        goto done;
    clip=tvg_paint_duplicate(obj);
    if(!clip) goto done;
    Tvg_Result attached=tvg_paint_set_composite_method(img,clip,TVG_COMPOSITE_METHOD_CLIP_PATH);
    clip=NULL; /* Pinned C API consumes valid targets even when attachment fails. */
    if(attached!=TVG_RESULT_SUCCESS) goto done;
    Tvg_Matrix matrix;lv_matrix_to_tvg(&matrix,m);
    if(tvg_paint_set_opacity(img,coverage?255:LV_UDIV255(p->opa*opa))!=TVG_RESULT_SUCCESS ||
       tvg_paint_set_transform(img,&matrix)!=TVG_RESULT_SUCCESS) goto done;
    Tvg_Result pushed=tvg_canvas_push(canvas,img);
    img=NULL; /* Pinned C API always consumes a paint for a valid canvas. */
    if(pushed!=TVG_RESULT_SUCCESS) goto done;
    result=true;
done:
    if(clip) tvg_paint_del(clip);
    if(img) tvg_paint_del(img);
    if(pixels) lv_draw_buf_destroy(pixels);
    lv_image_decoder_close(&decoder);
    return result;
}

''' + text[end:]
    text = text.replace("static void _set_paint_fill(", "static bool _set_paint_fill(")
    text = text.replace("_set_paint_fill_pattern(obj, canvas, &dsc->img_dsc, &imx, opa);",
                        "return _set_paint_fill_pattern(obj, canvas, &dsc->img_dsc, &imx, (opa * dsc->opa + 127U) / 255U, false, NULL);")
    text = text.replace('\n}\n\nstatic Tvg_Blend_Method lv_blend_to_tvg', '\n    return true;\n}\n\nstatic Tvg_Blend_Method lv_blend_to_tvg')
    start = text.index("static void _blend_draw_buf(")
    end = text.index("static void _task_draw_cb(", start)
    text = text[:start] + r"""
#ifndef AIC_LVGL_VECTOR_SURFACE_BYTES
#define AIC_LVGL_VECTOR_SURFACE_BYTES (4U * 1024U * 1024U)
#endif

/* Separable source-over and Porter-Duff/additive operators in premultiplied
 * integer space. Vector SUBTRACTIVE follows LVGL's VG-Lite mapping to
 * VG_LITE_BLEND_SUBTRACT (D * (1 - Sa)), not image RGB subtraction.
 * Keep the source and destination alpha contributions; ThorVG 0.15.3's special
 * solid blender instead treats its inputs as opaque channel values. */
static uint32_t _aic_vector_blend_pixel(uint32_t source, uint32_t dest, lv_vector_blend_t mode, uint32_t coverage)
{
    uint32_t sa=source>>24,da=dest>>24;
    bool bounded=mode==LV_VECTOR_BLEND_SRC_IN || mode==LV_VECTOR_BLEND_DST_IN || mode==LV_VECTOR_BLEND_NONE;
    if(!bounded && !sa) return dest;
    /* Source alpha includes geometric coverage; preserve the uncovered target. */
    coverage=LV_MAX(coverage,sa);
    uint32_t keep=255-coverage;
    uint32_t a=sa+(da*(255-sa)+127)/255;
    if(mode==LV_VECTOR_BLEND_ADDITIVE) a=LV_MIN(255,sa+da);
    else if(mode==LV_VECTOR_BLEND_SUBTRACTIVE) a=(da*(255-sa)+127)/255;
    if(mode==LV_VECTOR_BLEND_SRC_IN || mode==LV_VECTOR_BLEND_DST_IN) a=(da*(sa+keep)+127)/255;
    else if(mode==LV_VECTOR_BLEND_NONE) a=sa+(da*keep+127)/255;
    uint32_t value=a<<24;
    for(unsigned shift=0;shift<24;shift+=8) {
        uint32_t sc=(source>>shift)&255,dc=(dest>>shift)&255,c;
        if(mode==LV_VECTOR_BLEND_MULTIPLY)
            c=(sc*(255-da)+dc*(255-sa)+sc*dc+127)/255;
        else if(mode==LV_VECTOR_BLEND_SCREEN) c=sc+dc-(sc*dc+127)/255;
        else if(mode==LV_VECTOR_BLEND_DST_OVER) c=dc+(sc*(255-da)+127)/255;
        else if(mode==LV_VECTOR_BLEND_ADDITIVE) c=LV_MIN(255,sc+dc);
        else if(mode==LV_VECTOR_BLEND_SRC_IN) c=(sc*da+dc*keep+127)/255;
        else if(mode==LV_VECTOR_BLEND_DST_IN) c=(dc*(sa+keep)+127)/255;
        else if(mode==LV_VECTOR_BLEND_NONE) c=sc+(dc*keep+127)/255;
        else c=(dc*(255-sa)+127)/255; /* Vector SUBTRACTIVE: destination out. */
        value|=LV_MIN(c,a)<<shift;
    }
    return value;
}

static bool _aic_vector_blend_begin(_tvg_draw_state *state, lv_vector_blend_t mode)
{
    state->use_blend=mode==LV_VECTOR_BLEND_MULTIPLY || mode==LV_VECTOR_BLEND_SCREEN ||
        mode==LV_VECTOR_BLEND_DST_OVER || mode==LV_VECTOR_BLEND_ADDITIVE ||
        mode==LV_VECTOR_BLEND_SUBTRACTIVE || mode==LV_VECTOR_BLEND_SRC_IN ||
        mode==LV_VECTOR_BLEND_DST_IN || mode==LV_VECTOR_BLEND_NONE;
    if(!state->use_blend) return true;
    if(!*state->blend) {
        /* Both full clipped surfaces share the same configured byte budget. */
        if((uint64_t)state->surface->data_size*2>AIC_LVGL_VECTOR_SURFACE_BYTES) return false;
        *state->blend=lv_draw_buf_create(state->surface->header.w,state->surface->header.h,
                                       LV_COLOR_FORMAT_ARGB8888,state->surface->header.stride);
        if(!*state->blend) return false;
    }
    if((mode==LV_VECTOR_BLEND_SRC_IN || mode==LV_VECTOR_BLEND_DST_IN || mode==LV_VECTOR_BLEND_NONE) && !*state->coverage) {
        uint32_t w=state->surface->header.w,h=state->surface->header.h;
        uint64_t mask_bytes=(uint64_t)lv_draw_buf_width_to_stride(w,LV_COLOR_FORMAT_A8)*h;
        if((uint64_t)state->surface->data_size*2+mask_bytes>AIC_LVGL_VECTOR_SURFACE_BYTES) return false;
        *state->coverage=lv_draw_buf_create(w,h,LV_COLOR_FORMAT_A8,LV_STRIDE_AUTO);
        if(!*state->coverage) return false;
    }
    lv_draw_buf_t *src=*state->blend;
    lv_draw_buf_clear(src,NULL);
    return tvg_swcanvas_set_target(state->canvas,(uint32_t *)src->data,src->header.stride/4,
                                  src->header.w,src->header.h,TVG_COLORSPACE_ARGB8888)==TVG_RESULT_SUCCESS;
}

static bool _aic_vector_coverage(_tvg_draw_state *state, const lv_vector_path_t *path,
                                 const lv_vector_path_ctx_t *dsc, const lv_area_t *area)
{
    if(dsc->blend_mode!=LV_VECTOR_BLEND_SRC_IN && dsc->blend_mode!=LV_VECTOR_BLEND_DST_IN &&
       dsc->blend_mode!=LV_VECTOR_BLEND_NONE) return true;
    Tvg_Canvas *canvas=state->canvas;
    Tvg_Paint *obj=tvg_shape_new();
    if(!obj) return false;
    if(tvg_canvas_set_viewport(canvas,area->x1+state->translate_x,area->y1+state->translate_y,
                              lv_area_get_width(area),lv_area_get_height(area))!=TVG_RESULT_SUCCESS) goto fail;
    lv_matrix_t matrix;lv_matrix_identity(&matrix);
    lv_matrix_translate(&matrix,state->translate_x,state->translate_y);
    lv_matrix_multiply(&matrix,&dsc->matrix);
    Tvg_Matrix transform;lv_matrix_to_tvg(&transform,&matrix);
    if(tvg_paint_set_transform(obj,&transform)!=TVG_RESULT_SUCCESS) goto fail;
    if(!path) goto fail; /* Native clear subtasks always use SRC_OVER. */
    _set_paint_shape(obj,path);
    tvg_shape_set_fill_rule(obj,lv_fill_rule_to_tvg(dsc->fill_dsc.fill_rule));
    tvg_shape_set_fill_color(obj,255,255,255,dsc->fill_dsc.opa?255:0);
    if(dsc->fill_dsc.style==LV_VECTOR_DRAW_STYLE_PATTERN) {
        tvg_shape_set_fill_color(obj,0,0,0,0);
        if(dsc->fill_dsc.opa) {
            lv_matrix_t imx=matrix;
            if(dsc->fill_dsc.fill_units==LV_VECTOR_FILL_UNITS_OBJECT_BOUNDING_BOX) {
                float x,y,w,h;
                if(tvg_paint_get_bounds(obj,&x,&y,&w,&h,false)!=TVG_RESULT_SUCCESS) goto fail;
                lv_matrix_translate(&imx,x,y);
            }
            lv_matrix_multiply(&imx,&dsc->fill_dsc.matrix);
            if(!_set_paint_fill_pattern(obj,canvas,&dsc->fill_dsc.img_dsc,&imx,255,true,state)) goto fail;
        }
    }
    lv_vector_stroke_dsc_t stroke=dsc->stroke_dsc;
    stroke.style=LV_VECTOR_DRAW_STYLE_SOLID;stroke.color=lv_color_to_32(lv_color_white(),255);
    stroke.opa=stroke.opa?255:0;_set_paint_stroke(obj,&stroke);
    Tvg_Result pushed=tvg_canvas_push(canvas,obj);
    obj=NULL;
    if(pushed!=TVG_RESULT_SUCCESS) goto fail;
    if(tvg_canvas_draw(canvas)!=TVG_RESULT_SUCCESS || tvg_canvas_sync(canvas)!=TVG_RESULT_SUCCESS ||
       tvg_canvas_clear(canvas,true)!=TVG_RESULT_SUCCESS) goto fail;
    lv_draw_buf_t *src=*state->blend,*mask=*state->coverage;
    for(uint32_t y=0;y<src->header.h;y++) {
        const uint32_t *row=(const uint32_t *)(src->data+y*src->header.stride);
        uint8_t *to=mask->data+y*mask->header.stride;
        for(uint32_t x=0;x<src->header.w;x++) to[x]=row[x]>>24;
    }
    lv_draw_buf_clear(src,NULL);
    return true;
fail:
    if(obj) tvg_paint_del(obj);
    return false;
}

static bool _aic_vector_blend_finish(_tvg_draw_state *state, lv_vector_blend_t mode, const lv_area_t *area)
{
    if(!state->use_blend) return true;
    lv_draw_buf_t *src=*state->blend,*dst=state->surface;
    for(int32_t y=area->y1;y<=area->y2;y++) {
        uint32_t *to=(uint32_t *)(dst->data+(y+state->translate_y)*dst->header.stride);
        const uint32_t *from=(const uint32_t *)(src->data+(y+state->translate_y)*src->header.stride);
        for(int32_t x=area->x1;x<=area->x2;x++) {
            uint32_t i=x+state->translate_x,q=0;
            if(mode==LV_VECTOR_BLEND_SRC_IN || mode==LV_VECTOR_BLEND_DST_IN || mode==LV_VECTOR_BLEND_NONE)
                q=(*state->coverage)->data[(y+state->translate_y)*(*state->coverage)->header.stride+i];
            to[i]=_aic_vector_blend_pixel(from[i],to[i],mode,q);
        }
    }
    return tvg_swcanvas_set_target(state->canvas,(uint32_t *)dst->data,dst->header.stride/4,
                                  dst->header.w,dst->header.h,TVG_COLORSPACE_ARGB8888)==TVG_RESULT_SUCCESS;
}

""" + text[end:]
    text = text.replace("    Tvg_Paint * obj = tvg_shape_new();\n\n    _tvg_rect rc;\n    lv_area_to_tvg(&rc, &dsc->scissor_area);", """    lv_area_t clipped;
    if(state->failed || !lv_area_intersect(&clipped, &dsc->scissor_area, &state->clip_area)) return;
    if(!_aic_vector_blend_begin(state,dsc->blend_mode)) { state->failed=true; return; }
    if(!_aic_vector_coverage(state,path,dsc,&clipped)) {state->failed=true;return;}
    Tvg_Paint * obj = tvg_shape_new();
    if(!obj) { state->failed = true; return; }
    _tvg_rect rc;
    lv_area_to_tvg(&rc, &clipped);
    if(tvg_canvas_set_viewport(canvas, clipped.x1 + state->translate_x,
                              clipped.y1 + state->translate_y,
                              lv_area_get_width(&clipped), lv_area_get_height(&clipped)) != TVG_RESULT_SUCCESS) {
        tvg_paint_del(obj); state->failed = true; return;
    }""")
    text = text.replace("    tvg_paint_set_opacity(obj, state->opa);\n    tvg_canvas_push(canvas, obj);", """    tvg_paint_set_opacity(obj, state->opa);
    if(tvg_canvas_push(canvas, obj) != TVG_RESULT_SUCCESS) {
        /* A valid canvas consumes obj even if its update reports failure. */
        state->failed = true; return;
    }
    /* ThorVG's viewport belongs to the canvas, not to each queued paint. Flush
     * each native subtask before another scissor can replace its viewport.
     * The canvas clear releases paints but retains destination pixels. */
    if(tvg_canvas_draw(canvas) != TVG_RESULT_SUCCESS ||
       tvg_canvas_sync(canvas) != TVG_RESULT_SUCCESS ||
       tvg_canvas_clear(canvas, true) != TVG_RESULT_SUCCESS ||
       !_aic_vector_blend_finish(state,dsc->blend_mode,&clipped)) state->failed = true;""")
    text = text.replace("_set_paint_blend_mode(obj, dsc->blend_mode);",
                        "_set_paint_blend_mode(obj, state->use_blend ? LV_VECTOR_BLEND_SRC_OVER : dsc->blend_mode);")
    text = text.replace("_set_paint_fill(obj, canvas, &dsc->fill_dsc, &matrix, state->opa);",
                        "if(!_set_paint_fill(obj, canvas, &dsc->fill_dsc, &matrix, state->opa)) { tvg_paint_del(obj); state->failed=true; return; }")
    # Preserve the opaque endpoint: LV_OPA_MIX2(255,255) is 254. A vector
    # destination-out operation must fully erase at paint opacity 255.
    text = text.replace("color->a = LV_OPA_MIX2(c->alpha, opa);",
                        "color->a = (c->alpha * opa + 127U) / 255U;")
    # Native gradient/pattern paint opacity must participate before composition.
    text = text.replace("const lv_matrix_t * matrix)\n{\n    Tvg_Color_Stop * stops",
                        "const lv_matrix_t * matrix, lv_opa_t opacity)\n{\n    Tvg_Color_Stop * stops")
    text = text.replace("stops[i].a = s->opa;", "stops[i].a = (s->opa * opacity + 127U) / 255U;")
    text = text.replace("const lv_vector_gradient_t * g, const lv_matrix_t * m)\n{",
                        "const lv_vector_gradient_t * g, const lv_matrix_t * m, lv_opa_t opacity)\n{")
    text = text.replace("_setup_gradient(grad, g, m);", "_setup_gradient(grad, g, m, opacity);")
    text = text.replace("_set_paint_stroke_gradient(obj, &dsc->gradient, &dsc->matrix);",
                        "_set_paint_stroke_gradient(obj, &dsc->gradient, &dsc->matrix, dsc->opa);")
    text = text.replace("_set_paint_fill_gradient(obj, &dsc->gradient, &dsc->matrix);",
                        "_set_paint_fill_gradient(obj, &dsc->gradient, &dsc->matrix, dsc->opa);")
    start = text.index("void lv_draw_sw_vector(")
    end = text.index("\n/**********************", start)
    text = text[:start] + r"""
#ifndef AIC_LVGL_VECTOR_SURFACE_BYTES
#define AIC_LVGL_VECTOR_SURFACE_BYTES (4U * 1024U * 1024U)
#endif

/* ThorVG operates on premultiplied ARGB. Seed its bounded surface from the
 * actual destination so blend modes also see the correct backdrop. */
static uint32_t _aic_vector_load(const uint8_t * p, const uint8_t *alpha, lv_color_format_t cf, bool premult)
{
    uint32_t a=255, r, g, b;
    if(cf==LV_COLOR_FORMAT_RGB565 || cf==LV_COLOR_FORMAT_RGB565A8) {
        uint16_t value; lv_memcpy(&value,p,2);
        r=((value>>11)&31)*255/31; g=((value>>5)&63)*255/63; b=(value&31)*255/31;
    }
    else {
        b=p[0]; g=p[1]; r=p[2];
        if(cf!=LV_COLOR_FORMAT_RGB888 && cf!=LV_COLOR_FORMAT_XRGB8888) a=p[3];
        if(!premult) {r=(r*a+127)/255; g=(g*a+127)/255; b=(b*a+127)/255;}
    }
    if(alpha) { a=*alpha; r=(r*a+127)/255; g=(g*a+127)/255; b=(b*a+127)/255; }
    return (a<<24)|(r<<16)|(g<<8)|b;
}

static void _aic_vector_store(uint8_t * p, uint8_t *alpha, uint32_t value, lv_color_format_t cf, bool premult)
{
    uint32_t a=value>>24, r=(value>>16)&255, g=(value>>8)&255, b=value&255;
    if(alpha) *alpha=a;
    /* Alpha-less targets retain the premultiplied RGB against implicit black.
     * Unpremultiplication would undo destination-out on opaque RGB storage. */
    bool has_alpha=alpha || cf==LV_COLOR_FORMAT_ARGB8888 || cf==LV_COLOR_FORMAT_ARGB8888_PREMULTIPLIED;
    if(!premult && has_alpha && a) {
        r=LV_MIN(255,(r*255+a/2)/a); g=LV_MIN(255,(g*255+a/2)/a); b=LV_MIN(255,(b*255+a/2)/a);
    }
    if(cf==LV_COLOR_FORMAT_RGB565 || cf==LV_COLOR_FORMAT_RGB565A8) {
        uint16_t packed=((r>>3)<<11)|((g>>2)<<5)|(b>>3); lv_memcpy(p,&packed,2);
    }
    else {
        p[0]=b; p[1]=g; p[2]=r;
        if(cf!=LV_COLOR_FORMAT_RGB888 && cf!=LV_COLOR_FORMAT_XRGB8888) p[3]=a;
    }
}

void lv_draw_sw_vector(lv_draw_task_t * t, lv_draw_vector_dsc_t * dsc)
{
    if(!dsc->task_list) return;
    lv_layer_t * layer=dsc->base.layer;
    lv_draw_buf_t * dst=layer->draw_buf, *scratch=NULL, *blend=NULL, *coverage=NULL;
    Tvg_Canvas *canvas=NULL;
    lv_area_t area;
    if(!dst || !dst->data || !lv_area_intersect(&area,&t->clip_area,&layer->buf_area)) goto done;
    lv_color_format_t cf=dst->header.cf;
    if(cf!=LV_COLOR_FORMAT_RGB565 && cf!=LV_COLOR_FORMAT_RGB565A8 && cf!=LV_COLOR_FORMAT_RGB888 && cf!=LV_COLOR_FORMAT_XRGB8888 &&
       cf!=LV_COLOR_FORMAT_ARGB8888 && cf!=LV_COLOR_FORMAT_ARGB8888_PREMULTIPLIED) goto done;
    uint32_t pixel=lv_color_format_get_size(cf), width=lv_area_get_width(&area), height=lv_area_get_height(&area);
    int32_t layer_w=lv_area_get_width(&layer->buf_area),layer_h=lv_area_get_height(&layer->buf_area);
    if(layer_w<=0 || layer_h<=0 || layer_w>dst->header.w || layer_h>dst->header.h ||
       (uint64_t)dst->header.w*pixel>dst->header.stride ||
       (uint64_t)dst->header.stride*dst->header.h>dst->data_size) goto done;
    uint8_t *alpha_origin=NULL;
    if(cf==LV_COLOR_FORMAT_RGB565A8) {
        if((dst->header.stride&1U) ||
           (uint64_t)dst->header.stride*dst->header.h+(uint64_t)(dst->header.stride/2)*dst->header.h>dst->data_size) goto done;
        alpha_origin=dst->data+dst->header.stride*dst->header.h+
            (area.y1-layer->buf_area.y1)*(dst->header.stride/2)+(area.x1-layer->buf_area.x1);
    }
    uint64_t bytes=(uint64_t)lv_draw_buf_width_to_stride(width,LV_COLOR_FORMAT_ARGB8888)*height;
    if(!bytes || bytes>UINT32_MAX || bytes>AIC_LVGL_VECTOR_SURFACE_BYTES) goto done;
    scratch=lv_draw_buf_create(width,height,LV_COLOR_FORMAT_ARGB8888,LV_STRIDE_AUTO);
    if(!scratch) goto done;
    bool premult=cf==LV_COLOR_FORMAT_ARGB8888_PREMULTIPLIED ||
        (cf==LV_COLOR_FORMAT_ARGB8888 && (dst->header.flags&LV_IMAGE_FLAGS_PREMULTIPLIED));
    uint8_t *origin=dst->data+(area.y1-layer->buf_area.y1)*dst->header.stride+
        (area.x1-layer->buf_area.x1)*pixel;
    for(uint32_t y=0;y<height;y++) {
        uint32_t *row=(uint32_t *)(scratch->data+y*scratch->header.stride);
        for(uint32_t x=0;x<width;x++) row[x]=_aic_vector_load(origin+y*dst->header.stride+x*pixel,alpha_origin?alpha_origin+y*(dst->header.stride/2)+x:NULL,cf,premult);
    }
    canvas=tvg_swcanvas_create();
    if(!canvas || tvg_swcanvas_set_target(canvas,(uint32_t *)scratch->data,scratch->header.stride/4,
                                         width,height,TVG_COLORSPACE_ARGB8888)!=TVG_RESULT_SUCCESS) goto done;
    _tvg_draw_state state={.canvas=canvas,.translate_x=-area.x1,.translate_y=-area.y1,
                          .opa=t->opa,.clip_area=area,.failed=false,.surface=scratch,.blend=&blend,.coverage=&coverage};
    lv_vector_for_each_destroy_tasks(dsc->task_list,_task_draw_cb,&state);
    dsc->task_list=NULL;
    if(state.failed) { LV_LOG_WARN("Vector raster failed; destination preserved"); goto done; }
    for(uint32_t y=0;y<height;y++) {
        const uint32_t *row=(const uint32_t *)(scratch->data+y*scratch->header.stride);
        for(uint32_t x=0;x<width;x++) {
            uint8_t *to=origin+y*dst->header.stride+x*pixel;
            /* Exact preservation includes hidden RGB at alpha zero, padding,
             * unused X bytes and untouched low-alpha quantization. */
            uint8_t *alpha=alpha_origin?alpha_origin+y*(dst->header.stride/2)+x:NULL;
            if(row[x]!=_aic_vector_load(to,alpha,cf,premult)) _aic_vector_store(to,alpha,row[x],cf,premult);
        }
    }
done:
    if(canvas) tvg_canvas_destroy(canvas);
    if(coverage) lv_draw_buf_destroy(coverage);
    if(blend) lv_draw_buf_destroy(blend);
    if(scratch) lv_draw_buf_destroy(scratch);
    if(dsc->task_list) {
        LV_LOG_WARN("Vector surface unavailable; destination preserved");
        lv_vector_for_each_destroy_tasks(dsc->task_list,NULL,NULL); dsc->task_list=NULL;
    }
}
""" + text[end:]
    return text

def generate(source,destination):
    result=corrected(Path(source).read_text(encoding="utf-8"));output=Path(destination)
    output.parent.mkdir(parents=True,exist_ok=True)
    if not output.exists() or output.read_text(encoding="utf-8")!=result:
        output.write_bytes(result.encode("utf-8"))
    return str(output)
if __name__=="__main__":
    parser=argparse.ArgumentParser();parser.add_argument("--source",required=True);parser.add_argument("--output",required=True)
    args=parser.parse_args();generate(args.source,args.output)
