/**
 * @file lv_aic_manual_test.c
 * @brief Small product-independent smoke page for the LVGL 9.6 port.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "lv_aic_manual_test.h"

#include <stdbool.h>
#include "lv_aic_font_test.h"
#include "lv_aic_gif_test.h"
#include "lv_aic_apng_test.h"
#include "lv_aic_can_capture.h"
#include "lv_aic_demo_test.h"
#include "lv_aic_plane_test.h"
#include "lv_img_roller.h"
#include "lv_swipe_v1.h"
#include "lv_aic_canvas.h"

#if AIC_LVGL_USE_IMG_ROLLER && AIC_LVGL_USE_SWIPE_V1
#define LV_AIC_WIDGET_TEST 1
static lv_obj_t *lv_aic_widget_root;
#else
#define LV_AIC_WIDGET_TEST 0
#endif

#if LV_AIC_WIDGET_TEST && LV_USE_LIST && LV_USE_MENU
#define LV_AIC_NAV_TEST 1
static lv_obj_t *lv_aic_legacy_root;
#else
#define LV_AIC_NAV_TEST 0
#endif

#if LV_AIC_NATIVE_WIDGET_TEST
static lv_obj_t *native_widgets_root;
#endif
static lv_obj_t *lv_aic_manual_root;
static lv_obj_t *lv_aic_manual_status;
static lv_obj_t *lv_aic_manual_marker;
static lv_timer_t *lv_aic_manual_timer;
static int32_t lv_aic_manual_marker_x;
static volatile int lv_aic_manual_page_pending = -1;
static int lv_aic_manual_page_active;
static lv_obj_t *lv_aic_nav_root;
static lv_obj_t *lv_aic_nav_title;
static lv_obj_t *lv_aic_nav_position;
static lv_obj_t *lv_aic_nav_prev;
static lv_obj_t *lv_aic_nav_next;
#if AIC_LVGL_USE_GE2D && AIC_LVGL_USE_MPP_DEC
static lv_obj_t *lv_aic_rotation_root;
static lv_obj_t *lv_aic_combo_root;
#endif
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
#define LV_AIC_GE2D_C1_COUNT 6
static lv_obj_t *lv_aic_ge2d_img_plain;
static lv_obj_t *lv_aic_ge2d_img_swatch;
static lv_obj_t *lv_aic_ge2d_img_argb;
/* Phase 3C1: the same two fixtures, three global opacities each. */
static lv_obj_t *lv_aic_ge2d_c1_label;
static lv_obj_t *lv_aic_ge2d_c1_backdrop[2];
static lv_obj_t *lv_aic_ge2d_c1_img[LV_AIC_GE2D_C1_COUNT];
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
    lv_aic_manual_page_poll();
#if LV_AIC_PLANE_TEST_ENABLED
    lv_aic_plane_test_poll();
#endif
#if defined(AIC_LVGL_USE_APNG_WIDGET) && AIC_LVGL_USE_APNG_WIDGET
    lv_aic_apng_test_poll();
#endif
#if defined(AIC_LVGL_OFFICIAL_DEMOS) && AIC_LVGL_OFFICIAL_DEMOS
    lv_aic_demo_test_poll();
#endif
#if defined(AIC_LVGL_USE_CAN_CAPTURE) && AIC_LVGL_USE_CAN_CAPTURE
    lv_aic_can_capture_poll();
#endif
#if LV_USE_GIF && AIC_LVGL_BSP_RTTHREAD
    lv_aic_gif_test_poll();
#endif
#if AIC_LVGL_BSP_RTTHREAD && AIC_LVGL_BSP_MPP
    lv_aic_capture_poll();
#endif
    if (lv_aic_manual_marker == NULL || lv_aic_manual_page_active != 0) {
        return;
    }

    lv_aic_manual_marker_x += 4;
    if (lv_aic_manual_marker_x > marker_limit) {
        lv_aic_manual_marker_x = 0;
    }
    lv_obj_set_x(lv_aic_manual_marker, 24 + lv_aic_manual_marker_x);
}

