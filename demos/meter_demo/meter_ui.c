/* SPDX-License-Identifier: Apache-2.0 */
/*
 * Meter cluster demo for LVGL 9.6, adapted from
 * packages/artinchip/lvgl-ui/aic_demo/meter_demo/meter_ui.c.
 * See meter_ui.h for the kept/adapted/open split.
 */
#include "demos/meter_demo/meter_ui.h"

#include <stdio.h>
#include <string.h>

#if LV_FONT_MONTSERRAT_14
#define METER_FONT (&lv_font_montserrat_14)
#else
#define METER_FONT (LV_FONT_DEFAULT)
#endif

/* Design resolution of the vendor assets. Widget geometry below is authored
 * in these coordinates (the same numbers as the reference) and mapped with
 * meter_fit(); bitmap widgets share the ratio as LVGL zoom (256 = 1.0)
 * about their top-left corner so the whole design scales uniformly. */
#define METER_DESIGN_W 1024
#define METER_DESIGN_H 600

struct rot_id_info {
    int start_id;
    int end_id;
};

static lv_obj_t *bg_fps = NULL;
static lv_obj_t *img_bg = NULL;
static lv_obj_t *img_circle = NULL;
static lv_obj_t *img_point = NULL;
static lv_obj_t *img_engine = NULL;
static lv_obj_t *img_esp = NULL;
static lv_obj_t *img_oil_less = NULL;
static lv_obj_t *img_battery = NULL;
static lv_obj_t *img_battery_num = NULL;
static lv_obj_t *img_battery_hl = NULL;
static lv_obj_t *img_oil_fe = NULL;
static lv_obj_t *img_oil_num = NULL;
static lv_obj_t *img_oil = NULL;
static lv_obj_t *img_water_temp = NULL;
static lv_obj_t *img_water_num = NULL;
static lv_obj_t *img_gear = NULL;

static lv_obj_t *img_trip = NULL;
static lv_obj_t *img_trip_0 = NULL;
static lv_obj_t *img_trip_1 = NULL;
static lv_obj_t *img_trip_2 = NULL;
static lv_obj_t *img_trip_3 = NULL;
static lv_obj_t *img_trip_km = NULL;
static lv_obj_t *img_speed_num = NULL;
static lv_obj_t *img_speed_km = NULL;

static lv_obj_t *img_time = NULL;
static lv_obj_t *img_time_0 = NULL;
static lv_obj_t *img_time_1 = NULL;
static lv_obj_t *img_time_2 = NULL;
static lv_obj_t *img_time_3 = NULL;
static lv_obj_t *img_time_dot = NULL;
static lv_timer_t *speed_timer = NULL;

/* Fallback shapes for asset-less (host) runs; NULL once assets are bound. */
static lv_obj_t *fb_needle = NULL;
static lv_obj_t *fb_speed = NULL;
static lv_obj_t *fb_time = NULL;
static lv_obj_t *fb_trip = NULL;
static lv_obj_t *fb_gear = NULL;
static lv_obj_t *fb_status = NULL;
static lv_timer_t *meter_timers[8];

static struct {
    char asset_root[128];
    meter_ui_fps_fn fps;
    int32_t needle_angle;
    int speed_step;
    uint32_t fires;
    uint32_t zoom;
    bool created;
} meter;

struct rot_id_info rot_mode_list[] = {
    {1, 74},
    {74, 1},
    {1, 149},
    {149, 75},
    {75, 149},
    {149, 1},
};

static int32_t meter_fit(int32_t v)
{
    return (int32_t)((v * (int32_t)meter.zoom) / 256);
}

static inline void meter_ui_set_pos(lv_obj_t *obj, int32_t x, int32_t y)
{
    lv_obj_set_pos(obj, meter_fit(x + 112), meter_fit(y + 60));
}

/* Build "<root>/<sub>" into buf; NULL when no asset root is configured. */
static const char *meter_asset_path(const char *sub, char *buf, size_t size)
{
    if (meter.asset_root[0] == '\0') {
        return NULL;
    }
    snprintf(buf, size, "%s/%s", meter.asset_root, sub);
    return buf;
}

