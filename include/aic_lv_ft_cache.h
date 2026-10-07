/* SPDX-License-Identifier: Apache-2.0 */
#ifndef AIC_LV_FT_CACHE_H
#define AIC_LV_FT_CACHE_H
#include "lvgl_aic.h"
#ifndef AIC_LVGL_USE_FT_CACHE
#define AIC_LVGL_USE_FT_CACHE 0
#endif
#ifdef __cplusplus
extern "C" {
#endif
#if LV_USE_FREETYPE && AIC_LVGL_USE_FT_CACHE
#define LV_FREETYPE_FONT_STYLE_ANY 0xffff
#define LV_FREETYPE_FONT_RENDER_MODE_ANY 0xffff
typedef enum {
    AIC_LV_FT_CACHE_TYPE_GLYPH=1,
    AIC_LV_FT_CACHE_TYPE_DRAW_DATA=2,
    AIC_LV_FT_CACHE_TYPE_ALL=3
} aic_lv_ft_cache_type_t;
/* Counts of native LRU entries/references, NOT bytes or FreeType heap usage. */
typedef struct {
    uint32_t fonts, font_references;
    uint32_t glyph_entries, glyph_capacity, glyph_references;
    uint32_t draw_entries, draw_capacity, draw_references;
} aic_lv_ft_cache_stats_t;
/* Call between refreshes with all draw workers idle, on the LVGL owner thread
 * (or under its external lock). Serialize font create/delete and glyph access.
 * NULL path and ANY enums are wildcards; other conditions combine with AND.
 * A failed query leaves stats unchanged. Invalid masks are rejected.
 * Drop preflights every matched selected cache: referenced glyph/draw entries
 * or allocation failure return INVALID before eviction. Font objects survive.
 * No deferred purge, automatic invalidation or global byte limit is implied. */
lv_result_t aic_lv_ft_cache_get_stats(const char *pathname, lv_freetype_font_style_t style,
    lv_freetype_font_render_mode_t render_mode, aic_lv_ft_cache_stats_t *stats);
lv_result_t aic_lv_ft_cache_print_stats(void);
lv_result_t aic_lv_ft_cache_drop_all(aic_lv_ft_cache_type_t cache_type);
lv_result_t aic_lv_ft_cache_drop_specific(const char *pathname, lv_freetype_font_style_t style,
    lv_freetype_font_render_mode_t render_mode, aic_lv_ft_cache_type_t cache_type);
#endif
#ifdef __cplusplus
}
#endif
#endif
