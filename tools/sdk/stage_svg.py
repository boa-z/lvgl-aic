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
                decoder_dsc.decoder->custom_draw_cb(layer, &decoder_dsc, coords, &new_image_dsc, &clip_area);
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
