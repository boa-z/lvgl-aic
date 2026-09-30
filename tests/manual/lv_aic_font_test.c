/* SPDX-License-Identifier: Apache-2.0
 * Native LVGL font APIs only: the application owns fonts and their users. */
#include "lv_aic_font_test.h"
#if LV_USE_FREETYPE
#if AIC_LVGL_BSP_RTTHREAD
#define LOG_TAG "lvgl.font.test"
#define LOG_LVL LOG_LVL_INFO
#include "lv_aic_test_log.h"
#else
#include <stdio.h>
#define AIC_TEST_I(...) do { printf(__VA_ARGS__); puts(""); } while (0)
#define AIC_TEST_E(...) AIC_TEST_I(__VA_ARGS__)
#endif

static lv_font_t *page_fonts[4];
static lv_obj_t *font_panel, *font_button, *font_overlay;

/* The native FreeType bitmap backend returns an LVGL A8 draw buffer. Always
 * release the acquired glyph before deleting its font or requesting eviction. */
static int glyph_hash(lv_font_t *font, uint32_t code, uint32_t *hash)
{
    lv_font_glyph_dsc_t dsc = {0};
    if (!lv_font_get_glyph_dsc(font, &dsc, code, 0) || dsc.is_placeholder ||
        !dsc.box_w || !dsc.box_h) return -1;
    const lv_draw_buf_t *buf = lv_font_get_glyph_bitmap(&dsc, NULL);
    int result = -1;
    if (buf && buf->data && buf->header.cf == LV_COLOR_FORMAT_A8 &&
        buf->header.stride >= buf->header.w) {
        uint32_t h = 2166136261U;
        unsigned ink = 0;
        for (uint32_t y = 0; y < buf->header.h; y++) {
            for (uint32_t x = 0; x < buf->header.w; x++) {
                uint8_t v = buf->data[y * buf->header.stride + x];
                h = (h ^ v) * 16777619U;
                ink |= v;
            }
        }
        if (ink) { *hash = h; result = 0; }
    }
    lv_font_glyph_release_draw_data(&dsc);
    return result;
}

int lv_aic_font_probe(const char *latin, const char *cjk)
{
    const uint32_t sizes[] = {18, 28, 42};
    const lv_freetype_font_style_t styles[] = {
        LV_FREETYPE_FONT_STYLE_NORMAL, LV_FREETYPE_FONT_STYLE_BOLD,
        LV_FREETYPE_FONT_STYLE_ITALIC
    };
    for (unsigned i = 0; i < 3; i++) {
        lv_font_t *a = lv_freetype_font_create(latin,
            LV_FREETYPE_FONT_RENDER_MODE_BITMAP, sizes[i], styles[i]);
        lv_font_t *b = lv_freetype_font_create(cjk,
            LV_FREETYPE_FONT_RENDER_MODE_BITMAP, sizes[i], LV_FREETYPE_FONT_STYLE_NORMAL);
        uint32_t first = 0, again = 0, chinese = 0;
        int result = -1;
        if (!a || !b) goto release;
        a->fallback = b;
        lv_font_glyph_dsc_t dsc = {0};
        if (!lv_font_get_glyph_dsc(a, &dsc, 0x4e2d, 0) || dsc.resolved_font != b) goto release;
        if (glyph_hash(a, 'A', &first) || glyph_hash(a, 0x4e2d, &chinese)) goto release;
        for (unsigned cycle = 0; cycle < 3; cycle++) {
            for (uint32_t code = 33; code < 127; code++) {
                if (glyph_hash(a, code, &again)) goto release;
            }
        }
        if (glyph_hash(a, 'A', &again) || first != again) goto release;
        result = 0;
release:
        if (a) a->fallback = NULL;
        lv_freetype_font_delete(a);
        lv_freetype_font_delete(b);
        if (result) {
            AIC_TEST_E("FAIL native font size=%u", (unsigned)sizes[i]);
            return -1;
        }
        AIC_TEST_I("PASS font size=%u style=%u latin=%08x cjk=%08x",
            (unsigned)sizes[i], (unsigned)styles[i], (unsigned)first, (unsigned)chinese);
    }
    AIC_TEST_I("PASS native fonts: metrics, bitmap, fallback and cache churn");
    return 0;
}

static void show_panel(lv_event_t *event)
{
    (void)event;
    lv_obj_set_hidden(font_overlay, false);
}
static void hide_panel(lv_event_t *event)
{
    (void)event;
    lv_obj_set_hidden(font_overlay, true);
}

void lv_aic_font_test_delete(void)
{
    /* Remove every object referencing a font before destroying that font. */
    if (font_overlay) lv_obj_delete(font_overlay);
    if (font_button) lv_obj_delete(font_button);
    font_panel = font_button = font_overlay = NULL;
    for (unsigned i = 0; i < 4; i++) {
        lv_freetype_font_delete(page_fonts[i]);
        page_fonts[i] = NULL;
    }
}

