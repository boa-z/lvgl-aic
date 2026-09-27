/**
 * @file lv_aic_manual_test.c
 * @brief Small product-independent smoke page for the LVGL 9.6 port.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "lv_aic_manual_test.h"

#include <stdbool.h>

static lv_obj_t *lv_aic_manual_root;
static lv_obj_t *lv_aic_manual_status;
static lv_obj_t *lv_aic_manual_marker;
static lv_timer_t *lv_aic_manual_timer;
static int32_t lv_aic_manual_marker_x;
#if AIC_LVGL_USE_MPP_DEC
static lv_obj_t *lv_aic_mpp_label;
static lv_obj_t *lv_aic_mpp_images[3];
#endif
#if AIC_LVGL_USE_GE2D
#define LV_AIC_GE2D_SMALL_COUNT 6
static lv_obj_t *lv_aic_ge2d_label;
static lv_obj_t *lv_aic_ge2d_large;
static lv_obj_t *lv_aic_ge2d_medium;
static lv_obj_t *lv_aic_ge2d_small[LV_AIC_GE2D_SMALL_COUNT];
static lv_obj_t *lv_aic_ge2d_round;
#endif

/* The Phase 3B image probes reuse the Phase 2 MPP fixtures, so they need the
 * draw unit AND the decoder. The LAYER probe needs only the draw unit. */
#if AIC_LVGL_USE_GE2D && AIC_LVGL_USE_MPP_DEC
#define LV_AIC_GE2D_IMAGE_TEST 1
#else
#define LV_AIC_GE2D_IMAGE_TEST 0
#endif

#if AIC_LVGL_USE_GE2D
static lv_obj_t *lv_aic_ge2d_b3_label;
static lv_obj_t *lv_aic_ge2d_layer;
#endif
#if LV_AIC_GE2D_IMAGE_TEST
static lv_obj_t *lv_aic_ge2d_img_plain;
static lv_obj_t *lv_aic_ge2d_img_swatch;
static lv_obj_t *lv_aic_ge2d_img_argb;
#endif

#if AIC_LVGL_USE_GE2D
/* Plain opaque rectangle. radius 0 and no gradient are exactly the Phase 3A
 * preconditions, so these shapes are what the GE2D unit is allowed to claim. */