#if AIC_LVGL_USE_CANVAS
static lv_obj_t *canvas_root, *canvas_image, *canvas_status;
static unsigned canvas_generation;
static bool canvas_alternate;
static void canvas_text(void)
{
    lv_draw_label_dsc_t label;
    lv_draw_label_dsc_init(&label); label.color=lv_color_white();
    lv_aic_canvas_draw_text_to_center(canvas_image,&label,
        canvas_alternate ? "Short" : "Transparent canvas text");
    lv_label_set_text_fmt(canvas_status,"Buffer %u / %s text",canvas_generation,
                         canvas_alternate ? "short" : "long");
}
static void canvas_button_event(lv_event_t *e)
{
    if(lv_event_get_user_data(e)) {
        lv_draw_buf_t *buffer=lv_canvas_get_draw_buf(canvas_image);
        if(lv_aic_canvas_alloc_buffer(canvas_image,buffer->header.w,buffer->header.h)!=LV_RESULT_OK) {
            lv_label_set_text(canvas_status,"Buffer allocation failed; old canvas retained");
            return;
        }
        canvas_generation++;
    }
    else canvas_alternate=!canvas_alternate;
    canvas_text();
}
static int canvas_page_create(lv_display_t *display)
{
    int32_t width=lv_display_get_horizontal_resolution(display);
    int32_t height=lv_display_get_vertical_resolution(display)-64;
    int32_t cw=LV_MIN(width-48,480), ch=LV_MIN(height-180,160);
    if(cw<32 || ch<32) return LV_AIC_ERR_UNSUPPORTED;
    canvas_root=lv_obj_create(lv_screen_active());
    if(!canvas_root) return LV_AIC_ERR_NO_MEMORY;
    lv_obj_set_pos(canvas_root,0,64);lv_obj_set_size(canvas_root,width,height);
    lv_obj_set_style_pad_all(canvas_root,0,0);lv_obj_set_scrollable(canvas_root,false);
    lv_obj_t *hint=lv_label_create(canvas_root);
    lv_obj_t *backdrop=lv_obj_create(canvas_root);
    if(!hint || !backdrop) return LV_AIC_ERR_NO_MEMORY;
    lv_label_set_text(hint,"Replace text: no old glyphs. Rebuild: same image. Background stays visible.");
    lv_obj_set_width(hint,width-32);lv_obj_set_pos(hint,16,16);
    lv_obj_set_size(backdrop,cw,ch);lv_obj_set_pos(backdrop,(width-cw)/2,72);
    lv_obj_set_style_pad_all(backdrop,0,0);lv_obj_set_style_border_width(backdrop,0,0);
    lv_obj_set_style_radius(backdrop,0,0);lv_obj_set_scrollable(backdrop,false);
    lv_obj_set_style_bg_color(backdrop,lv_color_hex(0x245b80),0);
    lv_obj_set_style_bg_grad_color(backdrop,lv_color_hex(0x803c50),0);
    lv_obj_set_style_bg_grad_dir(backdrop,LV_GRAD_DIR_HOR,0);
    canvas_image=lv_aic_canvas_create(backdrop);
    if(!canvas_image || !lv_aic_canvas_set_budget(canvas_image,1024U*1024U) ||
       lv_aic_canvas_alloc_buffer(canvas_image,cw,ch)!=LV_RESULT_OK) return LV_AIC_ERR_NO_MEMORY;
    lv_obj_set_pos(canvas_image,0,0);
    for(unsigned i=0;i<2;i++) {
        lv_obj_t *button=lv_button_create(canvas_root);
        if(!button) return LV_AIC_ERR_NO_MEMORY;
        lv_obj_set_size(button,160,44);lv_obj_set_pos(button,width/2-172+(int32_t)i*184,height-108);
        lv_obj_add_event_cb(button,canvas_button_event,LV_EVENT_CLICKED,(void *)(uintptr_t)i);
        lv_obj_t *label=lv_label_create(button);if(!label) return LV_AIC_ERR_NO_MEMORY;
        lv_label_set_text(label,i?"Rebuild buffer":"Replace text");lv_obj_center(label);
    }
    canvas_status=lv_label_create(canvas_root);if(!canvas_status) return LV_AIC_ERR_NO_MEMORY;
    lv_obj_set_pos(canvas_status,16,height-44);
    canvas_generation=1;canvas_alternate=false;canvas_text();
    lv_obj_set_hidden(canvas_root,true);return LV_AIC_OK;
}
#endif

static int base_page_count(void)
{
#if LV_AIC_GE2D_IMAGE_TEST
    return 3 + LV_AIC_WIDGET_TEST + LV_AIC_NAV_TEST + AIC_LVGL_USE_CANVAS;
#else
    return 1 + LV_AIC_WIDGET_TEST + LV_AIC_NAV_TEST + AIC_LVGL_USE_CANVAS;
#endif
}

int lv_aic_manual_page_count(void)
{
    return base_page_count() + LV_AIC_NATIVE_WIDGET_TEST;
}

int lv_aic_manual_page_current(void)
{
    return lv_aic_manual_root ? lv_aic_manual_page_active : -1;
}

