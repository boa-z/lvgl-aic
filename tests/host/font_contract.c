/* SPDX-License-Identifier: Apache-2.0 */
#include "lv_aic_font_test.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

static unsigned flushes;
static unsigned long ink;
static unsigned char pixels[800 * 480 * 2];
static void flush(lv_display_t *display, const lv_area_t *area, uint8_t *data)
{
    size_t bytes = (size_t)lv_area_get_width(area) * lv_area_get_height(area) * 2;
    for (size_t i = 0; i < bytes; i++) ink += data[i] != 0;
    flushes++;
    lv_display_flush_ready(display);
}
static lv_obj_t *button(lv_obj_t *root, const char *name)
{
    for (uint32_t i = 0; i < lv_obj_get_child_count(root); i++) {
        lv_obj_t *obj = lv_obj_get_child(root, i);
        if (!lv_obj_check_type(obj, &lv_button_class)) continue;
        lv_obj_t *label = lv_obj_get_child(obj, 0);
        if (label && !strcmp(lv_label_get_text(label), name)) return obj;
    }
    return NULL;
}
int main(int argc, char **argv)
{
    assert(argc == 4);
    FILE *invalid = fopen(argv[3], "rb");
    assert(invalid); /* Exercise a corrupt font, not another missing path. */
    fclose(invalid);
    for (unsigned cycle = 0; cycle < 3; cycle++) {
        lv_init();
        assert(!lv_freetype_font_create("missing-font-file.ttf",
            LV_FREETYPE_FONT_RENDER_MODE_BITMAP, 28, LV_FREETYPE_FONT_STYLE_NORMAL));
        assert(!lv_freetype_font_create(argv[3],
            LV_FREETYPE_FONT_RENDER_MODE_BITMAP, 28, LV_FREETYPE_FONT_STYLE_NORMAL));
        lv_display_t *display = lv_display_create(800, 480);
        assert(display);
        lv_display_set_color_format(display, LV_COLOR_FORMAT_RGB565);
        lv_display_set_buffers(display, pixels, NULL, sizeof(pixels), LV_DISPLAY_RENDER_MODE_FULL);
        lv_display_set_flush_cb(display, flush);
        lv_obj_t *top = lv_layer_top();
        uint32_t before = lv_obj_get_child_count(top);
        assert(lv_aic_font_test_create(argv[1], "missing-font-file.ttf") == LV_AIC_ERR_UNSUPPORTED);
        assert(lv_obj_get_child_count(top) == before);
        assert(lv_aic_font_test_create(argv[1], argv[2]) == LV_AIC_OK);
        assert(lv_aic_font_test_create(argv[1], argv[2]) == LV_AIC_ERR_INVALID_STATE);
        assert(lv_obj_get_child_count(top) == before + 2);
        lv_obj_t *open = button(top, "Fonts");
        lv_obj_t *overlay = lv_obj_get_child(top, before + 1);
        lv_obj_t *panel = lv_obj_get_child(overlay, 0);
        lv_obj_t *close = button(panel, "Close");
        assert(open && close && lv_obj_is_hidden(overlay));
        assert(lv_obj_send_event(open, LV_EVENT_CLICKED, NULL) == LV_RESULT_OK);
        assert(!lv_obj_is_hidden(overlay));
        flushes = 0; ink = 0;
        lv_refr_now(display);
        assert(flushes && ink);
        /* Check actual white glyph pixels in each of the four label bounds,
         * not just the nonzero panel background. RGB565 white is 0xffff. */
        for (unsigned row = 0; row < 4; row++) {
            lv_area_t bounds;
            lv_obj_get_coords(lv_obj_get_child(panel, row), &bounds);
            unsigned white = 0;
            assert(bounds.x1 >= 0 && bounds.y1 >= 0 && bounds.x2 < 800 && bounds.y2 < 480);
            for (int y = bounds.y1; y <= bounds.y2; y++)
                for (int x = bounds.x1; x <= bounds.x2; x++) {
                    size_t offset = ((size_t)y * 800 + x) * 2;
                    white += pixels[offset] == 255 && pixels[offset + 1] == 255;
                }
            assert(white > 10);
        }
        lv_obj_send_event(close, LV_EVENT_CLICKED, NULL);
        assert(lv_obj_is_hidden(overlay));
        lv_aic_font_test_delete();
        lv_aic_font_test_delete();
        assert(lv_obj_get_child_count(top) == before);
        lv_display_delete(display);
        lv_deinit();
    }
    puts("PASS native FreeType: invalid files, rendering, fallback, cache and three full lifecycles");
    return 0;
}
