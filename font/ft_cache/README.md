# FreeType cache controls

Enable `AIC_LVGL_USE_FREETYPE` and `AIC_LVGL_USE_FT_CACHE` to use the
SDK-compatible statistics and selective purge APIs in `aic_lv_ft_cache.h`.
The implementation uses pinned LVGL 9.6 FreeType/cache/iterator internals;
it does not copy vendor LRU/tree structs or patch SDK/LVGL sources.

Applications keep native `lv_freetype_font_create/delete` and `font->fallback`
ownership. Cache operations run between refreshes, with draw workers idle and
font/glyph operations serialized on the LVGL owner thread or its external lock.
Referenced entries reject a selected purge before any eviction; live font
objects survive successful purges and regenerate glyphs on next use.

Statistics are entry, capacity and reference counts, not bytes. The native
per-cache count bound is not a global font heap budget. L1 is disabled by the
port configuration; an application override enabling L1 is not validated here.
See [cache API and validation](../../docs/ft-cache-stage.md) and
[native font ownership](../../docs/font-stage.md).