static void lv_aic_nav_update(void)
{
    static const char *const titles[] = {"Overview", "Image rotation", "Rotation + scale"};
    if (!lv_aic_nav_title) return;
#if LV_AIC_NATIVE_WIDGET_TEST
    if(lv_aic_manual_page_active==base_page_count())
        lv_label_set_text(lv_aic_nav_title,"Native dynamic widgets");
    else
#endif
#if AIC_LVGL_USE_CANVAS
    if(lv_aic_manual_page_active==base_page_count()-1)
        lv_label_set_text(lv_aic_nav_title,"AIC canvas");
    else
#endif
#if LV_AIC_WIDGET_TEST
#if LV_AIC_NAV_TEST
    if(lv_aic_manual_page_active==base_page_count()-1-AIC_LVGL_USE_CANVAS)
        lv_label_set_text(lv_aic_nav_title,"List / menu compatibility");
    else
#endif
    if (lv_aic_manual_page_active == base_page_count() - 1 - LV_AIC_NAV_TEST - AIC_LVGL_USE_CANVAS)
        lv_label_set_text(lv_aic_nav_title, "SDK widgets");
    else
#endif
    lv_label_set_text(lv_aic_nav_title, titles[lv_aic_manual_page_active]);
    lv_label_set_text_fmt(lv_aic_nav_position, "%d / %d",
                         lv_aic_manual_page_active + 1, lv_aic_manual_page_count());
}

void lv_aic_manual_page_request(int page)
{
    if (lv_aic_manual_root && page >= 0 && page < lv_aic_manual_page_count())
        lv_aic_manual_page_pending = page;
}

void lv_aic_manual_page_poll(void)
{
    int page = lv_aic_manual_page_pending;
    if (page < 0 || lv_aic_manual_root == NULL) return;
    lv_aic_manual_page_pending = -1;
    if (page >= lv_aic_manual_page_count()) return;
    lv_aic_manual_page_active = page;
#if LV_AIC_NATIVE_WIDGET_TEST
    lv_aic_native_widgets_show(native_widgets_root,page==base_page_count());
#endif
#if AIC_LVGL_USE_CANVAS
    lv_obj_set_hidden(canvas_root,page!=base_page_count()-1);
#endif
    lv_obj_set_hidden(lv_aic_manual_root, page != 0);
#if LV_AIC_WIDGET_TEST
    lv_obj_set_hidden(lv_aic_widget_root, page != base_page_count() - 1 - LV_AIC_NAV_TEST - AIC_LVGL_USE_CANVAS);
#endif
#if LV_AIC_NAV_TEST
    lv_obj_set_hidden(lv_aic_legacy_root,page!=base_page_count()-1-AIC_LVGL_USE_CANVAS);
#endif
#if LV_AIC_GE2D_IMAGE_TEST
    lv_obj_set_hidden(lv_aic_rotation_root, page != 1);
    lv_obj_set_hidden(lv_aic_combo_root, page != 2);
#endif
    lv_aic_nav_update();
}

static void lv_aic_page_button_event(lv_event_t *event)
{
    int direction = (int)(intptr_t)lv_event_get_user_data(event);
    int count = lv_aic_manual_page_count();
    lv_aic_manual_page_request((lv_aic_manual_page_active + direction + count) % count);
    lv_aic_manual_page_poll();
#if LV_AIC_PLANE_TEST_ENABLED
    lv_aic_plane_test_poll();
#endif
}

static lv_obj_t *lv_aic_nav_button(const char *text, int32_t x, int direction)
{
    lv_obj_t *button = lv_button_create(lv_aic_nav_root);
    if (!button) return NULL;
    lv_obj_set_size(button, 88, 44);
    lv_obj_set_pos(button, x, 10);
    lv_obj_set_style_radius(button, 8, 0);
    lv_obj_set_style_shadow_width(button, 0, 0);
    lv_obj_add_event_cb(button, lv_aic_page_button_event, LV_EVENT_CLICKED,
                        (void *)(intptr_t)direction);
    lv_obj_t *label = lv_label_create(button);
    if (!label) return NULL;
    lv_label_set_text(label, text);
    lv_obj_center(label);
    if (lv_aic_manual_page_count() == 1) lv_obj_add_state(button, LV_STATE_DISABLED);
    return button;
}

#if LV_AIC_WIDGET_TEST
static void lv_aic_swipe_button(lv_event_t *e)
{
    lv_obj_t *swipe = lv_event_get_user_data(e);
    lv_swipe_v1_set_next(swipe, LV_ANIM_ON);
}

