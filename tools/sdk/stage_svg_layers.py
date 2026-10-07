# SPDX-License-Identifier: Apache-2.0
"""Add bounded post-composition SVG opacity to the hash-guarded render unit."""

def corrected_layers(text):
    text = text.replace('#include "lv_svg_render.h"',
                        '#include "lv_svg_render.h"\n#include "../../misc/lv_area_private.h"')
    helpers = r"""
/* Unused upper flag bits keep the pinned public render-object layout unchanged.
 * Opacity is not inherited unless explicitly requested. Fill/stroke opacity
 * remains independent and retains the native style inheritance path. */
#define AIC_SVG_OPACITY_SET (1U << 24)
#define AIC_SVG_OPACITY_INHERIT (1U << 25)
#define AIC_SVG_OPACITY_MASK (255U << 16)
#ifndef AIC_LVGL_SVG_LAYER_BYTES
#define AIC_LVGL_SVG_LAYER_BYTES (4U * 1024U * 1024U)
#endif
#define AIC_SVG_RENDER_DEPTH 48U
static uint64_t _aic_svg_layer_bytes;
static uint32_t _aic_svg_render_depth;
static lv_opa_t _aic_svg_parent_opacity=255;

typedef struct {
    lv_layer_t *layer;
    lv_opa_t opacity,previous_opacity;
} _aic_svg_opacity_scope;

static void _aic_svg_set_opacity(lv_svg_render_obj_t *obj,const lv_svg_attr_t *attr)
{
    obj->flags &= ~(AIC_SVG_OPACITY_SET|AIC_SVG_OPACITY_INHERIT|AIC_SVG_OPACITY_MASK);
    if(attr->class_type==LV_SVG_ATTR_VALUE_CLASS_INHERIT) {
        obj->flags |= AIC_SVG_OPACITY_INHERIT;
        return;
    }
    float value=attr->value.fval;
    if(!isfinite(value)) return;
    value=LV_CLAMP(0.0f,value,1.0f);
    obj->flags |= AIC_SVG_OPACITY_SET | ((uint32_t)(value*255.0f+0.5f)<<16);
}

static lv_draw_vector_dsc_t *_aic_svg_opacity_begin(const lv_svg_render_obj_t *obj,
                                                    lv_draw_vector_dsc_t *parent,
                                                    _aic_svg_opacity_scope *scope)
{
    lv_memzero(scope,sizeof(*scope));
    lv_opa_t opacity=!obj?255:(obj->flags&AIC_SVG_OPACITY_INHERIT)?_aic_svg_parent_opacity:
        (obj->flags&AIC_SVG_OPACITY_SET)?(obj->flags>>16)&255:255;
    if(!opacity) return NULL;
    if(_aic_svg_render_depth==AIC_SVG_RENDER_DEPTH) {
        LV_LOG_WARN("SVG render depth exceeded");return NULL;
    }
    lv_draw_vector_dsc_t *draw=parent;
    if(opacity!=255) {
        lv_layer_t *target=parent->base.layer;
        lv_area_t area;
        if(!lv_area_intersect(&area,&target->_clip_area,&target->buf_area) ||
           !lv_area_intersect(&area,&area,&parent->ctx->scissor_area)) return NULL;
        uint64_t bytes=(uint64_t)lv_area_get_height(&area)*
            lv_draw_buf_width_to_stride(lv_area_get_width(&area),LV_COLOR_FORMAT_ARGB8888);
        if(!bytes || bytes>UINT32_MAX || _aic_svg_layer_bytes+bytes>AIC_LVGL_SVG_LAYER_BYTES) {
            LV_LOG_WARN("SVG opacity layer budget exceeded");return NULL;
        }
        scope->layer=lv_draw_layer_create(target,LV_COLOR_FORMAT_ARGB8888,&area);
        if(!scope->layer || !lv_draw_layer_alloc_buf(scope->layer)) goto failed;
        scope->layer->opa=255;scope->layer->recolor=lv_color32_make(0,0,0,0);
        draw=lv_draw_vector_dsc_create(scope->layer);
        if(!draw) goto failed;
        _copy_draw_dsc(draw->ctx,parent->ctx);
        draw->ctx->matrix=parent->ctx->matrix;
        draw->ctx->scissor_area=area;
        _aic_svg_layer_bytes+=bytes;
        /* Commit preceding vector subtasks before queuing this composite. */
        lv_draw_vector(parent);
    }
    scope->opacity=opacity;scope->previous_opacity=_aic_svg_parent_opacity;
    _aic_svg_parent_opacity=opacity;_aic_svg_render_depth++;
    return draw;
failed:
    lv_draw_layer_delete(scope->layer);scope->layer=NULL;
    LV_LOG_WARN("SVG opacity layer allocation failed");
    return NULL;
}

static void _aic_svg_opacity_end(lv_draw_vector_dsc_t *parent,lv_draw_vector_dsc_t *draw,
                                 _aic_svg_opacity_scope *scope)
{
    if(scope->layer) {
        lv_draw_vector(draw);
        _copy_draw_dsc(parent->ctx,draw->ctx);
        lv_draw_image_dsc_t composite;lv_draw_image_dsc_init(&composite);
        composite.src=scope->layer;composite.header=scope->layer->draw_buf->header;
        composite.opa=scope->opacity;
        lv_draw_layer(parent->base.layer,&composite,&scope->layer->buf_area);
        lv_draw_vector_dsc_delete(draw);
    }
    _aic_svg_parent_opacity=scope->previous_opacity;_aic_svg_render_depth--;
}

static void _aic_svg_render_object(const lv_svg_render_obj_t *obj,lv_draw_vector_dsc_t *dsc,
                                    const lv_matrix_t *matrix)
{
    _aic_svg_opacity_scope scope;
    lv_draw_vector_dsc_t *draw=_aic_svg_opacity_begin(obj,dsc,&scope);
    if(!draw) return;
    obj->clz->render(obj,draw,matrix);
    _aic_svg_opacity_end(dsc,draw,&scope);
}

"""
    text = text.replace("static void _set_render_attrs(",helpers+"static void _set_render_attrs(",1)
    text = text.replace("        obj->clz->set_attr(obj, &(state->draw_dsc->dsc), attr);",
                        '        if(attr->id==LV_SVG_ATTR_OPACITY) _aic_svg_set_opacity(obj,attr);\n        else obj->clz->set_attr(obj, &(state->draw_dsc->dsc), attr);')
    text = text.replace("list->clz->render(list, dsc, matrix);","_aic_svg_render_object(list, dsc, matrix);")
    text = text.replace("ref->clz->render(ref, dsc, NULL);","_aic_svg_render_object(ref, dsc, NULL);")
    # Span layout must advance even at zero opacity or when a layer is declined.
    text = text.replace("    lv_draw_vector_dsc_add_path(dsc, span->path);",r"""    _aic_svg_opacity_scope scope;
    lv_draw_vector_dsc_t *draw=_aic_svg_opacity_begin(obj,dsc,&scope);
    if(draw) {
        lv_draw_vector_dsc_add_path(draw,span->path);
        _aic_svg_opacity_end(dsc,draw,&scope);
    }""")
    start=text.index("void lv_draw_svg_render(")
    end=text.index('\nvoid lv_draw_svg(',start)
    text=text[:start]+r"""
void lv_draw_svg_render(lv_draw_vector_dsc_t *dsc,const lv_svg_render_obj_t *render)
{
    if(!render || !dsc) return;
    /* Budget is cumulative per submission, including pending sibling layers.
     * Native LVGL layer accounting remains the global lifetime owner. */
    uint64_t saved_bytes=_aic_svg_layer_bytes;
    uint32_t saved_depth=_aic_svg_render_depth;
    lv_opa_t saved_opacity=_aic_svg_parent_opacity;
    _aic_svg_layer_bytes=0;_aic_svg_render_depth=0;_aic_svg_parent_opacity=255;
    _aic_svg_opacity_scope scope;
    const lv_svg_render_obj_t *root=render->tag==LV_SVG_TAG_SVG?render:NULL;
    lv_draw_vector_dsc_t *draw=_aic_svg_opacity_begin(root,dsc,&scope);
    if(draw) {
        for(const lv_svg_render_obj_t *cur=render;cur;cur=cur->next) {
            if(cur->clz->render && ((cur->flags&3)==_RENDER_NORMAL)) {
                _prepare_render(cur,draw);
                if(cur==root) cur->clz->render(cur,draw,NULL);
                else _aic_svg_render_object(cur,draw,NULL);
            }
        }
        _aic_svg_opacity_end(dsc,draw,&scope);
    }
    _aic_svg_layer_bytes=saved_bytes;_aic_svg_render_depth=saved_depth;_aic_svg_parent_opacity=saved_opacity;
}
"""+text[end:]
    return text
