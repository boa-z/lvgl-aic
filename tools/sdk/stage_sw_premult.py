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
    # Invalid full-buffer mask footprints must not reach the pixel loop.
    mask_view = "    const lv_draw_buf_t * mask_draw_buf = mask_decoder_dsc.decoded;"
    if text.count(mask_view) != 1:
        raise ValueError("Unexpected software mask view site")
    text = text.replace(mask_view, mask_view + """
    if(!mask_draw_buf->data || !mask_draw_buf->header.w || !mask_draw_buf->header.h ||
       mask_draw_buf->header.stride < mask_draw_buf->header.w ||
       (uint64_t)mask_draw_buf->header.stride * mask_draw_buf->header.h > mask_draw_buf->data_size) {
        lv_image_decoder_close(&mask_decoder_dsc);
        return true; /* Match the native invalid-mask policy: draw unmasked. */
    }
""")
    # Reuse the reviewed native pixel kernel for bounded GE source preparation.
    # Only row storage changes: no duplicate recolor/alpha rounding algorithm.
    text += r"""
#if LV_USE_DRAW_SW
bool lv_aic_sw_recolor_copy(const lv_draw_buf_t *src, lv_draw_buf_t *dst,
                            lv_color_t color, lv_opa_t opacity)
{
    if(!src || !dst || !src->data || !dst->data || !src->header.w || !src->header.h)
        return false;
    lv_color_format_t cf=src->header.cf, out=dst->header.cf;
    if(cf==LV_COLOR_FORMAT_ARGB8888 && (src->header.flags&LV_IMAGE_FLAGS_PREMULTIPLIED))
        cf=LV_COLOR_FORMAT_ARGB8888_PREMULTIPLIED;
    if(out==LV_COLOR_FORMAT_ARGB8888 && (dst->header.flags&LV_IMAGE_FLAGS_PREMULTIPLIED))
        out=LV_COLOR_FORMAT_ARGB8888_PREMULTIPLIED;
    if(cf!=out || (cf!=LV_COLOR_FORMAT_RGB565 && cf!=LV_COLOR_FORMAT_RGB888 &&
       cf!=LV_COLOR_FORMAT_XRGB8888 && cf!=LV_COLOR_FORMAT_ARGB8888 &&
       cf!=LV_COLOR_FORMAT_ARGB8888_PREMULTIPLIED)) return false;
    uint32_t row=src->header.w*lv_color_format_get_size(cf);
    if(dst->header.w!=src->header.w || dst->header.h!=src->header.h ||
       row>src->header.stride || row>dst->header.stride ||
       (uint64_t)src->header.stride*src->header.h>src->data_size ||
       (uint64_t)dst->header.stride*dst->header.h>dst->data_size) return false;
    lv_draw_image_dsc_t effect={0};effect.recolor=color;effect.recolor_opa=opacity;
    for(uint32_t y=0;y<src->header.h;y++) {
        uint8_t *to=dst->data+y*dst->header.stride;
        if(opacity<=LV_OPA_MIN) lv_memcpy(to,src->data+y*src->header.stride,row);
        else {
            lv_area_t area={0,(int32_t)y,(int32_t)src->header.w-1,(int32_t)y};
            recolor(area,src->data,to,src->header.stride,cf,&effect);
        }
    }
    return true;
}

/* Native layer masking on a private copy: -1 invalid, 0 empty, 1 drawable.
 * Source area is the partial layer extent; image_area remains the whole image
 * extent used by LVGL to center its mask. apply_mask includes the reviewed
 * premultiplied RGB correction above. */
int lv_aic_sw_layer_mask_copy(const lv_draw_image_dsc_t *dsc, const lv_area_t *area,
                              const lv_draw_buf_t *src, lv_draw_buf_t *dst)
{
    if(!dsc || !dsc->bitmap_mask_src || !area || !src || !dst || src->data==dst->data ||
       (src->header.cf!=LV_COLOR_FORMAT_ARGB8888 &&
        src->header.cf!=LV_COLOR_FORMAT_ARGB8888_PREMULTIPLIED) ||
       lv_area_get_width(area)!=(int32_t)src->header.w ||
       lv_area_get_height(area)!=(int32_t)src->header.h ||
       !lv_aic_sw_recolor_copy(src,dst,lv_color_black(),LV_OPA_TRANSP)) return -1;
    lv_layer_t layer={0};layer.draw_buf=dst;layer.buf_area=*area;
    lv_draw_image_dsc_t copy=*dsc;copy.src=&layer;
    return apply_mask(&copy)?1:0;
}

/* Ordinary IMAGE masking uses the same reviewed LVGL mask kernel after a
 * private ARGB8888 conversion. GE accepts the resulting four-byte source;
 * RGB images receive opaque alpha and both straight and premultiplied ARGB
 * encodings retain their original channels. */
int lv_aic_sw_image_mask_copy(const lv_draw_image_dsc_t *dsc, const lv_area_t *area,
                              const lv_draw_buf_t *src, lv_draw_buf_t *dst)
{
    if(!dsc || !dsc->bitmap_mask_src || !area || !src || !dst || src->data==dst->data ||
       !src->data || !dst->data || !src->header.w || !src->header.h ||
       lv_area_get_width(area)!=(int32_t)src->header.w ||
       lv_area_get_height(area)!=(int32_t)src->header.h ||
       dst->header.w!=src->header.w || dst->header.h!=src->header.h ||
       (dst->header.cf!=LV_COLOR_FORMAT_ARGB8888 &&
        dst->header.cf!=LV_COLOR_FORMAT_ARGB8888_PREMULTIPLIED)) return -1;
    lv_color_format_t cf=src->header.cf;
    if(cf==LV_COLOR_FORMAT_ARGB8888 && (src->header.flags&LV_IMAGE_FLAGS_PREMULTIPLIED))
        cf=LV_COLOR_FORMAT_ARGB8888_PREMULTIPLIED;
    if(cf!=LV_COLOR_FORMAT_RGB565 && cf!=LV_COLOR_FORMAT_RGB888 &&
       cf!=LV_COLOR_FORMAT_XRGB8888 && cf!=LV_COLOR_FORMAT_ARGB8888 &&
       cf!=LV_COLOR_FORMAT_ARGB8888_PREMULTIPLIED) return -1;
    uint32_t src_row=src->header.w*lv_color_format_get_size(cf);
    if(src_row>src->header.stride || dst->header.stride<src->header.w*4U ||
       (uint64_t)src->header.stride*src->header.h>src->data_size ||
       (uint64_t)dst->header.stride*dst->header.h>dst->data_size) return -1;
    for(uint32_t y=0;y<src->header.h;y++) {
        const uint8_t *from=src->data+y*src->header.stride;
        uint8_t *to=dst->data+y*dst->header.stride;
        for(uint32_t x=0;x<src->header.w;x++) {
            const uint8_t *p=from+x*lv_color_format_get_size(cf);
            uint8_t *q=to+x*4U;
            if(cf==LV_COLOR_FORMAT_RGB565) {
                uint16_t v=(uint16_t)p[0]|((uint16_t)p[1]<<8);
                q[0]=(uint8_t)((v&0x1f)<<3);
                q[1]=(uint8_t)(((v>>5)&0x3f)<<2);
                q[2]=(uint8_t)((v>>11)<<3); q[3]=255;
            }
            else {
                q[0]=p[0]; q[1]=p[1]; q[2]=p[2];
                q[3]=(cf==LV_COLOR_FORMAT_RGB888 || cf==LV_COLOR_FORMAT_XRGB8888)?255:p[3];
            }
        }
    }
    lv_layer_t layer={0};layer.draw_buf=dst;layer.buf_area=*area;
    lv_draw_image_dsc_t copy=*dsc;copy.src=&layer;
    if(copy.image_area.x2==LV_COORD_MIN) copy.image_area=*area;
    return apply_mask(&copy)?1:0;
}

/* Convert a bounded source to straight ARGB8888 while applying LVGL's
 * inclusive RGB color-key range.  This is deliberately a source preparation
 * helper: the GE comparator is single-value and its RGB565 comparison space
 * is not specified by the SDK headers.  Keeping the reviewed LVGL range
 * comparison here makes transformed and filtered keys deterministic before
 * the native engine sees the pixels. */
bool lv_aic_sw_colorkey_copy(const lv_draw_buf_t *src, lv_draw_buf_t *dst,
                             const lv_image_colorkey_t *key)
{
    if(!src || !dst || !key || !src->data || !dst->data ||
       !src->header.w || !src->header.h || dst->header.w!=src->header.w ||
       dst->header.h!=src->header.h || dst->header.cf!=LV_COLOR_FORMAT_ARGB8888)
        return false;
    lv_color_format_t cf=src->header.cf;
    if(cf==LV_COLOR_FORMAT_ARGB8888 && (src->header.flags&LV_IMAGE_FLAGS_PREMULTIPLIED))
        cf=LV_COLOR_FORMAT_ARGB8888_PREMULTIPLIED;
    if(cf!=LV_COLOR_FORMAT_RGB565 && cf!=LV_COLOR_FORMAT_RGB888 &&
       cf!=LV_COLOR_FORMAT_XRGB8888 && cf!=LV_COLOR_FORMAT_ARGB8888 &&
       cf!=LV_COLOR_FORMAT_ARGB8888_PREMULTIPLIED) return false;
    uint32_t src_bpp=lv_color_format_get_size(cf), row=src->header.w*src_bpp;
    if(row>src->header.stride || dst->header.stride<src->header.w*4U ||
       (uint64_t)src->header.stride*src->header.h>src->data_size ||
       (uint64_t)dst->header.stride*dst->header.h>dst->data_size) return false;
    for(uint32_t y=0;y<src->header.h;y++) {
        const uint8_t *from=src->data+y*src->header.stride;
        uint8_t *to=dst->data+y*dst->header.stride;
        for(uint32_t x=0;x<src->header.w;x++) {
            const uint8_t *p=from+x*src_bpp; uint8_t *q=to+x*4U;
            lv_color_t color; uint8_t alpha=255;
            if(cf==LV_COLOR_FORMAT_RGB565) {
                color=lv_color16_to_color(*(const lv_color16_t *)p);
            }
            else {
                alpha=(cf==LV_COLOR_FORMAT_ARGB8888 ||
                       cf==LV_COLOR_FORMAT_ARGB8888_PREMULTIPLIED)?p[3]:255;
                uint8_t b=p[0],g=p[1],r=p[2];
                if(cf==LV_COLOR_FORMAT_ARGB8888_PREMULTIPLIED && alpha) {
                    r=(uint8_t)(((uint32_t)r*255U+alpha/2U)/alpha);
                    g=(uint8_t)(((uint32_t)g*255U+alpha/2U)/alpha);
                    b=(uint8_t)(((uint32_t)b*255U+alpha/2U)/alpha);
                }
                color=lv_color_make(r,g,b);
            }
            bool match=lv_color_is_in_range(color,key->low,key->high);
            if(match) { q[0]=q[1]=q[2]=q[3]=0; }
            else {
                q[0]=color.blue; q[1]=color.green; q[2]=color.red; q[3]=alpha;
            }
        }
    }
    return true;
}
#endif
"""
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