#if LV_AIC_NAV_TEST
#if defined(__GNUC__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wdeprecated-declarations"
#endif
static int lv_aic_legacy_page_create(lv_display_t *display)
{
    int32_t width=lv_display_get_horizontal_resolution(display);
    int32_t height=lv_display_get_vertical_resolution(display)-64;
    lv_aic_legacy_root=lv_obj_create(lv_screen_active());
    if(!lv_aic_legacy_root) return LV_AIC_ERR_NO_MEMORY;
    lv_obj_set_pos(lv_aic_legacy_root,0,64);lv_obj_set_size(lv_aic_legacy_root,width,height);
    lv_obj_set_style_pad_all(lv_aic_legacy_root,8,0);lv_obj_set_scrollable(lv_aic_legacy_root,false);
    lv_obj_t *list=lv_list_create(lv_aic_legacy_root);
    lv_obj_t *menu=lv_menu_create(lv_aic_legacy_root);
    if(!list || !menu) return LV_AIC_ERR_NO_MEMORY;
    lv_obj_set_size(list,(width-32)/2,height-20);lv_obj_set_pos(list,0,0);
    lv_obj_set_size(menu,(width-32)/2,height-20);lv_obj_set_pos(menu,width/2,0);
    if(!lv_list_add_text(list,"Scroll this list")) return LV_AIC_ERR_NO_MEMORY;
    for(unsigned i=0;i<12;i++) {
        char text[24];lv_snprintf(text,sizeof(text),"Compatibility item %u",i+1);
        if(!lv_list_add_button(list,LV_SYMBOL_FILE,text)) return LV_AIC_ERR_NO_MEMORY;
    }
    lv_obj_t *home=lv_menu_page_create(menu,"Home");
    lv_obj_t *detail=lv_menu_page_create(menu,"Details");
    if(!home || !detail) return LV_AIC_ERR_NO_MEMORY;
    lv_obj_t *entry=lv_menu_cont_create(home),*label;
    if(!entry || !(label=lv_label_create(entry))) return LV_AIC_ERR_NO_MEMORY;
    lv_label_set_text(label,"Open details");lv_menu_set_load_page_event(menu,entry,detail);
    label=lv_label_create(detail);if(!label) return LV_AIC_ERR_NO_MEMORY;
    lv_label_set_text(label,"Use the menu back arrow to return.");
    lv_obj_set_width(label,(width-64)/2);
    lv_menu_set_page(menu,home);lv_obj_set_hidden(lv_aic_legacy_root,true);
    return LV_AIC_OK;
}
#if defined(__GNUC__)
#pragma GCC diagnostic pop
#endif
#endif

static int lv_aic_widget_page_create(lv_display_t *display)
{
    static const char *const symbols[] = {LV_SYMBOL_HOME, LV_SYMBOL_SETTINGS,
                                         LV_SYMBOL_AUDIO, LV_SYMBOL_IMAGE};
    int32_t width = lv_display_get_horizontal_resolution(display);
    int32_t height = lv_display_get_vertical_resolution(display) - 64;
    lv_aic_widget_root = lv_obj_create(lv_screen_active());
    if (!lv_aic_widget_root) return LV_AIC_ERR_NO_MEMORY;
    lv_obj_set_pos(lv_aic_widget_root, 0, 64);
    lv_obj_set_size(lv_aic_widget_root, width, height);
    lv_obj_set_style_pad_all(lv_aic_widget_root, 12, 0);
    lv_obj_set_style_bg_color(lv_aic_widget_root, lv_color_hex(0x202020), 0);
    lv_obj_set_style_text_color(lv_aic_widget_root, lv_color_white(), 0);
    lv_obj_set_scrollable(lv_aic_widget_root, false);
    lv_obj_t *label = lv_label_create(lv_aic_widget_root);
    lv_obj_t *roller = lv_img_roller_create(lv_aic_widget_root);
    lv_obj_t *swipe = lv_swipe_v1_create(lv_aic_widget_root);
    lv_obj_t *button = lv_button_create(lv_aic_widget_root);
    if (!label || !roller || !swipe || !button) return LV_AIC_ERR_NO_MEMORY;
    lv_label_set_text(label, "Tap centered icon, then drag | Swipe: side icons / Next / front icon");
    lv_obj_set_width(label, width - 40);
    lv_obj_set_pos(roller, 8, 44);
    lv_obj_set_size(roller, width - 48, 100);
    lv_img_roller_set_loop_mode(roller, LV_ROLL_LOOP_ON);
    lv_img_roller_set_transform_ratio(roller, 128);
    lv_obj_set_style_pad_column(roller, 12, 0);
    static const uint32_t card_colors[] = {0x315b87, 0x346b5b, 0x785486, 0x876038};
    for (int i = 0; i < 4; i++) {
        lv_obj_t *card = lv_img_roller_add_child(roller, symbols[i]);
        if (!card) return LV_AIC_ERR_NO_MEMORY;
        /* Equal hit areas overflow the viewport, so real board drags exercise
         * loop/snap behavior instead of four tiny symbols that all fit. */
        lv_obj_set_size(card, (width - 48) / 3, 80);
        lv_image_set_inner_align(card, LV_IMAGE_ALIGN_CENTER);
        lv_obj_set_style_bg_color(card, lv_color_hex(card_colors[i]), 0);
        lv_obj_set_style_bg_opa(card, LV_OPA_COVER, 0);
        lv_obj_set_style_radius(card, 8, 0);
        lv_swipe_v1_child_add_state_src(swipe, i, (void *)symbols[i], (void *)symbols[i]);
        lv_swipe_v1_child_add_state_src(swipe, i, (void *)LV_SYMBOL_OK, (void *)LV_SYMBOL_CLOSE);
        lv_obj_t *child = lv_swipe_v1_get_child(swipe, i);
        if (!child) return LV_AIC_ERR_NO_MEMORY;
        lv_obj_set_pos(child, 40 + i * (width - 160) / 4, i == 3 ? 50 : 16);
        lv_obj_set_style_transform_scale(child, i == 3 ? 512 : 256, 0);
    }
    lv_img_roller_set_active(roller, 1, LV_ANIM_OFF);
    lv_img_roller_ready(roller);
    lv_obj_set_pos(swipe, 8, 160);
    lv_obj_set_size(swipe, width - 48, 130);
    lv_swipe_v1_set_anim_params(swipe, 400, NULL);
    lv_obj_set_size(button, 120, 44);
    lv_obj_align(button, LV_ALIGN_BOTTOM_RIGHT, -8, -8);
    lv_obj_add_event_cb(button, lv_aic_swipe_button, LV_EVENT_CLICKED, swipe);
    label = lv_label_create(button);
    if (!label) return LV_AIC_ERR_NO_MEMORY;
    lv_label_set_text(label, "Next icon");
    lv_obj_center(label);
    lv_obj_set_hidden(lv_aic_widget_root, true);
    return LV_AIC_OK;
}
#endif