static void meter_track_fire(void)
{
    meter.fires++;
}

/* Bitmap widget with the shared uniform scale; caller positions it. */
static lv_obj_t *meter_img(lv_obj_t *parent, const char *sub)
{
    char path[192];
    lv_obj_t *img = lv_image_create(parent);
    if (meter_asset_path(sub, path, sizeof(path)) != NULL) {
        lv_image_set_src(img, path);
    }
    lv_image_set_pivot(img, 0, 0);
    lv_image_set_scale(img, meter.zoom);
    return img;
}

static lv_obj_t *meter_label(lv_obj_t *parent, int32_t x, int32_t y, const char *text)
{
    lv_obj_t *label = lv_label_create(parent);
    lv_obj_set_pos(label, x, y);
    lv_label_set_text(label, text);
    lv_obj_set_style_text_font(label, METER_FONT, 0);
    return label;
}

static void point_callback(lv_timer_t *tmr)
{
    /* data_str returns with the phase-2 needle strip binding. */
    (void)tmr;

    static bool first = true;
    static int id = 1;
    static int direct = 0;
    static int mode_id = 0;
    static int mode_num = sizeof(rot_mode_list) / sizeof(rot_mode_list[0]);
    static int start_id = 0;
    static int end_id = 0;

    meter_track_fire();
    if (first) {
        first = false;
        start_id = rot_mode_list[mode_id].start_id;
        end_id = rot_mode_list[mode_id].end_id;
    }

    direct = start_id < end_id ? 0 : 1;

    if (meter.asset_root[0] != '\0') {
        /* Asset path mirrors the reference non-SIMPLE branch except the
         * needle strip: frames are bound in phase 2, so the simple
         * rotation bar below is used until then. Circle visibility still
         * follows the reference. */
        if (id < 75) {
            lv_obj_set_hidden(img_circle, false);
        } else {
            lv_obj_set_hidden(img_circle, true);
        }
    } else if (id < 75) {
        lv_obj_set_hidden(img_circle, false);
    } else {
        lv_obj_set_hidden(img_circle, true);
    }

    /* Reference LV_METER_SIMPLE_POINT rotation math on every target. */
    {
        float rot_angle;
        if (id < 75) {
            rot_angle = ((float)(74 - id) * 2 * 10) * 0.84f;
            if (rot_angle > 0) {
                rot_angle = 3600.0f - rot_angle;
            }
        } else {
            rot_angle = ((float)(id - 75) * 2 * 10) * 0.84f;
        }
        meter.needle_angle = (int32_t)rot_angle;
        if (fb_needle != NULL) {
            lv_obj_set_style_transform_rotation(fb_needle, (int32_t)rot_angle, 0);
        }
        /* Reference sweep pacing (D13x/G73X credited 150/300 ms). */
        if (speed_timer != NULL) {
            lv_timer_set_period(speed_timer, (id < 75) ? 150 : 300);
        }
    }

    if (direct == 0) {
        id++;
    } else {
        id--;
    }

    if ((!direct && (id > end_id)) ||
        (direct && (id < end_id))) {
        id = end_id;
        mode_id++;
        mode_id %= mode_num;
        start_id = rot_mode_list[mode_id].start_id;
        end_id = rot_mode_list[mode_id].end_id;
    }

    return;
}

static void speed_callback(lv_timer_t *tmr)
{
    char data_str[256];
    static int speed_num = 0;

    (void)tmr;
    meter_track_fire();
    snprintf(data_str, sizeof(data_str), "speed_num/%d.png", speed_num);
    meter.speed_step = speed_num;
    if (meter.asset_root[0] != '\0') {
        char path[192];
        if (meter_asset_path(data_str, path, sizeof(path)) != NULL) {
            lv_image_set_src(img_speed_num, path);
        }
    } else if (fb_speed != NULL) {
        snprintf(data_str, sizeof(data_str), "%d", speed_num);
        lv_label_set_text(fb_speed, data_str);
    }
    speed_num++;
    speed_num = speed_num > 9 ? 0 : speed_num;
}

