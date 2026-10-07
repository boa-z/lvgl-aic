#!/usr/bin/env python3
# SPDX-License-Identifier: Apache-2.0
"""Build-local corrections for pinned SVG coordinates, references and ownership."""
import argparse
import hashlib
from pathlib import Path
import runpy

# run_path is also used by SCons, where this directory is not on sys.path.
corrected_layers = runpy.run_path(str(Path(__file__).with_name('stage_svg_layers.py')))['corrected_layers']

SOURCES = {
    "draw": ("src/draw/lv_draw_image.c", "eda7a142395efcb4abe16a32e1613dbc823bff5373ccabd393321594b0b5bdfc"),
    "render": ("src/image/svg/lv_svg_render.c", "a8ccd5e0f3c8639494155d07cfcd28d164875f6b83dcced47aa549b4fc6d23f4"),
    "decoder": ("src/image/svg/lv_svg_decoder.c", "fb3543e2c4cf3f687ed7f48faac2bdb8fa281ec25f5202bd2a81111360c30076"),
}

def corrected_render(text):
    # Builder stack nodes and saved render contexts own their dash arrays.
    text = text.replace("    struct _lv_svg_draw_dsc * cur = dsc->next;\n    lv_free(dsc);", "    struct _lv_svg_draw_dsc * cur = dsc->next;\n    _deinit_draw_dsc(&dsc->dsc);\n    lv_free(dsc);")
    # Saved stroke dash arrays are owned copies, including nested groups/spans.
    text = text.replace("    _restore_matrix(&mtx, dsc);\n}\n\nstatic void _render_image", "    _deinit_draw_dsc(&save_dsc.dsc);\n    _restore_matrix(&mtx, dsc);\n}\n\nstatic void _render_image")
    text = text.replace("    _copy_draw_dsc(dsc->ctx, &(save_dsc.dsc));\n}\n#endif", "    _copy_draw_dsc(dsc->ctx, &(save_dsc.dsc));\n    _deinit_draw_dsc(&save_dsc.dsc);\n}\n#endif")
    text = text.replace("    if(!image->img_dsc.header.w || !image->img_dsc.header.h || !image->img_dsc.src) {\n        return;", "    if(!image->img_dsc.header.w || !image->img_dsc.header.h || !image->img_dsc.src) {\n        _restore_matrix(&imtx, dsc);\n        return;")
    start = text.index("static void _render_use(")
    end = text.index("\n#if LV_USE_FREETYPE", start)
    text = text[:start] + """/* SVG construction/rendering is serialized on the LVGL owner. Stack entries
 * span nested group/reference rendering and are removed on every exit. A
 * malformed reference cannot consume unbounded target stack or recurse into itself.
 * This is a reference-chain bound, not a parser/whole-document memory quota. */
#define AIC_SVG_REFERENCE_DEPTH 32
static bool _aic_svg_reference_enter(const lv_svg_render_obj_t * obj,
                                    const lv_svg_render_obj_t ** stack, uint32_t * depth)
{
    if(*depth == AIC_SVG_REFERENCE_DEPTH) return false;
    for(uint32_t i = 0; i < *depth; i++) if(stack[i] == obj) return false;
    stack[(*depth)++] = obj;
    return true;
}

static void _render_use(const lv_svg_render_obj_t * obj, lv_draw_vector_dsc_t * dsc, const lv_matrix_t * matrix)
{
    static const lv_svg_render_obj_t * active[AIC_SVG_REFERENCE_DEPTH];
    static uint32_t depth;
    const lv_svg_render_use_t * use = (const lv_svg_render_use_t *)obj;
    if(!use->xlink || !_aic_svg_reference_enter(obj, active, &depth)) return;
    lv_matrix_t saved;
    _setup_matrix(&saved, dsc, obj);
    if(matrix) lv_matrix_multiply(&dsc->ctx->matrix, matrix);
    /* <use> translation precedes the referenced element and descendant
     * transforms. Passing it to a leaf instead scales/rotates the translation. */
    lv_matrix_translate(&dsc->ctx->matrix, use->x, use->y);
    lv_svg_render_obj_t * ref = obj->head;
    while(ref) {
        if(ref->id && strcmp(use->xlink, ref->id) == 0) {
            if(ref->clz->render) {
                /* The shadow subtree inherits use styles. Explicit referenced
                 * element attributes override that inherited context. */
                _copy_draw_dsc_from_ref(dsc, obj);
                _special_render(ref, dsc);
                ref->clz->render(ref, dsc, NULL);
            }
            break;
        }
        ref = ref->next;
    }
    _restore_matrix(&saved, dsc);
    active[--depth] = NULL;
}
""" + text[end:]
    start = text.index("static void _get_use_bounds(")
    end = text.index("\n#if LV_USE_FREETYPE", start)
    text = text[:start] + """static void _get_use_bounds(const lv_svg_render_obj_t * obj, lv_area_t * area)
{
    static const lv_svg_render_obj_t * active[AIC_SVG_REFERENCE_DEPTH];
    static uint32_t depth;
    const lv_svg_render_use_t * use = (const lv_svg_render_use_t *)obj;
    lv_memzero(area, sizeof(*area));
    if(!use->xlink || !_aic_svg_reference_enter(obj, active, &depth)) return;
    const lv_svg_render_obj_t * ref = obj->head;
    while(ref) {
        if(ref->id && strcmp(use->xlink, ref->id) == 0) {
            if(ref->clz->get_bounds) ref->clz->get_bounds(ref, area);
            break;
        }
        ref = ref->next;
    }
    active[--depth] = NULL;
}
""" + text[end:]
    return corrected_layers(text)