static int lv_aic_nav_create(lv_display_t *display)
{
    int32_t width = lv_display_get_horizontal_resolution(display);
    /* Reserve the rightmost 104 px for the optional top-layer Fonts launcher.
     * One shared header stays visible on every page; controls never overlap. */
    lv_aic_nav_root = lv_obj_create(lv_layer_top());
    if (!lv_aic_nav_root) return LV_AIC_ERR_NO_MEMORY;
    lv_obj_remove_style_all(lv_aic_nav_root);
    lv_obj_set_size(lv_aic_nav_root, width, 64);
    lv_obj_set_style_bg_opa(lv_aic_nav_root, LV_OPA_COVER, 0);
    lv_obj_set_style_bg_color(lv_aic_nav_root, lv_color_hex(0x132438), 0);
    lv_obj_set_style_text_color(lv_aic_nav_root, lv_color_white(), 0);
    lv_obj_set_scrollable(lv_aic_nav_root, false);
    lv_aic_nav_title = lv_label_create(lv_aic_nav_root);
    lv_aic_nav_position = lv_label_create(lv_aic_nav_root);
    lv_obj_t *subtitle = lv_label_create(lv_aic_nav_root);
    if (!lv_aic_nav_title || !lv_aic_nav_position || !subtitle) return LV_AIC_ERR_NO_MEMORY;
    lv_obj_set_pos(lv_aic_nav_title, 24, 10);
    lv_obj_set_width(lv_aic_nav_title, width - 424);
    lv_label_set_long_mode(lv_aic_nav_title, LV_LABEL_LONG_CLIP);
    lv_obj_set_pos(subtitle, 24, 34);
    lv_label_set_text(subtitle, "LVGL 9.6 | ArtInChip platform tests");
    lv_obj_set_style_text_color(subtitle, lv_color_hex(0xa9bfd5), 0);
    lv_obj_set_width(subtitle, width - 424);
    lv_label_set_long_mode(subtitle, LV_LABEL_LONG_CLIP);
    lv_aic_nav_prev = lv_aic_nav_button("< Prev", width - 384, -1);
    lv_aic_nav_next = lv_aic_nav_button("Next >", width - 208, 1);
    if (!lv_aic_nav_prev || !lv_aic_nav_next) return LV_AIC_ERR_NO_MEMORY;
    lv_obj_set_pos(lv_aic_nav_position, width - 288, 25);
    lv_obj_set_width(lv_aic_nav_position, 72);
    lv_obj_set_style_text_align(lv_aic_nav_position, LV_TEXT_ALIGN_CENTER, 0);
    lv_aic_nav_update();
    return LV_AIC_OK;
}