static void time_callback(lv_timer_t *tmr)
{
    char data_str[256];
    static int hour = 2;
    static int min = 0;

    (void)tmr;
    meter_track_fire();
    min++;
    if (min >= 60) {
        hour++;
        min = 0;
    }

    if (hour >= 24) {
        hour = 0;
    }

    if (meter.asset_root[0] != '\0') {
        char path[192];
        snprintf(data_str, sizeof(data_str), "time/%d.png", hour / 10);
        if (meter_asset_path(data_str, path, sizeof(path)) != NULL) {
            lv_image_set_src(img_time_0, path);
        }
        snprintf(data_str, sizeof(data_str), "time/%d.png", hour % 10);
        if (meter_asset_path(data_str, path, sizeof(path)) != NULL) {
            lv_image_set_src(img_time_1, path);
        }
        snprintf(data_str, sizeof(data_str), "time/%d.png", min / 10);
        if (meter_asset_path(data_str, path, sizeof(path)) != NULL) {
            lv_image_set_src(img_time_2, path);
        }
        snprintf(data_str, sizeof(data_str), "time/%d.png", min % 10);
        if (meter_asset_path(data_str, path, sizeof(path)) != NULL) {
            lv_image_set_src(img_time_3, path);
        }
    } else if (fb_time != NULL) {
        snprintf(data_str, sizeof(data_str), "%02d:%02d", hour, min);
        lv_label_set_text(fb_time, data_str);
    }
}

static void trip_callback(lv_timer_t *tmr)
{
    char data_str[256];
    int num[4];
    int cur;
    static int trip = 98;

    (void)tmr;
    meter_track_fire();
    trip++;
    if (trip >= 9999) {
        trip = 0;
    }

    num[0] = trip / 1000;
    cur = trip % 1000;
    num[1] = cur / 100;
    cur = cur % 100;
    num[2] = cur / 10;
    num[3] = cur % 10;

    if (meter.asset_root[0] != '\0') {
        char path[192];
        lv_obj_t *digits[4] = {img_trip_0, img_trip_1, img_trip_2, img_trip_3};
        int i;
        for (i = 0; i < 4; i++) {
            snprintf(data_str, sizeof(data_str), "mileage/%d.png", num[i]);
            if (meter_asset_path(data_str, path, sizeof(path)) != NULL) {
                lv_image_set_src(digits[i], path);
            }
        }
    } else if (fb_trip != NULL) {
        snprintf(data_str, sizeof(data_str), "%04d", trip);
        lv_label_set_text(fb_trip, data_str);
    }
}

static void obj_set_clear_hidden_flag(lv_obj_t *obj)
{
    if (lv_obj_is_hidden(obj)) {
        lv_obj_set_hidden(obj, false);
    } else {
        lv_obj_set_hidden(obj, true);
    }
}

static void water_anim(void)
{
    char data_str[256];
    static int level = 1;
    static int direct = 0;

    snprintf(data_str, sizeof(data_str), "water_temp/%d.png", level);
    if (meter.asset_root[0] != '\0') {
        char path[192];
        char alt[192];
        if (meter_asset_path(data_str, path, sizeof(path)) != NULL) {
            lv_image_set_src(img_water_num, path);
        }
        if (meter_asset_path(level < 19 ? "water_temp/water_temp2.png" : "water_temp/water_temp.png",
                             alt, sizeof(alt)) != NULL) {
            lv_image_set_src(img_water_temp, alt);
        }
    }

    if (direct == 0) {
        level++;
    } else {
        level--;
    }

    if (level > 20) {
        level = 20;
        direct = 1;
    }

    if (level < 1) {
        level = 1;
        direct = 0;
    }
}

static void baterry_anim(void)
{
    char data_str[256];
    static int level = 1;
    static int direct = 0;

    snprintf(data_str, sizeof(data_str), "battery/%d.png", level);
    if (meter.asset_root[0] != '\0') {
        char path[192];
        char alt[192];
        if (meter_asset_path(data_str, path, sizeof(path)) != NULL) {
            lv_image_set_src(img_battery_num, path);
        }
        if (meter_asset_path(level > 1 ? "battery/battery2.png" : "battery/battery.png",
                             alt, sizeof(alt)) != NULL) {
            lv_image_set_src(img_battery, alt);
        }
    }

    if (direct == 0) {
        level++;
    } else {
        level--;
    }

    if (level > 5) {
        level = 5;
        direct = 1;
    }

    if (level < 1) {
        level = 1;
        direct = 0;
    }
}