static lv_obj_t *lv_aic_ge2d_make_rect(lv_obj_t *parent, int32_t x, int32_t y,
                                       int32_t w, int32_t h, uint32_t color,
                                       int32_t radius)
{
    lv_obj_t *rect = lv_obj_create(parent);

    if (rect == NULL) {
        return NULL;
    }
    lv_obj_set_size(rect, w, h);
    lv_obj_set_pos(rect, x, y);
    lv_obj_set_style_bg_color(rect, lv_color_hex(color), 0);
    lv_obj_set_style_bg_opa(rect, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(rect, 0, 0);
    lv_obj_set_style_pad_all(rect, 0, 0);
    lv_obj_set_style_radius(rect, radius, 0);
    return rect;
}
#endif

#if LV_AIC_GE2D_IMAGE_TEST
/* One of the Phase 2 MPP fixtures at its NATIVE size. The Phase 2 row scales
 * b.png and c.png by 4x, which makes them transformed copies; these are the
 * same files unscaled, so they are plain blits the GE2D unit can claim. */
static lv_obj_t *lv_aic_ge2d_make_image(lv_obj_t *parent, const char *path,
                                        int32_t x, int32_t y)
{
    lv_obj_t *image = lv_image_create(parent);

    if (image == NULL) {
        return NULL;
    }
    lv_image_set_src(image, path);
    lv_obj_set_pos(image, x, y);
    return image;
}
#endif

static void lv_aic_manual_button_event(lv_event_t *event)
{
    if (lv_aic_manual_status != NULL) {
        lv_label_set_text(lv_aic_manual_status, "button event received");
    }
    (void)event;
}

static void lv_aic_manual_timer_callback(lv_timer_t *timer)
{
    const int32_t marker_limit = 240;

    (void)timer;
    if (lv_aic_manual_marker == NULL) {
        return;
    }

    lv_aic_manual_marker_x += 4;
    if (lv_aic_manual_marker_x > marker_limit) {
        lv_aic_manual_marker_x = 0;
    }
    lv_obj_set_x(lv_aic_manual_marker, 24 + lv_aic_manual_marker_x);
}

int lv_aic_manual_test_create(void)
{
    lv_display_t *display = lv_display_get_default();
    lv_obj_t *button;
    lv_obj_t *button_label;

    if (display == NULL) {
        return LV_AIC_ERR_INVALID_STATE;
    }
    if (lv_aic_manual_root != NULL) {
        return LV_AIC_ERR_INVALID_STATE;
    }

    lv_aic_manual_root = lv_obj_create(lv_screen_active());
    if (lv_aic_manual_root == NULL) {
        return LV_AIC_ERR_NO_MEMORY;
    }
    lv_obj_set_size(lv_aic_manual_root,
                    lv_display_get_horizontal_resolution(display),
                    lv_display_get_vertical_resolution(display));
    lv_obj_center(lv_aic_manual_root);
    lv_obj_set_style_bg_color(lv_aic_manual_root, lv_color_hex(0x202020), 0);
    lv_obj_set_style_bg_opa(lv_aic_manual_root, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(lv_aic_manual_root, 0, 0);
    lv_obj_set_style_pad_all(lv_aic_manual_root, 0, 0);
    lv_obj_set_style_radius(lv_aic_manual_root, 0, 0);

    lv_aic_manual_status = lv_label_create(lv_aic_manual_root);
    if (lv_aic_manual_status == NULL) {
        goto fail;
    }
    lv_label_set_text(lv_aic_manual_status, "lvgl-aic LVGL 9.6 software baseline");
    lv_obj_set_style_text_color(lv_aic_manual_status, lv_color_hex(0xffffff), 0);
    lv_obj_set_pos(lv_aic_manual_status, 24, 24);

    button = lv_button_create(lv_aic_manual_root);
    if (button == NULL) {
        goto fail;
    }
    lv_obj_set_size(button, 180, 56);
    lv_obj_set_pos(button, 24, 80);
    lv_obj_add_event_cb(button, lv_aic_manual_button_event, LV_EVENT_CLICKED, NULL);

    button_label = lv_label_create(button);
    if (button_label == NULL) {
        goto fail;
    }
    lv_label_set_text(button_label, "platform smoke test");
    lv_obj_center(button_label);

    lv_aic_manual_marker = lv_obj_create(lv_aic_manual_root);
    if (lv_aic_manual_marker == NULL) {
        goto fail;
    }
    lv_obj_set_size(lv_aic_manual_marker, 32, 32);
    lv_obj_set_pos(lv_aic_manual_marker, 24, 160);
    lv_obj_set_style_bg_color(lv_aic_manual_marker, lv_color_hex(0x40c060), 0);
    lv_obj_set_style_bg_opa(lv_aic_manual_marker, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(lv_aic_manual_marker, 0, 0);
    lv_obj_set_style_radius(lv_aic_manual_marker, 16, 0);

    lv_aic_manual_timer = lv_timer_create(lv_aic_manual_timer_callback, 30, NULL);
    if (lv_aic_manual_timer == NULL) {
        goto fail;
    }

#if AIC_LVGL_USE_MPP_DEC
    /* Phase 2A MPP section: three FILE images, non-product test assets.
     * Missing files render as LVGL placeholders; no crash, no product UI. */
    lv_aic_mpp_label = lv_label_create(lv_aic_manual_root);
    if (lv_aic_mpp_label == NULL) {
        goto fail;
    }
    lv_label_set_text(lv_aic_mpp_label,
                      "mpp: a.jpg JPEG | b.png RGB | c.png RGBA on white");
    lv_obj_set_style_text_color(lv_aic_mpp_label, lv_color_hex(0xffffff), 0);
    lv_obj_set_pos(lv_aic_mpp_label, 24, 210);

    {
        static const char *const paths[3] = {
            "L:/data/mpp_test/a.jpg",
            "L:/data/mpp_test/b.png",
            "L:/data/mpp_test/c.png",
        };
        lv_obj_t *backdrop;

        for (int i = 0; i < 3; i++) {
            if (i == 2) {
                /* White swatch under the RGBA fixture. c.png carries a 0..255
                 * alpha ramp, so against white the transparent corner stays
                 * white and the opaque corner keeps its colour: alpha blending
                 * becomes provable instead of blending into the dark page. */
                backdrop = lv_obj_create(lv_aic_manual_root);
                if (backdrop == NULL) {
                    goto fail;
                }
                lv_obj_set_size(backdrop, 128, 128);
                lv_obj_set_pos(backdrop, 24 + i * 220, 240);
                lv_obj_set_style_bg_color(backdrop, lv_color_hex(0xffffff), 0);
                lv_obj_set_style_bg_opa(backdrop, LV_OPA_COVER, 0);
                lv_obj_set_style_border_width(backdrop, 0, 0);
                lv_obj_set_style_radius(backdrop, 0, 0);
                lv_obj_set_style_pad_all(backdrop, 0, 0);
            }
            lv_aic_mpp_images[i] = lv_image_create(lv_aic_manual_root);
            if (lv_aic_mpp_images[i] == NULL) {
                goto fail;
            }
            lv_image_set_src(lv_aic_mpp_images[i], paths[i]);
            lv_obj_set_pos(lv_aic_mpp_images[i], 24 + i * 220, 240);
            /* Keep the JPEG at 160x120; enlarge the 32x32 PNG fixtures to
             * 128x128 so channel order and alpha are visible on the 800x480
             * panel (pivot 0,0 grows the scaled image right/down). */
            if (i != 0) {
                lv_image_set_pivot(lv_aic_mpp_images[i], 0, 0);
                lv_image_set_scale(lv_aic_mpp_images[i], 1024);
            }
        }
    }
#endif

#if AIC_LVGL_USE_GE2D
    /* Phase 3A GE2D section. The left-hand shapes are opaque with square
     * corners, so the GE2D unit claims them; the right-hand rounded rectangle
     * is deliberately unclaimable and exercises the software fallback. */
    lv_aic_ge2d_label = lv_label_create(lv_aic_manual_root);
    if (lv_aic_ge2d_label == NULL) {
        goto fail;
    }
    lv_label_set_text(lv_aic_ge2d_label,
                      "ge2d: opaque squares accelerated | rounded -> software");
    lv_obj_set_style_text_color(lv_aic_ge2d_label, lv_color_hex(0xffffff), 0);
    lv_obj_set_pos(lv_aic_ge2d_label, 16, 372);

    lv_aic_ge2d_large = lv_aic_ge2d_make_rect(lv_aic_manual_root, 16, 400, 280, 56,
                                              0xc04040, 0);
    lv_aic_ge2d_medium = lv_aic_ge2d_make_rect(lv_aic_manual_root, 308, 400, 120, 56,
                                               0x4080c0, 0);
    if ((lv_aic_ge2d_large == NULL) || (lv_aic_ge2d_medium == NULL)) {
        goto fail;
    }

    for (int i = 0; i < LV_AIC_GE2D_SMALL_COUNT; i++) {
        lv_aic_ge2d_small[i] = lv_aic_ge2d_make_rect(lv_aic_manual_root,
                                                     444 + i * 34, 414, 28, 28,
                                                     0x50c080, 0);
        if (lv_aic_ge2d_small[i] == NULL) {
            goto fail;
        }
    }

    lv_aic_ge2d_round = lv_aic_ge2d_make_rect(lv_aic_manual_root, 664, 400, 88, 56,
                                              0xd0a040, 18);
    if (lv_aic_ge2d_round == NULL) {
        goto fail;
    }

    /* Phase 3B section. It occupies the free strip to the right of the Phase 2
     * MPP row and adds no object that could shift the Phase 2 or Phase 3A
     * baselines.
     *
     * The layer probe is the reason the section exists: opa_layered !=
     * LV_OPA_COVER makes LVGL render the rectangle into a child layer and
     * submit a LAYER task to composite it (calculate_layer_type() ->
     * LV_LAYER_TYPE_SIMPLE). The rectangle is fully covered and opaque
     * underneath, so that layer is RGB rather than ARGB and the composite is a
     * plain layer blit carrying a partial global opacity.
     *
     * That composite is CLAIMED by the GE2D unit but not drawn by the engine on
     * this board: LVGL allocates the layer buffer with lv_malloc, which is the
     * RT-Thread system heap at 0x30040000, below the 0x40000000 floor the D13x
     * GE can reach, so the blit declines and the task is composited in software.
     * The probe is still worth having - it exercises the claim path and proves
     * the layer is never dropped - and it becomes a real engine test the moment
     * layer buffers are placed where the engine can see them. The 50% blend also
     * makes a wrong result visible at the panel rather than only in a counter. */
    lv_aic_ge2d_b3_label = lv_label_create(lv_aic_manual_root);
    if (lv_aic_ge2d_b3_label == NULL) {
        goto fail;
    }
    lv_label_set_text(lv_aic_ge2d_b3_label,
                      LV_AIC_GE2D_IMAGE_TEST ? "3b: rgb|argb|layer" : "3b: layer");
    lv_obj_set_style_text_color(lv_aic_ge2d_b3_label, lv_color_hex(0xffffff), 0);
    lv_obj_set_pos(lv_aic_ge2d_b3_label, 600, 240);

#if LV_AIC_GE2D_IMAGE_TEST
    /* The same two fixtures the Phase 2 row scales, at their native 32x32. A
     * scaled image is a transformed copy and stays with the software renderer;
     * unscaled they are plain blits, and the decoder's output format picks the
     * path: b.png is RGB -> RGB888, a straight copy, and c.png is RGBA ->
     * ARGB8888, a blend with its own per-pixel alpha. The white swatch under
     * c.png is what makes the alpha ramp provable - against the dark page a
     * missing blend would be far harder to see. */
    lv_aic_ge2d_img_plain = lv_aic_ge2d_make_image(lv_aic_manual_root,
                                                   "L:/data/mpp_test/b.png",
                                                   600, 268);
    if (lv_aic_ge2d_img_plain == NULL) {
        goto fail;
    }

    lv_aic_ge2d_img_swatch = lv_aic_ge2d_make_rect(lv_aic_manual_root, 640, 268,
                                                   32, 32, 0xffffff, 0);
    if (lv_aic_ge2d_img_swatch == NULL) {
        goto fail;
    }

    lv_aic_ge2d_img_argb = lv_aic_ge2d_make_image(lv_aic_manual_root,
                                                  "L:/data/mpp_test/c.png",
                                                  640, 268);
    if (lv_aic_ge2d_img_argb == NULL) {
        goto fail;
    }
#endif /* LV_AIC_GE2D_IMAGE_TEST */

    lv_aic_ge2d_layer = lv_aic_ge2d_make_rect(lv_aic_manual_root, 600, 312, 88, 40,
                                              0x40a0e0, 0);
    if (lv_aic_ge2d_layer == NULL) {
        goto fail;
    }
    lv_obj_set_style_opa_layered(lv_aic_ge2d_layer, LV_OPA_50, 0);
#endif /* AIC_LVGL_USE_GE2D */

    return LV_AIC_OK;

fail:
    lv_aic_manual_test_deinit();
    return LV_AIC_ERR_NO_MEMORY;
}

const char *lv_aic_manual_test_status_text(void)
{
    if (lv_aic_manual_status == NULL) {
        return NULL;
    }
    return lv_label_get_text(lv_aic_manual_status);
}

void lv_aic_manual_test_deinit(void)
{
    if (lv_aic_manual_timer != NULL) {
        lv_timer_delete(lv_aic_manual_timer);
        lv_aic_manual_timer = NULL;
    }
    if (lv_aic_manual_root != NULL) {
        lv_obj_delete(lv_aic_manual_root);
        lv_aic_manual_root = NULL;
        lv_aic_manual_status = NULL;
        lv_aic_manual_marker = NULL;
#if AIC_LVGL_USE_MPP_DEC
        lv_aic_mpp_label = NULL;
        lv_aic_mpp_images[0] = NULL;
        lv_aic_mpp_images[1] = NULL;
        lv_aic_mpp_images[2] = NULL;
#endif
#if AIC_LVGL_USE_GE2D
        lv_aic_ge2d_label = NULL;
        lv_aic_ge2d_large = NULL;
        lv_aic_ge2d_medium = NULL;
        lv_aic_ge2d_round = NULL;
        lv_aic_ge2d_b3_label = NULL;
        lv_aic_ge2d_layer = NULL;
        for (int i = 0; i < LV_AIC_GE2D_SMALL_COUNT; i++) {
            lv_aic_ge2d_small[i] = NULL;
        }
#endif
#if LV_AIC_GE2D_IMAGE_TEST
        lv_aic_ge2d_img_plain = NULL;
        lv_aic_ge2d_img_swatch = NULL;
        lv_aic_ge2d_img_argb = NULL;
#endif
    }
    lv_aic_manual_marker_x = 0;
}
