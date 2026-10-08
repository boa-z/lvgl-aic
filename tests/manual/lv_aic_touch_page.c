/* SPDX-License-Identifier: Apache-2.0 */
/* Touch test and calibration check.
 *
 * Five rings (four corners inset by MARGIN, plus the centre) are targets. The
 * person touches each one; the page averages the coordinates LVGL delivered
 * during that touch and shows the error against the ring centre. Once the four
 * corners are in, it fits delivered = scale * target + offset per axis. The
 * delivered coordinate is raw * panel / range, so the true controller range is
 * range * scale: that is the value to put in AIC_TOUCH_X/Y_COORDINATE_RANGE
 * when the scale is not about 1.000. A live crosshair shows where the pointer
 * currently is, and the header line the raw numbers. */
#include "lv_aic_touch_page.h"
#include "lvgl_aic.h"

#if AIC_LVGL_USE_TOUCH && AIC_LVGL_BSP_RTTHREAD
#include "../../port/lv_aic_indev.h"

#include <stdint.h>
#include <string.h>

#define MARGIN 48
#define RING 56
#define HIT_RADIUS 100
#define TARGETS 5

static const char *const names[TARGETS] = {"top-left", "top-right", "bottom-right",
                                            "bottom-left", "center"};

typedef struct {
    lv_obj_t *root, *header, *report, *fit, *ring[TARGETS], *cross_h, *cross_v;
    int32_t w, h;
    int32_t target_x[TARGETS], target_y[TARGETS];
    bool done[TARGETS];
    int32_t got_x[TARGETS], got_y[TARGETS];
    int64_t sum_x, sum_y;
    uint32_t count;
} touch_page_t;

static touch_page_t *page;

static int32_t mean(int64_t sum, uint32_t n)
{
    return n ? (int32_t)(sum / (int64_t)n) : 0;
}

static void refresh_report(void)
{
    char text[400];
    int used = 0;

    for (int i = 0; i < TARGETS; i++) {
        if (page->done[i]) {
            used += lv_snprintf(text + used, sizeof(text) - (size_t)used,
                                "%-12s got (%d,%d)  err (%+d,%+d)\n", names[i],
                                (int)page->got_x[i], (int)page->got_y[i],
                                (int)(page->got_x[i] - page->target_x[i]),
                                (int)(page->got_y[i] - page->target_y[i]));
        } else {
            used += lv_snprintf(text + used, sizeof(text) - (size_t)used, "%-12s --\n", names[i]);
        }
    }
    lv_label_set_text(page->report, text);
}

static void refresh_fit(void)
{
    for (int i = 0; i < 4; i++) {
        if (!page->done[i]) {
            lv_label_set_text(page->fit, "Touch the four corner rings to see the fit.");
            return;
        }
    }
    /* Corner order: TL, TR, BR, BL. Left/right and top/bottom pairs. */
    int32_t left = (page->got_x[0] + page->got_x[3]) / 2, right = (page->got_x[1] + page->got_x[2]) / 2;
    int32_t top = (page->got_y[0] + page->got_y[1]) / 2, bottom = (page->got_y[2] + page->got_y[3]) / 2;
    int32_t span_x = page->w - 2 * MARGIN, span_y = page->h - 2 * MARGIN;
    int32_t sx = (right - left) * 1000 / span_x, sy = (bottom - top) * 1000 / span_y;
    int32_t ox = left - sx * MARGIN / 1000, oy = top - sy * MARGIN / 1000;
    lv_aic_touch_diagnostics_t d = {0};
    char text[240];

    (void)lv_aic_indev_get_diagnostics(lv_aic_get_pointer_indev(), &d);
    lv_snprintf(text, sizeof(text),
                "scale x=%d.%03d y=%d.%03d  offset x=%+d y=%+d\n"
                "range now %dx%d  -> %s %dx%d",
                (int)(sx / 1000), (int)(sx % 1000), (int)(sy / 1000), (int)(sy % 1000), (int)ox,
                (int)oy, (int)d.range_x, (int)d.range_y,
                (sx > 970 && sx < 1030 && sy > 970 && sy < 1030) ? "OK, keep" : "suggest",
                (int)(d.range_x * sx / 1000), (int)(d.range_y * sy / 1000));
    lv_label_set_text(page->fit, text);
}

static void place_cross(int32_t x, int32_t y)
{
    lv_obj_set_pos(page->cross_h, 0, y);
    lv_obj_set_pos(page->cross_v, x, 0);
}

static void touch_event(lv_event_t *event)
{
    lv_event_code_t code = lv_event_get_code(event);
    lv_point_t point;

    if (page == NULL) {
        return;
    }
    lv_indev_get_point(lv_indev_active(), &point);
    if (code == LV_EVENT_PRESSED) {
        page->sum_x = page->sum_y = 0;
        page->count = 0;
    }
    if (code == LV_EVENT_PRESSED || code == LV_EVENT_PRESSING) {
        page->sum_x += point.x;
        page->sum_y += point.y;
        page->count++;
        place_cross(point.x, point.y);
        lv_label_set_text_fmt(page->header, "x=%d y=%d   (screen %dx%d)", (int)point.x,
                              (int)point.y, (int)page->w, (int)page->h);
    } else if (code == LV_EVENT_RELEASED && page->count) {
        int32_t mx = mean(page->sum_x, page->count), my = mean(page->sum_y, page->count);
        int best = -1;
        int64_t best_d2 = (int64_t)HIT_RADIUS * HIT_RADIUS;

        for (int i = 0; i < TARGETS; i++) {
            int64_t dx = mx - page->target_x[i], dy = my - page->target_y[i];
            if (dx * dx + dy * dy < best_d2) {
                best_d2 = dx * dx + dy * dy;
                best = i;
            }
        }
        if (best >= 0) {
            page->done[best] = true;
            page->got_x[best] = mx;
            page->got_y[best] = my;
            lv_obj_set_style_border_color(page->ring[best], lv_color_hex(0x40c060), 0);
            refresh_report();
            refresh_fit();
        }
        page->count = 0;
    }
}