static void oil_anim(void)
{
    char data_str[256];
    static int level = 1;
    static int direct = 0;

    snprintf(data_str, sizeof(data_str), "oil_level/%d.png", level);
    if (meter.asset_root[0] != '\0') {
        char path[192];
        char alt[192];
        if (meter_asset_path(data_str, path, sizeof(path)) != NULL) {
            lv_image_set_src(img_oil_num, path);
        }
        if (meter_asset_path(level > 1 ? "oil_level/oil_level2.png" : "oil_level/oil_level.png",
                             alt, sizeof(alt)) != NULL) {
            lv_image_set_src(img_oil, alt);
        }
    }

    if (direct == 0) {
        level++;
    } else {
        level--;
    }

    if (level > 5) {
        level = 5;
        direct = 1;
    }

    if (level < 1) {
        level = 1;
        direct = 0;
    }
}

static void gear_callback(lv_timer_t *tmr)
{
    char data_str[256];
    static int level = 1;
    static int direct = 0;

    (void)tmr;

    if (level <= 6) {
        snprintf(data_str, sizeof(data_str), "gear/d%d.png", level);
    } else {
        snprintf(data_str, sizeof(data_str), "gear/n.png");
    }

    if (meter.asset_root[0] != '\0') {
        char path[192];
        if (meter_asset_path(data_str, path, sizeof(path)) != NULL) {
            lv_image_set_src(img_gear, path);
        }
    } else if (fb_gear != NULL) {
        if (level <= 6) {
            snprintf(data_str, sizeof(data_str), "d%d", level);
        } else {
            snprintf(data_str, sizeof(data_str), "n");
        }
        lv_label_set_text(fb_gear, data_str);
    }

    if (direct == 0) {
        level++;
    } else {
        level--;
    }

    if (level > 7) {
        level = 7;
        direct = 1;
    }

    if (level < 1) {
        level = 1;
        direct = 0;
    }
}

static void other_callback(lv_timer_t *tmr)
{
    static int mode = 0;
    char text[48];
    (void)tmr;

    meter_track_fire();
    if (mode <= 2) {
        water_anim();
        baterry_anim();
    } else if (mode > 2 && mode <= 5) {
        water_anim();
        oil_anim();
    }

    mode++;
    if (mode > 5) {
        mode = 0;
    }

    if (fb_status != NULL) {
        snprintf(text, sizeof(text), "mode %d", mode);
        lv_label_set_text(fb_status, text);
    }
}

static void signal_callback(lv_timer_t *tmr)
{
    static int mode = 0;
    (void)tmr;

    meter_track_fire();
    if (mode == 0) {
        obj_set_clear_hidden_flag(img_engine);
    } else if (mode == 1) {
        obj_set_clear_hidden_flag(img_engine);
    } else if (mode == 2) {
        obj_set_clear_hidden_flag(img_esp);
    } else if (mode == 3) {
        obj_set_clear_hidden_flag(img_esp);
    } else if (mode == 4) {
        obj_set_clear_hidden_flag(img_oil_less);
    } else if (mode == 5) {
        obj_set_clear_hidden_flag(img_oil_less);
    }

    mode++;
    if (mode > 5) {
        mode = 0;
    }
}

static void fps_callback(lv_timer_t *tmr)
{
    char data_str[256];
    int fps;

    (void)tmr;
    meter_track_fire();
    if (bg_fps == NULL || meter.fps == NULL) {
        return;
    }
    fps = meter.fps();
    if (fps < 0) {
        lv_obj_set_hidden(bg_fps, true);
        return;
    }
    lv_obj_set_hidden(bg_fps, false);

    /* frame rate */
    snprintf(data_str, sizeof(data_str), "%2d FPS", fps);
    lv_label_set_text(bg_fps, data_str);
}