def corrected(text, kind):
    text = text.replace("\r\n", "\n")
    if hashlib.sha256(text.encode("utf-8")).hexdigest() != SOURCES[kind][1]:
        raise ValueError("LVGL SVG source changed; review staged SVG corrections")
    if kind == "render":
        return corrected_render(text)
    if kind == "draw":
        start = text.index("            lv_area_t draw_area = layer->buf_area;")
        end = text.index("\n\n        }\n\n        lv_image_decoder_close", start)
        text = text[:start] + '''            /* Keep source coordinates separate from transformed bounds. A
             * canvas draw has no object; a child layer still uses absolute
             * coordinates. Never replace the caller's clip with image bounds. */
            lv_area_t bounds;
            lv_image_buf_get_transformed_area(&bounds, lv_area_get_width(coords), lv_area_get_height(coords),
                                              dsc->rotation, dsc->scale_x, dsc->scale_y, &dsc->pivot);
            lv_area_move(&bounds, coords->x1, coords->y1);
            lv_area_t clip_area;
            if(lv_area_intersect(&clip_area, &layer->_clip_area, &layer->buf_area) &&
               lv_area_intersect(&clip_area, &clip_area, &bounds)) {
                /* Isolate SVG effects from previous destination drawing. The
                 * staged vector backend preserves the target pixel format. */
                bool compose = decoder_dsc.decoder->name && !lv_strcmp(decoder_dsc.decoder->name, "SVG");
                if(!compose) {
                    decoder_dsc.decoder->custom_draw_cb(layer, &decoder_dsc, coords, &new_image_dsc, &clip_area);
                }
                else {
                    /* Apply image effects once after overlapping vector shapes
                     * have been composed. Non-tiled sources stay at destination
                     * resolution; tiles use one reusable intrinsic-size image. */
                    lv_area_t raster_area = clip_area;
                    lv_area_t raster_coords = *coords;
                    lv_draw_image_dsc_t vector_dsc = new_image_dsc;
                    if(new_image_dsc.tile) {
                        lv_area_set(&raster_area, 0, 0, new_image_dsc.header.w - 1, new_image_dsc.header.h - 1);
                        raster_coords = raster_area;
                        vector_dsc.rotation = 0;
                        vector_dsc.scale_x = vector_dsc.scale_y = LV_SCALE_NONE;
                        vector_dsc.pivot = (lv_point_t){0, 0};
                    }
                    uint64_t raster_bytes = (uint64_t)lv_area_get_height(&raster_area) *
                        lv_draw_buf_width_to_stride(lv_area_get_width(&raster_area), LV_COLOR_FORMAT_ARGB8888);
                    lv_layer_t * raster = raster_bytes && raster_bytes <= UINT32_MAX ?
                        lv_draw_layer_create(layer, LV_COLOR_FORMAT_ARGB8888, &raster_area) : NULL;
                    if(raster && lv_draw_layer_alloc_buf(raster)) {
                        /* Straight ARGB is shared by vector and nested opacity
                         * layers; native layer composition writes straight RGB. */
                        raster->opa = LV_OPA_COVER;
                        raster->recolor = lv_color32_make(0, 0, 0, 0);
                        vector_dsc.opa = LV_OPA_COVER;
                        vector_dsc.recolor_opa = LV_OPA_TRANSP;
                        vector_dsc.clip_radius = 0;
                        vector_dsc.bitmap_mask_src = NULL;
                        vector_dsc.colorkey = NULL;
                        vector_dsc.tile = false;
                        vector_dsc.blend_mode = LV_BLEND_MODE_NORMAL;
                        decoder_dsc.decoder->custom_draw_cb(raster, &decoder_dsc, &raster_coords, &vector_dsc, &raster_area);
                        lv_draw_image_dsc_t composite = new_image_dsc;
                        composite.src = raster;
                        composite.header = raster->draw_buf->header;
                        if(!composite.tile) {
                            composite.rotation = 0;
                            composite.scale_x = composite.scale_y = LV_SCALE_NONE;
                            composite.pivot = (lv_point_t){0, 0};
                        }
                        lv_draw_layer(layer, &composite, composite.tile ? coords : &raster_area);
                    }
                    else {
                        lv_draw_layer_delete(raster);
                        LV_LOG_WARN("SVG image effect layer allocation failed");
                    }
                }
            }''' + text[end:]
    else:
        start = text.index("        int32_t off_x =")
        end = text.index("        lv_matrix_translate(&matrix, image_dsc->pivot.x", start)
        text = text[:start] + text[end:]
    return text

def generate(root, destination):
    destination = Path(destination)
    destination.mkdir(parents=True, exist_ok=True)
    outputs = []
    for kind, (relative, _) in SOURCES.items():
        result = corrected((Path(root) / relative).read_text(encoding="utf-8"), kind)
        output = destination / ("lvgl-svg-" + kind + ".c")
        if not output.exists() or output.read_text(encoding="utf-8") != result:
            output.write_bytes(result.encode("utf-8"))
        outputs.append(str(output))
    return outputs

if __name__ == "__main__":
    parser = argparse.ArgumentParser()
    parser.add_argument("--root", required=True)
    parser.add_argument("--output", required=True)
    args = parser.parse_args()
    generate(args.root, args.output)