int lv_aic_font_test_create(const char *latin, const char *cjk)
{
    if (font_overlay || font_button) return LV_AIC_ERR_INVALID_STATE;
    lv_display_t *display = lv_display_get_default();
    if (!display) return LV_AIC_ERR_INVALID_STATE;
    if (lv_aic_font_probe(latin, cjk)) return LV_AIC_ERR_UNSUPPORTED;
    const uint32_t sizes[] = {18, 28, 42, 28};
    for (unsigned i = 0; i < 4; i++) {
        page_fonts[i] = lv_freetype_font_create(i == 3 ? cjk : latin,
            LV_FREETYPE_FONT_RENDER_MODE_BITMAP, sizes[i], LV_FREETYPE_FONT_STYLE_NORMAL);
        if (!page_fonts[i]) goto fail;
    }
    font_button = lv_button_create(lv_layer_top());
    if (!font_button) goto fail;
    lv_obj_set_pos(font_button, lv_display_get_horizontal_resolution(display) - 104, 10);
    lv_obj_set_size(font_button, 88, 44);
    lv_obj_set_style_radius(font_button, 8, 0);
    lv_obj_set_style_shadow_width(font_button, 0, 0);
    lv_obj_add_event_cb(font_button, show_panel, LV_EVENT_CLICKED, NULL);
    lv_obj_t *text = lv_label_create(font_button);
    if (!text) goto fail;
    lv_label_set_text(text, "Fonts"); lv_obj_center(text);
    /* Cover the entire input surface so touches cannot activate the toolbar or
     * the test button behind the dialog. Only our objects are removed on close. */
    font_overlay = lv_obj_create(lv_layer_top());
    if (!font_overlay) goto fail;
    lv_obj_remove_style_all(font_overlay);
    lv_obj_set_size(font_overlay, lv_display_get_horizontal_resolution(display),
                    lv_display_get_vertical_resolution(display));
    lv_obj_set_style_bg_color(font_overlay, lv_color_black(), 0);
    lv_obj_set_style_bg_opa(font_overlay, LV_OPA_70, 0);
    lv_obj_set_scrollable(font_overlay, false);
    lv_obj_set_clickable(font_overlay, true);
    font_panel = lv_obj_create(font_overlay);
    if (!font_panel) goto fail;
    lv_obj_set_size(font_panel, 760, 424); lv_obj_center(font_panel);
    lv_obj_set_style_pad_all(font_panel, 20, 0);
    lv_obj_set_style_border_width(font_panel, 1, 0);
    lv_obj_set_style_border_color(font_panel, lv_color_hex(0x40546c), 0);
    lv_obj_set_style_radius(font_panel, 12, 0);
    lv_obj_set_style_bg_opa(font_panel, LV_OPA_COVER, 0);
    lv_obj_set_style_bg_color(font_panel, lv_color_hex(0x182430), 0);
    lv_obj_set_scrollable(font_panel, false);
    const char *lines[] = {
        "Native FreeType 18 px: AaZz 0123456789",
        "Dynamic font 28 px: AaZz 0123456789",
        "42 px: AaZz 0123456789",
        "中文仪表 字体测试 0123456789"
    };
    for (unsigned i = 0; i < 4; i++) {
        text = lv_label_create(font_panel);
        if (!text) goto fail;
        lv_obj_set_style_text_font(text, page_fonts[i], 0);
        lv_obj_set_style_text_color(text, lv_color_white(), 0);
        lv_obj_set_pos(text, 12, 45 + i * 65);
        lv_label_set_text(text, lines[i]);
    }
    text = lv_label_create(font_panel);
    if (!text) goto fail;
    lv_label_set_text(text, "Native FreeType | Latin + Chinese");
    lv_obj_set_style_text_color(text, lv_color_hex(0xa9bfd5), 0);
    lv_obj_set_pos(text, 12, 10);
    lv_obj_t *close = lv_button_create(font_panel);
    if (!close) goto fail;
    lv_obj_set_size(close, 96, 44);
    lv_obj_set_style_shadow_width(close, 0, 0);
    lv_obj_align(close, LV_ALIGN_TOP_RIGHT, 0, 0);
    lv_obj_add_event_cb(close, hide_panel, LV_EVENT_CLICKED, NULL);
    text = lv_label_create(close);
    if (!text) goto fail;
    lv_label_set_text(text, "Close");
    lv_obj_center(text);
    lv_obj_set_hidden(font_overlay, true);
    return LV_AIC_OK;
fail:
    lv_aic_font_test_delete();
    return LV_AIC_ERR_NO_MEMORY;
}
#endif