void meter_ui_configure(const meter_ui_config_t *config)
{
    if (meter.created) {
        return;
    }
    if (config != NULL) {
        meter.fps = config->fps;
        if (config->asset_root != NULL) {
            snprintf(meter.asset_root, sizeof(meter.asset_root), "%s", config->asset_root);
        }
    }
}

static lv_obj_t *meter_warn_dot(lv_obj_t *parent, int32_t x, int32_t y, lv_color_t color)
{
    int32_t d = meter_fit(24);
    lv_obj_t *dot = lv_obj_create(parent);
    lv_obj_set_size(dot, d, d);
    lv_obj_set_pos(dot, x, y);
    lv_obj_set_style_bg_color(dot, color, 0);
    lv_obj_set_style_bg_opa(dot, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(dot, d / 2, 0);
    lv_obj_set_style_border_width(dot, 0, 0);
    return dot;
}

void meter_ui_init(void)
{
    lv_obj_t *root;
    int32_t width;
    int32_t height;
    size_t i;
    static const char *const warn_assets[3] = {
        "warning/engine_err.png", "warning/esp.png", "warning/oil_less.png",
    };

    if (meter.created) {
        return;
    }

    width = (int32_t)lv_display_get_horizontal_resolution(lv_display_get_default());
    height = (int32_t)lv_display_get_vertical_resolution(lv_display_get_default());
    /* Uniform letterbox fit of the 1024x600 design into any panel. */
    {
        uint32_t zx = (uint32_t)((width * 256) / METER_DESIGN_W);
        uint32_t zy = (uint32_t)((height * 256) / METER_DESIGN_H);
        meter.zoom = (zx < zy) ? zx : zy;
        if (meter.zoom == 0) {
            meter.zoom = 1;
        }
    }

    /* Overlay on the top layer (same pattern as the APNG acceptance
     * panel): the smoke/manual nav header lives on lv_layer_top and
     * survives screen loads, so a new screen cannot cover it. */
    root = lv_obj_create(lv_layer_top());
    if (root == NULL) {
        return;
    }
    img_bg = root;
    lv_obj_set_size(img_bg, width, height);
    lv_obj_set_pos(img_bg, 0, 0);
    lv_obj_set_style_bg_color(img_bg, lv_color_hex(0x101418), 0);
    lv_obj_set_style_bg_opa(img_bg, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(img_bg, 0, 0);
    lv_obj_set_style_radius(img_bg, 0, 0);
    lv_obj_set_scrollable(img_bg, false);

    if (meter.asset_root[0] != '\0') {
        char path[192];
        lv_obj_t *bg;
        /* LVGL scales about the pivot (pivot stays put); compensate so the
         * scaled design centers on the panel instead of drifting right by
         * pivot * (1 - zoom). */
        int32_t bg_w = meter_fit(METER_DESIGN_W);
        int32_t bg_h = meter_fit(METER_DESIGN_H);
        int32_t bg_x = (width - bg_w) / 2 -
                       (int32_t)(((METER_DESIGN_W / 2) * (256 - (int32_t)meter.zoom)) / 256);
        int32_t bg_y = (height - bg_h) / 2 -
                       (int32_t)(((METER_DESIGN_H / 2) * (256 - (int32_t)meter.zoom)) / 256);
        snprintf(path, sizeof(path), "%s/bg/bg_red.jpg", meter.asset_root);
        bg = lv_image_create(img_bg);
        lv_image_set_src(bg, path);
        lv_image_set_pivot(bg, METER_DESIGN_W / 2, METER_DESIGN_H / 2);
        lv_image_set_scale(bg, meter.zoom);
        lv_obj_set_pos(bg, bg_x, bg_y);
    }

    /* Warning icons: vendor PNGs with assets, fallback dots on host. */
    if (meter.asset_root[0] != '\0') {
        img_engine = meter_img(img_bg, warn_assets[0]);
        img_esp = meter_img(img_bg, warn_assets[1]);
        img_oil_less = meter_img(img_bg, warn_assets[2]);
        meter_ui_set_pos(img_engine, 21, 13);
        meter_ui_set_pos(img_esp, 109, 12);
        meter_ui_set_pos(img_oil_less, 740, 12);
    } else {
        img_engine = meter_warn_dot(img_bg, meter_fit(21), meter_fit(73), lv_color_hex(0xff8020));
        img_esp = meter_warn_dot(img_bg, meter_fit(221), meter_fit(72), lv_color_hex(0xff2020));
        img_oil_less = meter_warn_dot(img_bg, width - meter_fit(68),
                                      meter_fit(72), lv_color_hex(0xffc020));
    }

    /* Water */
    img_water_temp = meter_img(img_bg, "water_temp/water_temp2.png");
    meter_ui_set_pos(img_water_temp, 59, 178);

    img_water_num = meter_img(img_bg, "water_temp/1.png");
    meter_ui_set_pos(img_water_num, 117, 121);

    /* Battery */
    img_battery = meter_img(img_bg, "battery/battery.png");
    meter_ui_set_pos(img_battery, 12, 349);

    img_battery_hl = meter_img(img_bg, "battery/h_l.png");
    meter_ui_set_pos(img_battery_hl, 39, 282);
    img_battery_num = meter_img(img_bg, "battery/5.png");
    meter_ui_set_pos(img_battery_num, 56, 281);

    /* Oil */
    img_oil_num = meter_img(img_bg, "oil_level/5.png");
    meter_ui_set_pos(img_oil_num, 628, 281);

    img_oil_fe = meter_img(img_bg, "oil_level/fe.png");
    meter_ui_set_pos(img_oil_fe, 731, 282);
    img_oil = meter_img(img_bg, "oil_level/oil_level.png");
    meter_ui_set_pos(img_oil, 754, 352);

    img_gear = meter_img(img_bg, "gear/d6.png");
    meter_ui_set_pos(img_gear, 357, 380);
    img_circle = meter_img(img_bg, "bg/small_blue.png");
    meter_ui_set_pos(img_circle, 306, 152);

    /* Trip */
    img_trip = meter_img(img_bg, "mileage/trip.png");
    meter_ui_set_pos(img_trip, 9, 439);

    img_trip_0 = meter_img(img_bg, "mileage/0.png");
    meter_ui_set_pos(img_trip_0, 69 + 5, 439);

    img_trip_1 = meter_img(img_bg, "mileage/0.png");
    meter_ui_set_pos(img_trip_1, 89, 439);

    img_trip_2 = meter_img(img_bg, "mileage/9.png");
    meter_ui_set_pos(img_trip_2, 109 - 5, 439);

    img_trip_3 = meter_img(img_bg, "mileage/8.png");
    meter_ui_set_pos(img_trip_3, 129 - 10, 439);

    img_trip_km = meter_img(img_bg, "mileage/km.png");
    meter_ui_set_pos(img_trip_km, 149 - 10, 439);

    /* Speed */
    img_speed_num = meter_img(img_bg, "speed_num/0.png");
    meter_ui_set_pos(img_speed_num, 372, 204);

    img_speed_km = meter_img(img_bg, "speed_num/mph.png");
    meter_ui_set_pos(img_speed_km, 361, 284);

    /* Time */
    img_time = meter_img(img_bg, "time/time.png");
    meter_ui_set_pos(img_time, 690, 154);

    img_time_0 = meter_img(img_bg, "time/0.png");
    meter_ui_set_pos(img_time_0, 690 - 29, 154 - 28);
    img_time_1 = meter_img(img_bg, "time/2.png");
    meter_ui_set_pos(img_time_1, 690, 154 - 28);

    img_time_dot = meter_img(img_bg, "time/dot.png");
    meter_ui_set_pos(img_time_dot, 690 + 29, 154 - 28);

    img_time_2 = meter_img(img_bg, "time/0.png");
    meter_ui_set_pos(img_time_2, 690 + 29 + 17, 154 - 28);
    img_time_3 = meter_img(img_bg, "time/0.png");
    meter_ui_set_pos(img_time_3, 690 + 29 * 2 + 17, 154 - 28);

    if (meter.asset_root[0] == '\0') {
        /* Asset-less fallbacks: rotation bar, text digits and dots. The
         * vendor frames stay vendored under assets/ for phase 2. */
        fb_needle = lv_obj_create(img_bg);
        lv_obj_set_size(fb_needle, meter_fit(8), meter_fit(200));
        lv_obj_set_pos(fb_needle, width / 2 - meter_fit(4), height / 2 - meter_fit(190));
        lv_obj_set_style_bg_color(fb_needle, lv_color_hex(0xff3040), 0);
        lv_obj_set_style_bg_opa(fb_needle, LV_OPA_COVER, 0);
        lv_obj_set_style_border_width(fb_needle, 0, 0);
        lv_obj_set_style_radius(fb_needle, meter_fit(4), 0);
        lv_obj_set_style_transform_pivot_x(fb_needle, meter_fit(4), 0);
        lv_obj_set_style_transform_pivot_y(fb_needle, meter_fit(190), 0);
        lv_obj_set_scrollable(fb_needle, false);

        fb_speed = meter_label(img_bg, width / 2 - meter_fit(20),
                               height / 2 - meter_fit(40), "0");
        fb_time = meter_label(img_bg, width - meter_fit(220), meter_fit(100), "02:00");
        fb_trip = meter_label(img_bg, meter_fit(90), height - meter_fit(60), "0098");
        fb_gear = meter_label(img_bg, width / 2 - meter_fit(20),
                              height - meter_fit(80), "d1");
        fb_status = meter_label(img_bg, meter_fit(90), height - meter_fit(100), "mode 0");
    }

    bg_fps = meter_label(img_bg, width - meter_fit(120), meter_fit(10), "");
    if (meter.fps == NULL) {
        lv_obj_set_hidden(bg_fps, true);
    }

    img_point = NULL; /* Needle strip frames bind in phase 2. */

    meter_timers[0] = lv_timer_create(point_callback, 10, 0);
    meter_timers[1] = lv_timer_create(fps_callback, 1000, 0);
    meter_timers[2] = lv_timer_create(time_callback, 1000 * 60, 0);
    meter_timers[3] = lv_timer_create(trip_callback, 1000 * 5, 0);
    meter_timers[4] = lv_timer_create(other_callback, 600, 0);
    meter_timers[5] = lv_timer_create(signal_callback, 500, 0);
    meter_timers[6] = lv_timer_create(gear_callback, 1000 * 3, 0);
    speed_timer = lv_timer_create(speed_callback, 100, 0);
    meter_timers[7] = speed_timer;
    for (i = 0; i < 8; i++) {
        if (meter_timers[i] == NULL) {
            meter_ui_destroy();
            return;
        }
    }

    meter.created = true;
    return;
}

void meter_ui_destroy(void)
{
    size_t i;
    for (i = 0; i < 8; i++) {
        if (meter_timers[i] != NULL) {
            lv_timer_delete(meter_timers[i]);
            meter_timers[i] = NULL;
        }
    }
    speed_timer = NULL;
    if (img_bg != NULL) {
        lv_obj_delete(img_bg);
    }
    img_bg = img_circle = img_point = img_engine = img_esp = img_oil_less = NULL;
    img_battery = img_battery_num = img_battery_hl = NULL;
    img_oil_fe = img_oil_num = img_oil = NULL;
    img_water_temp = img_water_num = NULL;
    img_gear = img_trip = img_trip_0 = img_trip_1 = NULL;
    img_trip_2 = img_trip_3 = img_trip_km = NULL;
    img_speed_num = img_speed_km = NULL;
    img_time = img_time_0 = img_time_1 = NULL;
    img_time_2 = img_time_3 = img_time_dot = NULL;
    bg_fps = fb_needle = fb_speed = fb_time = NULL;
    fb_trip = fb_gear = fb_status = NULL;
    memset(&meter, 0, sizeof(meter));
}

uint32_t meter_ui_timer_fires(void)
{
    return meter.fires;
}

int meter_ui_get_speed_step(void)
{
    if (!meter.created) {
        return -1;
    }
    return meter.speed_step;
}

int32_t meter_ui_get_needle_angle(void)
{
    return meter.needle_angle;
}
