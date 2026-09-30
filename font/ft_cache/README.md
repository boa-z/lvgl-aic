# Native FreeType and the reserved vendor cache

Native LVGL 9.6 FreeType bitmap fonts are now available through
AIC_LVGL_USE_FREETYPE (default off), selecting the independent SDK FreeType
library. There is no legacy lvgl-ui font adapter or SDK source patch.
AIC_LVGL_FREETYPE_GLYPHS defaults to 64 cached glyphs per native cache.
Applications use lv_freetype_font_create/delete and font->fallback directly.

AIC_LVGL_USE_FT_CACHE and this directory still reserve the old vendor-cache
integration; enabling that placeholder does not implement it. The native cache
is count-bounded, not a global byte-bounded substitute for the vendor cache.
See [native font integration and validation](../../docs/font-stage.md).
