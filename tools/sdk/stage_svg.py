#!/usr/bin/env python3
# SPDX-License-Identifier: Apache-2.0
"""Build-local corrections for pinned SVG image coordinates and clipping."""
import argparse
import hashlib
from pathlib import Path

SOURCES = {
    "draw": ("src/draw/lv_draw_image.c", "eda7a142395efcb4abe16a32e1613dbc823bff5373ccabd393321594b0b5bdfc"),
    "decoder": ("src/image/svg/lv_svg_decoder.c", "fb3543e2c4cf3f687ed7f48faac2bdb8fa281ec25f5202bd2a81111360c30076"),
}

def corrected(text, kind):
    text = text.replace("\r\n", "\n")
    if hashlib.sha256(text.encode("utf-8")).hexdigest() != SOURCES[kind][1]:
        raise ValueError("LVGL SVG source changed; review custom image coordinates")
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
                /* Native ThorVG writes premultiplied pixels. A vector-only
                 * temporary layer also protects straight-alpha destinations
                 * containing earlier non-vector drawing. */
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
                        /* ThorVG writes premultiplied pixels into this otherwise
                         * empty, vector-only layer. Publish that layout to both
                         * GE and software composition. */
                        raster->draw_buf->header.flags |= LV_IMAGE_FLAGS_PREMULTIPLIED;
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
