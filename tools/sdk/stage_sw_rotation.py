#!/usr/bin/env python3
# SPDX-License-Identifier: Apache-2.0
"""Preserve Q15 rotation coefficients in pinned LVGL geometry and SW sampling."""
import argparse
import hashlib
from pathlib import Path

SOURCES = {
    "area": ("src/misc/lv_area.c", "890240acfff75ab84d2517ff76d1f36cbb63adb31192b78215bbf6a3c05c217e"),
    "transform": ("src/draw/sw/lv_draw_sw_transform.c", "57a22b972ed7ddb844a700d8c03224d52f83dbe2b09b7e9e8934d94b9b23cf4f"),
}


def corrected(text, kind):
    text = text.replace("\r\n", "\n")
    if hashlib.sha256(text.encode("utf-8")).hexdigest() != SOURCES[kind][1]:
        raise ValueError("LVGL rotation source changed; review Q15 correction: " + kind)
    # Keep the tenths numerator until both axes have been normalized. Rounding
    # each axis before normalization amplifies table error at remote pivots.
    helper = """static int32_t lv_aic_rotation_interpolate(int32_t a, int32_t b, int32_t rem)
{
    return a * (10 - rem) + b * rem;
}

/* The inputs retain a factor of ten from degree interpolation. One inverse
 * square-root step about unit length removes chord contraction, then a single
 * symmetric rounding yields Q15. Cardinal values stay exact. */
static void lv_aic_rotation_normalize(int32_t * s, int32_t * c)
{
    const int64_t unit2 = 100LL * 1073741824;
    int64_t norm = (int64_t)*s * *s + (int64_t)*c * *c;
    int64_t factor = 3 * unit2 - norm;
    int64_t sn = *s * factor, cn = *c * factor;
    *s = (int32_t)((sn + (sn < 0 ? -10 * unit2 : 10 * unit2)) / (20 * unit2));
    *c = (int32_t)((cn + (cn < 0 ? -10 * unit2 : 10 * unit2)) / (20 * unit2));
}

"""
    marker = "void lv_point_transform(" if kind == "area" else "void lv_draw_sw_transform("
    text = text.replace(marker, helper + marker, 1)
    text = text.replace("(s1 * (10 - angle_rem) + s2 * angle_rem) / 10",
                        "lv_aic_rotation_interpolate(s1, s2, angle_rem)")
    text = text.replace("(c1 * (10 - angle_rem) + c2 * angle_rem) / 10",
                        "lv_aic_rotation_interpolate(c1, c2, angle_rem)")
    if kind == "area":
        text = text.replace("#define LV_TRANSFORM_TRIGO_SHIFT 10", "#define LV_TRANSFORM_TRIGO_SHIFT LV_TRIGO_SHIFT")
        start = text.index("    uint32_t i;", text.index("void lv_point_array_transform("))
        stop = text.index("    if(angle == 0) {", start)
        text = text[:start] + "    size_t i;\n\n" + text[stop:]
        text = text.replace("((int32_t)(points[i].x) * scale_x)", "(((int64_t)points[i].x - pivot->x) * scale_x)")
        text = text.replace("((int32_t)(points[i].y) * scale_y)", "(((int64_t)points[i].y - pivot->y) * scale_y)")
        text = text.replace("int32_t angle_limited = angle;\n    if(angle_limited > 3600) angle_limited -= 3600;",
                            "int32_t angle_limited = angle % 3600;")
        text = text.replace("    cosma = cosma >> (LV_TRIGO_SHIFT - LV_TRANSFORM_TRIGO_SHIFT);",
                            "    lv_aic_rotation_normalize(&sinma, &cosma);")
        text = text.replace("int32_t x = points[i].x;", "int64_t x = (int64_t)points[i].x - pivot->x;")
        text = text.replace("int32_t y = points[i].y;", "int64_t y = (int64_t)points[i].y - pivot->y;")
    else:
        text = text.replace("tr_dsc.angle = -draw_dsc->rotation;",
                            "tr_dsc.angle = (3600 - draw_dsc->rotation % 3600) % 3600;")
        text = text.replace("    tr_dsc.sinma = tr_dsc.sinma >> (LV_TRIGO_SHIFT - 10);\n", "")
        text = text.replace("    tr_dsc.cosma = tr_dsc.cosma >> (LV_TRIGO_SHIFT - 10);",
                            "    lv_aic_rotation_normalize(&tr_dsc.sinma, &tr_dsc.cosma);")
        text = text.replace("the table gives 1023 for cos(0)", "Q15 preserves exact cardinal values")
        text = text.replace("cosma = 1024;", "cosma = 32768;")
        text = text.replace(" * 16384)", " * 512)")
        text = text.replace(" * 64) / tr_dsc.scale_", " * 2) / tr_dsc.scale_")
        # Q16 steps can overflow a 32-bit column product even when the final
        # source coordinate is small (remote-pivot cancellation).
        text = text.replace("xs_step * x_abs", "(int64_t)xs_step * x_abs")
        text = text.replace("ys_step * x_abs", "(int64_t)ys_step * x_abs")
    return text


def generate(root, output):
    outputs = []
    for kind, (relative, _) in SOURCES.items():
        result = corrected((Path(root) / relative).read_text(encoding="utf-8"), kind)
        destination = Path(output) / ("lvgl-sw-" + kind + ".c")
        destination.parent.mkdir(parents=True, exist_ok=True)
        if not destination.exists() or destination.read_text(encoding="utf-8") != result:
            destination.write_bytes(result.encode("utf-8"))
        outputs.append(str(destination))
    return outputs


if __name__ == "__main__":
    parser = argparse.ArgumentParser()
    parser.add_argument("--root", required=True)
    parser.add_argument("--output", required=True)
    args = parser.parse_args()
    generate(args.root, args.output)
