/* SPDX-License-Identifier: Apache-2.0
 * SDK-compatible cache controls implemented against the LVGL 9.6 boundary.
 * No mirrored LRU/tree/face-list structs or SDK source modifications. */
#include "aic_lv_ft_cache.h"
#if LV_USE_FREETYPE && AIC_LVGL_USE_FT_CACHE
#include "src/font/freetype/lv_freetype_private.h"
#include "src/misc/cache/lv_cache_entry.h"
#include "src/misc/lv_iter_private.h"
#include <limits.h>
#include <string.h>
#include <stdio.h>

typedef struct {
    lv_freetype_cache_node_t key;
    lv_cache_entry_t *entry;
#if LV_FREETYPE_CACHE_FT_GLYPH_L1
    void *fresh_l1;
#endif
} selected_t;

static bool add(uint32_t *value,size_t n)
{
    if(n>UINT32_MAX-*value) return false;
    *value+=(uint32_t)n;return true;
}
static lv_iter_t *iterator(lv_cache_t *cache)
{
    lv_iter_t *it=lv_cache_iter_create(cache);
    /* The pinned upstream iterator may return a header after context OOM. */
    if(it && !lv_iter_get_context(it)) { lv_iter_destroy(it);return NULL; }
    return it;
}
static bool counts(lv_cache_t *cache,uint32_t *entries,uint32_t *capacity,uint32_t *references)
{
    if(!cache) return true;
    size_t n=lv_cache_get_size(cache,NULL);
    if(!add(entries,n) || !add(capacity,lv_cache_get_max_size(cache,NULL))) return false;
    if(!n) return true;
    void *copy=lv_malloc(lv_cache_entry_get_size(cache->node_size));
    lv_iter_t *it=copy?iterator(cache):NULL;
    if(!it) { lv_free(copy);return false; }
    size_t seen=0;bool ok=true;
    while(lv_iter_next(it,copy)==LV_RESULT_OK) {
        int32_t refs=lv_cache_entry_get_ref(lv_cache_entry_get_entry(copy,cache->node_size));
        if(refs<0 || !add(references,(size_t)refs)) { ok=false;break; }
        seen++;
    }
    lv_iter_destroy(it);lv_free(copy);return ok && seen==n;
}
static lv_result_t inspect(const char *path,lv_freetype_font_style_t style,
                          lv_freetype_font_render_mode_t mode,unsigned drop,
                          aic_lv_ft_cache_stats_t *output)
{
    lv_freetype_context_t *ctx=lv_freetype_get_context();
    if(!ctx || !ctx->cache_node_cache) return LV_RESULT_INVALID;
    lv_cache_t *cache=ctx->cache_node_cache;
    size_t n=lv_cache_get_size(cache,NULL);
    aic_lv_ft_cache_stats_t stats={0};
    if(!n) { if(output) *output=stats;return LV_RESULT_OK; }
    if(n>SIZE_MAX/sizeof(selected_t) || cache->node_size!=sizeof(lv_freetype_cache_node_t)) return LV_RESULT_INVALID;
    selected_t *selected=drop?lv_malloc_zeroed(n*sizeof(*selected)):NULL;
    void *copy=(!drop || selected)?lv_malloc(lv_cache_entry_get_size(cache->node_size)):NULL;
    lv_iter_t *it=copy?iterator(cache):NULL;
    if(!it) { lv_free(copy);lv_free(selected);return LV_RESULT_INVALID; }
    size_t used=0,seen=0;bool ok=true;
    while(lv_iter_next(it,copy)==LV_RESULT_OK) {
        lv_freetype_cache_node_t *node=copy;
        if(++seen>n) { ok=false;break; }
        if((path && (!node->pathname || strcmp(path,node->pathname))) ||
           (style!=LV_FREETYPE_FONT_STYLE_ANY && style!=node->style) ||
           (mode!=LV_FREETYPE_FONT_RENDER_MODE_ANY && mode!=node->render_mode)) continue;
        uint32_t gr=0,dr=0;
        int32_t refs=lv_cache_entry_get_ref(lv_cache_entry_get_entry(copy,cache->node_size));
        if(refs<0 || !add(&stats.fonts,1) || !add(&stats.font_references,(size_t)refs) ||
           !counts(node->glyph_cache,&stats.glyph_entries,&stats.glyph_capacity,&gr) ||
           !counts(node->draw_data_cache,&stats.draw_entries,&stats.draw_capacity,&dr) ||
           !add(&stats.glyph_references,gr) || !add(&stats.draw_references,dr) ||
           ((drop&AIC_LV_FT_CACHE_TYPE_GLYPH) && gr) ||
           ((drop&AIC_LV_FT_CACHE_TYPE_DRAW_DATA) && dr)) { ok=false;break; }
        if(drop) selected[used++].key=*node;
    }
    lv_iter_destroy(it);lv_free(copy);
    ok=ok && seen==n;
    /* Pin real nodes only after destroying the iterator: acquire moves LRU
     * links. No cache can be purged until all planning/allocation succeeds. */
    if(ok && drop) for(size_t i=0;i<used;i++) {
        selected[i].entry=lv_cache_acquire(cache,&selected[i].key,NULL);
        if(!selected[i].entry) { ok=false;break; }
    }
#if LV_FREETYPE_CACHE_FT_GLYPH_L1
    if(ok && (drop&AIC_LV_FT_CACHE_TYPE_GLYPH)) for(size_t i=0;i<used;i++) {
        lv_freetype_cache_node_t fresh={0};
        lv_freetype_glyph_l1_init(&fresh);
        selected[i].fresh_l1=fresh.glyph_l1;
        if(!fresh.glyph_l1) { ok=false;break; }
    }
#endif
    if(ok && drop) for(size_t i=0;i<used;i++) {
        lv_freetype_cache_node_t *node=lv_cache_entry_get_data(selected[i].entry);
        if((drop&AIC_LV_FT_CACHE_TYPE_GLYPH) && node->glyph_cache) {
            lv_cache_drop_all(node->glyph_cache,NULL);
#if LV_FREETYPE_CACHE_FT_GLYPH_L1
            /* Replace copied L1 metrics only after all replacement storage
             * exists; the default port configuration has L1 disabled. */
            lv_freetype_glyph_l1_deinit(node);
            node->glyph_l1=selected[i].fresh_l1;selected[i].fresh_l1=NULL;
#endif
        }
        if((drop&AIC_LV_FT_CACHE_TYPE_DRAW_DATA) && node->draw_data_cache)
            lv_cache_drop_all(node->draw_data_cache,NULL);
    }
    if(drop) for(size_t i=0;i<used;i++) {
        if(selected[i].entry) lv_cache_release(cache,selected[i].entry,NULL);
#if LV_FREETYPE_CACHE_FT_GLYPH_L1
        lv_free(selected[i].fresh_l1);
#endif
    }
    lv_free(selected);
    if(ok && output) *output=stats;
    return ok?LV_RESULT_OK:LV_RESULT_INVALID;
}
lv_result_t aic_lv_ft_cache_get_stats(const char *path,lv_freetype_font_style_t style,
    lv_freetype_font_render_mode_t mode,aic_lv_ft_cache_stats_t *stats)
{ return stats?inspect(path,style,mode,0,stats):LV_RESULT_INVALID; }
lv_result_t aic_lv_ft_cache_drop_specific(const char *path,lv_freetype_font_style_t style,
    lv_freetype_font_render_mode_t mode,aic_lv_ft_cache_type_t type)
{
    if(!type || ((unsigned)type&~(unsigned)AIC_LV_FT_CACHE_TYPE_ALL)) return LV_RESULT_INVALID;
    return inspect(path,style,mode,(unsigned)type,NULL);
}
lv_result_t aic_lv_ft_cache_drop_all(aic_lv_ft_cache_type_t type)
{ return aic_lv_ft_cache_drop_specific(NULL,LV_FREETYPE_FONT_STYLE_ANY,LV_FREETYPE_FONT_RENDER_MODE_ANY,type); }
lv_result_t aic_lv_ft_cache_print_stats(void)
{
    aic_lv_ft_cache_stats_t s;
    if(aic_lv_ft_cache_get_stats(NULL,LV_FREETYPE_FONT_STYLE_ANY,LV_FREETYPE_FONT_RENDER_MODE_ANY,&s)!=LV_RESULT_OK)
        return LV_RESULT_INVALID;
    printf("FT caches=%u font_refs=%u (entry counts, not bytes)\n",(unsigned)s.fonts,(unsigned)s.font_references);
    printf("FT glyph entries=%u capacity=%u refs=%u\n",(unsigned)s.glyph_entries,(unsigned)s.glyph_capacity,(unsigned)s.glyph_references);
    printf("FT draw entries=%u capacity=%u refs=%u\n",(unsigned)s.draw_entries,(unsigned)s.draw_capacity,(unsigned)s.draw_references);
    return LV_RESULT_OK;
}
#endif
