/* SPDX-License-Identifier: Apache-2.0 */
#include "aic_lv_ft_cache.h"
#include "src/font/freetype/lv_freetype_private.h"
#include "src/misc/cache/lv_cache_entry.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
static unsigned allocation,fail_at;
static void *guarded_alloc(size_t n);
static void *guarded_zero(size_t n);
static lv_iter_t *guarded_iter(lv_cache_t *c);
#define lv_malloc guarded_alloc
#define lv_malloc_zeroed guarded_zero
#define lv_cache_iter_create guarded_iter
#include "../../font/ft_cache/aic_lv_ft_cache.c"
#undef lv_malloc
#undef lv_malloc_zeroed
#undef lv_cache_iter_create
static void *guarded_alloc(size_t n) { return ++allocation==fail_at?NULL:lv_malloc(n); }
static void *guarded_zero(size_t n) { return ++allocation==fail_at?NULL:lv_malloc_zeroed(n); }
static lv_iter_t *guarded_iter(lv_cache_t *c) { return ++allocation==fail_at?NULL:lv_cache_iter_create(c); }
static aic_lv_ft_cache_stats_t stats(const char *p,lv_freetype_font_style_t style)
{
    aic_lv_ft_cache_stats_t s;
    assert(aic_lv_ft_cache_get_stats(p,style,LV_FREETYPE_FONT_RENDER_MODE_ANY,&s)==LV_RESULT_OK);
    return s;
}
static uint32_t hash(const lv_draw_buf_t *b)
{
    assert(b && b->data);uint32_t n=2166136261U;
    for(unsigned y=0;y<b->header.h;y++) for(unsigned x=0;x<b->header.w;x++)
        n=(n^b->data[y*b->header.stride+x])*16777619U;
    return n;
}
static uint32_t glyph(lv_font_t *f,unsigned code)
{
    lv_font_glyph_dsc_t d={0};assert(lv_font_get_glyph_dsc(f,&d,code,0) && !d.is_placeholder);
    const lv_draw_buf_t *b=lv_font_get_glyph_bitmap(&d,NULL);uint32_t h=hash(b);
    lv_font_glyph_release_draw_data(&d);return h;
}
int main(int argc,char **argv)
{
    assert(argc==3);unsigned failure_checks=0;
    for(unsigned cycle=0;cycle<3;cycle++) {
        lv_init();
        aic_lv_ft_cache_stats_t s=stats(NULL,LV_FREETYPE_FONT_STYLE_ANY);assert(s.fonts==0);
        assert(aic_lv_ft_cache_drop_all(AIC_LV_FT_CACHE_TYPE_ALL)==LV_RESULT_OK);
        assert(aic_lv_ft_cache_drop_all((aic_lv_ft_cache_type_t)0)==LV_RESULT_INVALID);
        assert(aic_lv_ft_cache_drop_all((aic_lv_ft_cache_type_t)4)==LV_RESULT_INVALID);
        assert(aic_lv_ft_cache_get_stats(NULL,0,0,NULL)==LV_RESULT_INVALID);
        lv_font_t *normal=lv_freetype_font_create(argv[1],LV_FREETYPE_FONT_RENDER_MODE_BITMAP,24,LV_FREETYPE_FONT_STYLE_NORMAL);
        lv_font_t *large=lv_freetype_font_create(argv[1],LV_FREETYPE_FONT_RENDER_MODE_BITMAP,38,LV_FREETYPE_FONT_STYLE_NORMAL);
        lv_font_t *bold=lv_freetype_font_create(argv[1],LV_FREETYPE_FONT_RENDER_MODE_BITMAP,24,LV_FREETYPE_FONT_STYLE_BOLD);
        lv_font_t *cjk=lv_freetype_font_create(argv[2],LV_FREETYPE_FONT_RENDER_MODE_BITMAP,24,LV_FREETYPE_FONT_STYLE_NORMAL);
        assert(normal && large && bold && cjk);
        uint32_t normal_hash=glyph(normal,'A'),cjk_hash=glyph(cjk,0x4e2d);
        glyph(large,'A');glyph(bold,'A');
        s=stats(NULL,LV_FREETYPE_FONT_STYLE_ANY);assert(s.fonts==3 && s.font_references==4);
        assert(s.glyph_entries>=4 && s.draw_entries>=4 && !s.draw_references);
        assert(stats(argv[1],LV_FREETYPE_FONT_STYLE_NORMAL).font_references==2);
        assert(stats("missing.ttf",LV_FREETYPE_FONT_STYLE_ANY).fonts==0);
        aic_lv_ft_cache_stats_t empty;
        assert(aic_lv_ft_cache_get_stats(NULL,LV_FREETYPE_FONT_STYLE_ANY,LV_FREETYPE_FONT_RENDER_MODE_OUTLINE,&empty)==LV_RESULT_OK && !empty.fonts);
        assert(aic_lv_ft_cache_drop_specific(argv[1],LV_FREETYPE_FONT_STYLE_BOLD,LV_FREETYPE_FONT_RENDER_MODE_BITMAP,AIC_LV_FT_CACHE_TYPE_DRAW_DATA)==LV_RESULT_OK);
        assert(stats(argv[1],LV_FREETYPE_FONT_STYLE_BOLD).draw_entries==0);
        assert(stats(argv[1],LV_FREETYPE_FONT_STYLE_BOLD).glyph_entries>0);
        assert(stats(argv[1],LV_FREETYPE_FONT_STYLE_NORMAL).draw_entries>0);
        assert(glyph(normal,'A')==normal_hash);glyph(bold,'A');
        /* Pinned bitmap: all-matches purge must leave every font untouched. */
        lv_font_glyph_dsc_t held={0};assert(lv_font_get_glyph_dsc(cjk,&held,0x4e2d,0));
        const lv_draw_buf_t *bitmap=lv_font_get_glyph_bitmap(&held,NULL);
        uint32_t held_hash=hash(bitmap);glyph(bold,'B');
        aic_lv_ft_cache_stats_t before=stats(NULL,LV_FREETYPE_FONT_STYLE_ANY);
        assert(before.draw_references==1);
        assert(aic_lv_ft_cache_drop_all(AIC_LV_FT_CACHE_TYPE_ALL)==LV_RESULT_INVALID);
        s=stats(NULL,LV_FREETYPE_FONT_STYLE_ANY);assert(!memcmp(&s,&before,sizeof s) && hash(bitmap)==held_hash);
        assert(aic_lv_ft_cache_drop_specific(argv[1],LV_FREETYPE_FONT_STYLE_ANY,LV_FREETYPE_FONT_RENDER_MODE_ANY,AIC_LV_FT_CACHE_TYPE_ALL)==LV_RESULT_OK);
        assert(hash(bitmap)==held_hash && stats(argv[2],LV_FREETYPE_FONT_STYLE_NORMAL).draw_references==1);
        /* Glyph metrics can be evicted while the separate bitmap is pinned. */
        assert(aic_lv_ft_cache_drop_specific(argv[2],LV_FREETYPE_FONT_STYLE_NORMAL,
            LV_FREETYPE_FONT_RENDER_MODE_BITMAP,AIC_LV_FT_CACHE_TYPE_GLYPH)==LV_RESULT_OK);
        s=stats(argv[2],LV_FREETYPE_FONT_STYLE_NORMAL);
        assert(!s.glyph_entries && s.draw_entries && s.draw_references==1 && hash(bitmap)==held_hash);
        lv_font_glyph_release_draw_data(&held);
        assert(glyph(normal,'A')==normal_hash && glyph(cjk,0x4e2d)==cjk_hash);
        glyph(large,'B');glyph(bold,'A');
        /* Fail each component snapshot/plan/iterator allocation. Nothing is
         * evicted until the complete cross-font preflight has succeeded. */
        before=stats(NULL,LV_FREETYPE_FONT_STYLE_ANY);allocation=0;
        assert(aic_lv_ft_cache_drop_all(AIC_LV_FT_CACHE_TYPE_ALL)==LV_RESULT_OK);
        unsigned total=allocation;
        glyph(normal,'A');glyph(large,'B');glyph(bold,'A');glyph(cjk,0x4e2d);
        before=stats(NULL,LV_FREETYPE_FONT_STYLE_ANY);
        for(unsigned f=1;f<=total;f++) {
            allocation=0;fail_at=f;
            assert(aic_lv_ft_cache_drop_all(AIC_LV_FT_CACHE_TYPE_ALL)==LV_RESULT_INVALID);
            fail_at=0;s=stats(NULL,LV_FREETYPE_FONT_STYLE_ANY);
            assert(!memcmp(&s,&before,sizeof s));failure_checks++;
        }
        aic_lv_ft_cache_stats_t untouched;memset(&untouched,0xa5,sizeof untouched);s=untouched;
        allocation=0;fail_at=1;
        assert(aic_lv_ft_cache_get_stats(NULL,LV_FREETYPE_FONT_STYLE_ANY,LV_FREETYPE_FONT_RENDER_MODE_ANY,&s)==LV_RESULT_INVALID);
        assert(!memcmp(&s,&untouched,sizeof s));fail_at=0;
        assert(aic_lv_ft_cache_drop_all(AIC_LV_FT_CACHE_TYPE_ALL)==LV_RESULT_OK);
        s=stats(NULL,LV_FREETYPE_FONT_STYLE_ANY);
        assert(s.fonts==3 && s.font_references==4 && !s.glyph_entries && !s.draw_entries);
        assert(glyph(normal,'A')==normal_hash && glyph(cjk,0x4e2d)==cjk_hash);
        assert(aic_lv_ft_cache_print_stats()==LV_RESULT_OK);
        lv_freetype_font_delete(large);lv_freetype_font_delete(normal);lv_freetype_font_delete(bold);lv_freetype_font_delete(cjk);
        lv_deinit();
        /* No native context: report failure without modifying caller output. */
        s=untouched;
        assert(aic_lv_ft_cache_get_stats(NULL,LV_FREETYPE_FONT_STYLE_ANY,
            LV_FREETYPE_FONT_RENDER_MODE_ANY,&s)==LV_RESULT_INVALID);
        assert(!memcmp(&s,&untouched,sizeof s));
        assert(aic_lv_ft_cache_drop_all(AIC_LV_FT_CACHE_TYPE_ALL)==LV_RESULT_INVALID);
    }
    printf("PASS FreeType cache filters, shared faces, held glyph atomic rejection, bitmap rebuild, %u allocation failures and three lifecycles\n",failure_checks);
    return 0;
}