#if LV_AIC_GE2D_IMAGE_TEST
static int lv_aic_rotation_page_create(lv_display_t *display)
{
    static const int32_t angles[4] = {0, 900, 1800, 2700};
    lv_aic_rotation_root = lv_obj_create(lv_screen_active());
    if (lv_aic_rotation_root == NULL) return LV_AIC_ERR_NO_MEMORY;
    lv_obj_set_size(lv_aic_rotation_root, lv_display_get_horizontal_resolution(display),
                    lv_display_get_vertical_resolution(display) - 64);
    lv_obj_set_pos(lv_aic_rotation_root, 0, 64);
    lv_obj_set_style_bg_color(lv_aic_rotation_root, lv_color_hex(0x202020), 0);
    lv_obj_set_style_bg_opa(lv_aic_rotation_root, LV_OPA_COVER, 0);
    lv_obj_set_style_pad_all(lv_aic_rotation_root, 0, 0);
    lv_obj_set_style_border_width(lv_aic_rotation_root, 0, 0);
    lv_obj_set_style_radius(lv_aic_rotation_root, 0, 0);
    lv_obj_set_style_text_color(lv_aic_rotation_root, lv_color_hex(0xffffff), 0);
    lv_obj_set_scrollable(lv_aic_rotation_root, false);
    lv_obj_center(lv_aic_rotation_root);
    for (int i = 0; i < 4; i++) {
        lv_obj_t *frame = lv_aic_ge2d_make_rect(lv_aic_rotation_root, 24 + i * 190, 100,
                                                160, 160, 0xffffff, 0);
        lv_obj_t *image;
        lv_obj_t *label;
        if (!frame) return LV_AIC_ERR_NO_MEMORY;
        image = lv_aic_ge2d_make_image(frame, i & 1 ? "L:/data/mpp_test/c.png" :
                                       "L:/data/mpp_test/b.png", 64, 64);
        label = lv_label_create(lv_aic_rotation_root);
        if (!image || !label) return LV_AIC_ERR_NO_MEMORY;
        lv_image_set_pivot(image, 16, 16);
        lv_image_set_rotation(image, angles[i]);
        lv_obj_set_pos(label, 32 + i * 190, 76);
        {
            char text[12];
            lv_snprintf(text, sizeof(text), "%d deg", (int)(angles[i] / 10));
            lv_label_set_text(label, text);
        }
    }
    lv_obj_set_hidden(lv_aic_rotation_root, true);
    return LV_AIC_OK;
}

