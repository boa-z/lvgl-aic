/* SPDX-License-Identifier: Apache-2.0 */
/* LVGL v8/9.0 names the official SDK demos still use but that LVGL 9.6's own
 * api_map headers no longer provide. Each entry is a plain rename to the 9.6
 * API; add one here instead of editing a vendored demo.
 *
 * Force-included into every official demo source (-include, see the component
 * SConscript and tests/host/CMakeLists.txt): some demo files never include
 * aic_ui.h. Hence self-contained, LVGL first. */
#ifndef LV_AIC_V8_COMPAT_H
#define LV_AIC_V8_COMPAT_H

#include "lvgl.h"
/* LVGL 9.2+ moved widget/theme/draw-task structs and the image cache API to
 * private headers; demos (demo_hub) still dereference them directly. */
#include "lvgl_private.h"
/* The SDK's widget lv_aic_player.h also pulls in the MPP player header, and
 * demo_hub's audio/video apps rely on that for struct av_media_info and u64.
 * The component's lv_aic_player.h does not, so restore it here. */
#if defined(__has_include)
#if __has_include("aic_player.h")
#include "aic_player.h"
#endif
#endif

#ifndef lv_style_set_arc_img_src
#define lv_style_set_arc_img_src lv_style_set_arc_image_src /* dashboard_demo */
#endif

/* v8 memory API (multi_lang_demo) */
#ifndef lv_mem_alloc
#define lv_mem_alloc lv_malloc
#endif
#ifndef lv_mem_free
#define lv_mem_free lv_free
#endif

/* v8 image API (demo_hub) */
#ifndef lv_img_decoder_get_info
#define lv_img_decoder_get_info lv_image_decoder_get_info
#endif
#ifndef lv_obj_set_style_bg_img_opa
#define lv_obj_set_style_bg_img_opa lv_obj_set_style_bg_image_opa
#endif
/* Demos rewrite image sources in place and invalidate them first. The
 * component's MPP decoder caches by source (file path or descriptor address),
 * which LVGL's own cache drop does not reach: drop both. */
void lv_aic_official_image_drop(const void *src);
#ifndef lv_img_cache_invalidate_src
#define lv_img_cache_invalidate_src lv_aic_official_image_drop
#endif
/* v8 sized the image cache in entries; LVGL 9 sizes it in bytes, and here the
 * cache that holds decoded JPEG/PNG is the component's MPP cache, whose budget
 * the demo gate sets while a demo runs. Kept as an accepted no-op. */
#ifndef lv_img_cache_set_size
#define lv_img_cache_set_size(entries) ((void)(entries))
#endif

/* LVGL 9 replaced the built-in SimSun CJK font with Source Han Sans SC
 * (multi_lang_demo opts in through LV_FONT_SIMSUN_16_CJK; lv_conf.h enables
 * both switches together). */
#if defined(LV_FONT_SIMSUN_16_CJK) && LV_FONT_SIMSUN_16_CJK && \
    defined(LV_FONT_SOURCE_HAN_SANS_SC_16_CJK) && LV_FONT_SOURCE_HAN_SANS_SC_16_CJK
#define lv_font_simsun_16_cjk lv_font_source_han_sans_sc_16_cjk
#endif

#endif /* LV_AIC_V8_COMPAT_H */
