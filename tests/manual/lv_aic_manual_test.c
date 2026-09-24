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
#if defined(AIC_LVGL_USE_MPP_DEC)
static lv_obj_t *lv_aic_mpp_label;
static lv_obj_t *lv_aic_mpp_images[3];
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

#if defined(AIC_LVGL_USE_MPP_DEC)
    /* Phase 2A MPP section: three FILE images, non-product test assets.
     * Missing files render as LVGL placeholders; no crash, no product UI. */
    lv_aic_mpp_label = lv_label_create(lv_aic_manual_root);
    if (lv_aic_mpp_label == NULL) {
        goto fail;
    }
    lv_label_set_text(lv_aic_mpp_label, "mpp: L:/data/mpp_test/{a.jpg,b.png,c.png}");
    lv_obj_set_style_text_color(lv_aic_mpp_label, lv_color_hex(0xffffff), 0);
    lv_obj_set_pos(lv_aic_mpp_label, 24, 210);

    {
        static const char *const paths[3] = {
            "L:/data/mpp_test/a.jpg",
            "L:/data/mpp_test/b.png",
            "L:/data/mpp_test/c.png",
        };
        for (int i = 0; i < 3; i++) {
            lv_aic_mpp_images[i] = lv_image_create(lv_aic_manual_root);
            if (lv_aic_mpp_images[i] == NULL) {
                goto fail;
            }
            lv_image_set_src(lv_aic_mpp_images[i], paths[i]);
            lv_obj_set_pos(lv_aic_mpp_images[i], 24 + i * 220, 240);
            lv_obj_set_size(lv_aic_mpp_images[i], 200, 150);
        }
    }
#endif

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
#if defined(AIC_LVGL_USE_MPP_DEC)
        lv_aic_mpp_label = NULL;
        lv_aic_mpp_images[0] = NULL;
        lv_aic_mpp_images[1] = NULL;
        lv_aic_mpp_images[2] = NULL;
#endif
    }
    lv_aic_manual_marker_x = 0;
}