static int lv_aic_combo_page_create(lv_display_t *display)
{
    static const int32_t rotations[4] = {900, 900, 1800, 1800};
    static const uint32_t scales[4] = {128, 384, 512, 512};
    static const char *const scale_text[4] = {"0.5x", "1.5x", "2.0x", "2.0x"};
    lv_aic_combo_root = lv_obj_create(lv_screen_active());
    if (lv_aic_combo_root == NULL) return LV_AIC_ERR_NO_MEMORY;
    lv_obj_set_size(lv_aic_combo_root, lv_display_get_horizontal_resolution(display),
                    lv_display_get_vertical_resolution(display) - 64);
    lv_obj_set_pos(lv_aic_combo_root, 0, 64);
    lv_obj_set_style_bg_color(lv_aic_combo_root, lv_color_hex(0x202020), 0);
    lv_obj_set_style_bg_opa(lv_aic_combo_root, LV_OPA_COVER, 0);
    lv_obj_set_style_pad_all(lv_aic_combo_root, 0, 0);
    lv_obj_set_style_border_width(lv_aic_combo_root, 0, 0);
    lv_obj_set_style_radius(lv_aic_combo_root, 0, 0);
    lv_obj_set_scrollable(lv_aic_combo_root, false);
    lv_obj_set_style_text_color(lv_aic_combo_root, lv_color_white(), 0);
    lv_obj_center(lv_aic_combo_root);
    for (int i = 0; i < 4; i++) {
        int32_t x = 24 + i * 190;
        lv_obj_t *frame = lv_aic_ge2d_make_rect(lv_aic_combo_root, x, 112, 160, 160,
                                                0xffffff, 0);
        lv_obj_t *image;
        lv_obj_t *label;
        if (!frame) return LV_AIC_ERR_NO_MEMORY;
        image = lv_aic_ge2d_make_image(frame, (i & 1) ? "L:/data/mpp_test/c.png" :
                                       "L:/data/mpp_test/b.png", 64, 64);
        label = lv_label_create(lv_aic_combo_root);
        if (!image || !label) return LV_AIC_ERR_NO_MEMORY;
        lv_image_set_pivot(image, 16, 16);
        lv_image_set_scale_x(image, scales[i]);
        lv_image_set_scale_y(image, scales[i]);
        lv_image_set_rotation(image, rotations[i]);
        if (i & 1) lv_obj_set_style_image_opa(image, 128, 0);
        lv_obj_set_pos(label, x, 78);
        lv_obj_set_width(label, 170);
        lv_label_set_text_fmt(label, "%s | %s / %d deg", (i & 1) ? "ARGB" : "RGB",
                              scale_text[i], (int)(rotations[i] / 10));
    }
    lv_obj_set_hidden(lv_aic_combo_root, true);
    return LV_AIC_OK;
}
#endif

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
    lv_obj_set_scrollable(lv_aic_manual_root, false);

    lv_aic_manual_status = lv_label_create(lv_aic_manual_root);
    if (lv_aic_manual_status == NULL) {
        goto fail;
    }
    lv_label_set_text(lv_aic_manual_status, "Touch: ready");
    lv_obj_set_style_text_color(lv_aic_manual_status, lv_color_hex(0xffffff), 0);
    lv_obj_set_pos(lv_aic_manual_status, 24, 140);

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
    lv_label_set_text(button_label, "Test touch");
    lv_obj_center(button_label);

    lv_aic_manual_marker = lv_obj_create(lv_aic_manual_root);
    if (lv_aic_manual_marker == NULL || lv_aic_manual_page_active != 0) {
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
                      "MPP decode: JPEG | RGB | RGBA on white");
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
                      "GE2D fill: square = hardware | rounded = software");
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
    /* The same two fixtures the Phase 2 row scales, at their native 32x32.
     * Unscaled they are plain blits, and the decoder's output format picks the
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

    /* Phase 3C1: image opacity. The Phase 3B row shows both fixtures at full
     * opacity; this row adds the global-opacity axis, which is the half of the
     * blend the port used to decline.
     *
     * Each trio sits on a backdrop that differs from the image, because that is
     * what makes a partial opacity visible at all. b.png (RGB, no alpha) over
     * mid grey shows the global opacity alone; c.png (RGBA) over white shows it
     * multiplying its own 0..255 alpha ramp, so the two contributions stay
     * distinguishable instead of one masking the other.
     *
     * The row is also the visual counterpart of the numeric blend probe in
     * lv_aic_ge2d_test.c: the probe says the arithmetic matches LVGL, this says
     * the result reaches the panel through the real RGB565 draw buffer. It is
     * placed in the strip above the Phase 2 row, clear of the moving marker, so
     * no existing probe moves. */
    lv_aic_ge2d_c1_label = lv_label_create(lv_aic_manual_root);
    if (lv_aic_ge2d_c1_label == NULL) {
        goto fail;
    }
    lv_label_set_text(lv_aic_ge2d_c1_label, "Opacity: RGB / ARGB 255,128,64");
    lv_obj_set_style_text_color(lv_aic_ge2d_c1_label, lv_color_hex(0xffffff), 0);
    lv_obj_set_pos(lv_aic_ge2d_c1_label, 320, 146);

    lv_aic_ge2d_c1_backdrop[0] = lv_aic_ge2d_make_rect(lv_aic_manual_root,
                                                       320, 166, 104, 32,
                                                       0x808080, 0);
    lv_aic_ge2d_c1_backdrop[1] = lv_aic_ge2d_make_rect(lv_aic_manual_root,
                                                       428, 166, 104, 32,
                                                       0xffffff, 0);
    if ((lv_aic_ge2d_c1_backdrop[0] == NULL) || (lv_aic_ge2d_c1_backdrop[1] == NULL)) {
        goto fail;
    }

    {
        /* The three opacities the phase tests, in the order they appear. */
        static const lv_opa_t opas[3] = { LV_OPA_COVER, 128, 64 };

        for (int i = 0; i < LV_AIC_GE2D_C1_COUNT; i++) {
            lv_aic_ge2d_c1_img[i] = lv_aic_ge2d_make_image(lv_aic_manual_root,
                                                           (i < 3)
                                                           ? "L:/data/mpp_test/b.png"
                                                           : "L:/data/mpp_test/c.png",
                                                           320 + i * 36, 166);
            if (lv_aic_ge2d_c1_img[i] == NULL) {
                goto fail;
            }
            lv_obj_set_style_image_opa(lv_aic_ge2d_c1_img[i], opas[i % 3], 0);
        }
    }
    /* 3C2: use the free upper-right strip, preserving the earlier page rows.
     * Root ownership releases these children on every lifecycle test. */
    {
        static const uint16_t scales[8] = {128,384,512,128,384,512,384,384};
        static const char *const names[8] = {"R .5", "R 1.5", "R 2", "A .5",
                                              "A 1.5", "A 2", "XY", "clip"};
        for (int i = 0; i < 8; i++) {
            lv_obj_t *label = lv_label_create(lv_aic_manual_root);
            lv_obj_t *frame = lv_aic_ge2d_make_rect(lv_aic_manual_root,
                                                    220+i*70,78,i==7?20:64,i==7?13:64,0xffffff,0);
            lv_obj_t *image;
            if (!label || !frame) goto fail;
            lv_obj_set_scrollable(frame, false);
            lv_label_set_text(label,names[i]);
            lv_obj_set_style_text_color(label,lv_color_hex(0xffffff),0);
            lv_obj_set_pos(label,220+i*70,58);
            image = lv_aic_ge2d_make_image(frame,
                      i<3 ? "L:/data/mpp_test/b.png" : "L:/data/mpp_test/c.png",
                      i==7 ? -1 : 0,i==7 ? -3 : 0);
            if (!image) goto fail;
            lv_image_set_pivot(image,i==7?7:0,i==7?9:0);
            lv_image_set_scale_x(image,scales[i]);
            lv_image_set_scale_y(image,i>=6?192:scales[i]);
            if (i>=3) lv_obj_set_style_image_opa(image,128,0);
        }
    }
