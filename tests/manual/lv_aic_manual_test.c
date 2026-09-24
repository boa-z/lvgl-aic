/**
 * @file lv_aic_manual_test.c
 * @brief Small product-independent smoke page for the LVGL 9.6 port.
 */

#include "lv_aic_manual_test.h"

#include <stdbool.h>

static lv_obj_t *lv_aic_manual_root;
static lv_obj_t *lv_aic_manual_status;

static void lv_aic_manual_button_event(lv_event_t *event)
{
    if (lv_aic_manual_status != NULL) {
        lv_label_set_text(lv_aic_manual_status, "button event received");
    }
    (void)event;
}

int lv_aic_manual_test_create(void)
{
    lv_display_t *display = lv_display_get_default();
    lv_obj_t *button;

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
    lv_obj_set_style_radius(lv_aic_manual_root, 0, 0);

    lv_aic_manual_status = lv_label_create(lv_aic_manual_root);
    lv_label_set_text(lv_aic_manual_status, "lvgl-aic LVGL 9.6 software baseline");
    lv_obj_set_pos(lv_aic_manual_status, 24, 24);

    button = lv_button_create(lv_aic_manual_root);
    lv_obj_set_size(button, 180, 56);
    lv_obj_set_pos(button, 24, 80);
    lv_obj_add_event_cb(button, lv_aic_manual_button_event, LV_EVENT_CLICKED, NULL);

    lv_obj_t *button_label = lv_label_create(button);
    lv_label_set_text(button_label, "platform smoke test");
    lv_obj_center(button_label);

    return LV_AIC_OK;
}

void lv_aic_manual_test_deinit(void)
{
    if (lv_aic_manual_root != NULL) {
        lv_obj_delete(lv_aic_manual_root);
        lv_aic_manual_root = NULL;
        lv_aic_manual_status = NULL;
    }
}