static void reset_event(lv_event_t *event)
{
    LV_UNUSED(event);
    if (page == NULL) {
        return;
    }
    memset(page->done, 0, sizeof(page->done));
    for (int i = 0; i < TARGETS; i++) {
        lv_obj_set_style_border_color(page->ring[i], lv_color_hex(0xe0a030), 0);
    }
    refresh_report();
    refresh_fit();
}

static void close_page(bool async);

static void exit_event(lv_event_t *event)
{
    LV_UNUSED(event);
    close_page(true);
}

static lv_obj_t *make_button(lv_obj_t *parent, const char *text, int32_t x, int32_t y,
                             lv_event_cb_t cb)
{
    lv_obj_t *button = lv_button_create(parent);
    lv_obj_t *label = lv_label_create(button);

    lv_obj_set_size(button, 104, 44);
    lv_obj_set_pos(button, x, y);
    lv_obj_add_event_cb(button, cb, LV_EVENT_CLICKED, NULL);
    lv_label_set_text(label, text);
    lv_obj_center(label);
    return button;
}

static lv_obj_t *make_line(lv_obj_t *parent, int32_t w, int32_t h)
{
    lv_obj_t *line = lv_obj_create(parent);

    lv_obj_remove_style_all(line);
    lv_obj_set_size(line, w, h);
    lv_obj_set_style_bg_color(line, lv_color_hex(0x40a0ff), 0);
    lv_obj_set_style_bg_opa(line, LV_OPA_70, 0);
    lv_obj_set_clickable(line, false);
    return line;
}

bool lv_aic_touch_page_available(void)
{
    return true;
}

void lv_aic_touch_page_open(void)
{
    lv_display_t *display = lv_display_get_default();

    if (page != NULL || display == NULL) {
        return;
    }
    page = lv_malloc(sizeof(*page));
    if (page == NULL) {
        return;
    }
    memset(page, 0, sizeof(*page));
    page->w = lv_display_get_horizontal_resolution(display);
    page->h = lv_display_get_vertical_resolution(display);

    page->root = lv_obj_create(lv_layer_top());
    lv_obj_remove_style_all(page->root);
    lv_obj_set_size(page->root, page->w, page->h);
    lv_obj_set_style_bg_color(page->root, lv_color_hex(0x101820), 0);
    lv_obj_set_style_bg_opa(page->root, LV_OPA_COVER, 0);
    lv_obj_set_style_text_color(page->root, lv_color_white(), 0);
    lv_obj_set_scrollable(page->root, false);
    lv_obj_add_event_cb(page->root, touch_event, LV_EVENT_ALL, NULL);

    page->cross_h = make_line(page->root, page->w, 1);
    page->cross_v = make_line(page->root, 1, page->h);
    place_cross(page->w / 2, page->h / 2);

    page->target_x[0] = MARGIN;           page->target_y[0] = MARGIN;
    page->target_x[1] = page->w - MARGIN; page->target_y[1] = MARGIN;
    page->target_x[2] = page->w - MARGIN; page->target_y[2] = page->h - MARGIN;
    page->target_x[3] = MARGIN;           page->target_y[3] = page->h - MARGIN;
    page->target_x[4] = page->w / 2;      page->target_y[4] = page->h / 2;
    for (int i = 0; i < TARGETS; i++) {
        lv_obj_t *ring = lv_obj_create(page->root);

        lv_obj_remove_style_all(ring);
        lv_obj_set_size(ring, RING, RING);
        lv_obj_set_pos(ring, page->target_x[i] - RING / 2, page->target_y[i] - RING / 2);
        lv_obj_set_style_radius(ring, RING / 2, 0);
        lv_obj_set_style_border_width(ring, 4, 0);
        lv_obj_set_style_border_color(ring, lv_color_hex(0xe0a030), 0);
        lv_obj_set_clickable(ring, false);
        page->ring[i] = ring;
    }

    page->header = lv_label_create(page->root);
    lv_label_set_text(page->header, "Touch each ring (corner rings first)");
    lv_obj_set_pos(page->header, MARGIN + RING, 12);
    page->report = lv_label_create(page->root);
    lv_obj_set_pos(page->report, MARGIN + RING + 8, 60);
    page->fit = lv_label_create(page->root);
    lv_obj_set_style_text_color(page->fit, lv_color_hex(0xa9d0ff), 0);
    lv_obj_set_pos(page->fit, MARGIN + RING + 8, page->h - 150);
    refresh_report();
    refresh_fit();

    make_button(page->root, "Reset", page->w / 2 - 116, page->h - 70, reset_event);
    make_button(page->root, "Exit", page->w / 2 + 12, page->h - 70, exit_event);
}

static void close_page(bool async)
{
    if (page == NULL) {
        return;
    }
    /* Called from the Exit button's own event: defer the delete. Teardown
     * deletes at once, because the display goes away right after. */
    if (async) {
        lv_obj_delete_async(page->root);
    } else {
        lv_obj_delete(page->root);
    }
    lv_free(page);
    page = NULL;
}

void lv_aic_touch_page_close(void)
{
    close_page(false);
}

#else /* touch disabled */

bool lv_aic_touch_page_available(void) { return false; }
void lv_aic_touch_page_open(void) {}
void lv_aic_touch_page_close(void) {}

#endif