#endif /* LV_AIC_GE2D_IMAGE_TEST */

    lv_aic_ge2d_layer = lv_aic_ge2d_make_rect(lv_aic_manual_root, 600, 312, 88, 40,
                                              0x40a0e0, 0);
    if (lv_aic_ge2d_layer == NULL) {
        goto fail;
    }
    lv_obj_set_style_opa_layered(lv_aic_ge2d_layer, LV_OPA_50, 0);
#endif /* AIC_LVGL_USE_GE2D */

#if AIC_LVGL_USE_GE2D && AIC_LVGL_USE_MPP_DEC
    if (lv_aic_rotation_page_create(display) != LV_AIC_OK) goto fail;
    if (lv_aic_combo_page_create(display) != LV_AIC_OK) goto fail;
#endif

#if LV_AIC_WIDGET_TEST
    if (lv_aic_widget_page_create(display) != LV_AIC_OK) goto fail;
#if LV_AIC_NAV_TEST
    if(lv_aic_legacy_page_create(display)!=LV_AIC_OK) goto fail;
#endif
#endif
#if AIC_LVGL_USE_CANVAS
    if(canvas_page_create(display)!=LV_AIC_OK) goto fail;
#endif
#if LV_AIC_NATIVE_WIDGET_TEST
    native_widgets_root=lv_aic_native_widgets_create(lv_screen_active(),
        lv_display_get_horizontal_resolution(display),lv_display_get_vertical_resolution(display)-64);
    if(!native_widgets_root) goto fail;
    lv_obj_set_pos(native_widgets_root,0,64);
#endif
    if (lv_aic_nav_create(display) != LV_AIC_OK) goto fail;

#if LV_USE_FREETYPE && AIC_LVGL_BSP_RTTHREAD
    if (lv_aic_font_test_create("/data/mpp_test/Lato-Regular.ttf",
                               "/data/mpp_test/NotoSansSC-Regular.ttf") != LV_AIC_OK) goto fail;
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
#if LV_AIC_NATIVE_WIDGET_TEST
    if(native_widgets_root) lv_obj_delete(native_widgets_root);
    native_widgets_root=NULL;
#endif
#if AIC_LVGL_USE_CANVAS
    if(canvas_root) lv_obj_delete(canvas_root);
    canvas_root=canvas_image=canvas_status=NULL;
#endif
#if LV_AIC_PLANE_TEST_ENABLED
    lv_aic_plane_test_deinit();
#endif
#if defined(AIC_LVGL_USE_APNG_WIDGET) && AIC_LVGL_USE_APNG_WIDGET
    lv_aic_apng_test_deinit();
#endif
    /* A running official demo owns a screen and timers; tear it down with
     * the page instead of leaving it behind. */
#if defined(AIC_LVGL_OFFICIAL_DEMOS) && AIC_LVGL_OFFICIAL_DEMOS
    lv_aic_demo_test_deinit();
#endif
#if LV_AIC_WIDGET_TEST
    if (lv_aic_widget_root) lv_obj_delete(lv_aic_widget_root);
    lv_aic_widget_root = NULL;
#if LV_AIC_NAV_TEST
    if(lv_aic_legacy_root) lv_obj_delete(lv_aic_legacy_root);
    lv_aic_legacy_root=NULL;
#endif
#endif
#if LV_USE_GIF && AIC_LVGL_BSP_RTTHREAD
    lv_aic_gif_test_deinit();
#endif
#if LV_USE_FREETYPE && AIC_LVGL_BSP_RTTHREAD
    lv_aic_font_test_delete();
#endif
    if (lv_aic_nav_root) lv_obj_delete(lv_aic_nav_root);
    lv_aic_nav_root = lv_aic_nav_title = lv_aic_nav_position = NULL;
    lv_aic_nav_prev = lv_aic_nav_next = NULL;
    if (lv_aic_manual_timer != NULL) {
        lv_timer_delete(lv_aic_manual_timer);
        lv_aic_manual_timer = NULL;
    }
#if AIC_LVGL_USE_GE2D && AIC_LVGL_USE_MPP_DEC
    if (lv_aic_rotation_root != NULL) {
        lv_obj_delete(lv_aic_rotation_root);
        lv_aic_rotation_root = NULL;
    }
    if (lv_aic_combo_root != NULL) {
        lv_obj_delete(lv_aic_combo_root);
        lv_aic_combo_root = NULL;
    }
#endif
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
        lv_aic_ge2d_c1_label = NULL;
        lv_aic_ge2d_c1_backdrop[0] = NULL;
        lv_aic_ge2d_c1_backdrop[1] = NULL;
        for (int i = 0; i < LV_AIC_GE2D_C1_COUNT; i++) {
            lv_aic_ge2d_c1_img[i] = NULL;
        }
#endif
    }
    lv_aic_manual_marker_x = 0;
    lv_aic_manual_page_pending = -1;
    lv_aic_manual_page_active = 0;
}
