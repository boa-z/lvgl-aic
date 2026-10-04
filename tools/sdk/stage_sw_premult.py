#!/usr/bin/env python3
# SPDX-License-Identifier: Apache-2.0
"""Normalize flagged premultiplied source formats in pinned LVGL software drawing."""
import argparse
import hashlib
from pathlib import Path

REVIEWED_SHA256 = "bbf0a2d3a9e7d6d26f9038524382ce36d195d27007ca0be89705a05e321815a1"

def corrected(text):
    text = text.replace("\r\n", "\n")
    if hashlib.sha256(text.encode("utf-8")).hexdigest() != REVIEWED_SHA256:
        raise ValueError("LVGL software image source changed; review premultiplied correction")
    old = "    lv_color_format_t cf = decoded->header.cf;"
    if text.count(old) != 4:
        raise ValueError("Unexpected software image format sites")
    text = text.replace(old, old + "\n    if (cf == LV_COLOR_FORMAT_ARGB8888 &&\n"
        "        (decoded->header.flags & LV_IMAGE_FLAGS_PREMULTIPLIED))\n"
        "        cf = LV_COLOR_FORMAT_ARGB8888_PREMULTIPLIED;")

    # Alpha-only masking is valid for straight pixels. Premultiplied layers
    # must scale RGB as well or transparent masked pixels keep emitting colour.
    mask = "            img_start[x * 4 + 3] = LV_OPA_MIX2(mask_start[x], img_start[x * 4 + 3]);"
    if text.count(mask) != 1:
        raise ValueError("Unexpected software layer mask site")
    text = text.replace(mask, """            if(image_draw_buf->header.cf == LV_COLOR_FORMAT_ARGB8888_PREMULTIPLIED ||
               (image_draw_buf->header.flags & LV_IMAGE_FLAGS_PREMULTIPLIED)) {
                for(unsigned channel = 0; channel < 3; channel++)
                    img_start[x * 4 + channel] = LV_OPA_MIX2(mask_start[x], img_start[x * 4 + channel]);
            }
""" + mask)
    return text

def generate(source, destination):
    result = corrected(Path(source).read_text(encoding="utf-8"))
    destination = Path(destination)
    destination.parent.mkdir(parents=True, exist_ok=True)
    if not destination.exists() or destination.read_text(encoding="utf-8") != result:
        destination.write_bytes(result.encode("utf-8"))
    return str(destination)

if __name__ == "__main__":
    parser = argparse.ArgumentParser()
    parser.add_argument("--source", required=True)
    parser.add_argument("--output", required=True)
    args = parser.parse_args()
    generate(args.source, args.output)
