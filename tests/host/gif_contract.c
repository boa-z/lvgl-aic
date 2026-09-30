/* SPDX-License-Identifier: Apache-2.0 */
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <lvgl/lvgl.h>
#include "lv_aic_gif_test.h"

static const char *gif_path;
static uint8_t pixels[320 * 240 * 2];
static unsigned opens, closes, flushes;
static lv_point_t pointer;
static bool pressed;
static void read_pointer(lv_indev_t *indev, lv_indev_data_t *data)
{
    (void)indev;
    data->point = pointer;
    data->state = pressed ? LV_INDEV_STATE_PRESSED : LV_INDEV_STATE_RELEASED;
}

static void *open_cb(lv_fs_drv_t *drv, const char *path, lv_fs_mode_t mode)
{
    (void)drv;
    if (mode != LV_FS_MODE_RD || strcmp(path, "/bulb.gif")) return NULL;
    FILE *file = fopen(gif_path, "rb");
    if (file) opens++;
    return file;
}
static lv_fs_res_t close_cb(lv_fs_drv_t *drv, void *file)
{
    (void)drv;
    closes++;
    return fclose(file) ? LV_FS_RES_FS_ERR : LV_FS_RES_OK;
}
static lv_fs_res_t read_cb(lv_fs_drv_t *drv, void *file, void *buf, uint32_t n, uint32_t *read)
{
    (void)drv;
    *read = (uint32_t)fread(buf, 1, n, file);
    return ferror(file) ? LV_FS_RES_FS_ERR : LV_FS_RES_OK;
}
static lv_fs_res_t seek_cb(lv_fs_drv_t *drv, void *file, uint32_t pos, lv_fs_whence_t whence)
{
    (void)drv;
    int origin = whence == LV_FS_SEEK_SET ? SEEK_SET : whence == LV_FS_SEEK_CUR ? SEEK_CUR : SEEK_END;
    return fseek(file, (long)pos, origin) ? LV_FS_RES_FS_ERR : LV_FS_RES_OK;
}
static lv_fs_res_t tell_cb(lv_fs_drv_t *drv, void *file, uint32_t *pos)
{
    (void)drv;
    long value = ftell(file);
    if (value < 0) return LV_FS_RES_FS_ERR;
    *pos = (uint32_t)value;
    return LV_FS_RES_OK;
}
static void flush(lv_display_t *display, const lv_area_t *area, uint8_t *data)
{
    (void)area; (void)data;
    flushes++;
    lv_display_flush_ready(display);
}
static uint32_t frame_hash(void)
{
    uint32_t hash = 2166136261U;
    for (size_t i = 0; i < sizeof(pixels); i++) hash = (hash ^ pixels[i]) * 16777619U;
    return hash;
}
static void step(void)
{
    lv_tick_inc(50);
    lv_timer_handler();
}
static void check_animation(lv_obj_t *gif)
{
    assert(lv_gif_is_loaded(gif));
    assert(lv_gif_get_frame_count(gif) > 1);
    lv_gif_set_loop_count(gif, 0);
    lv_refr_now(NULL);
    uint32_t first = frame_hash();
    bool changed = false;
    int32_t initial = lv_gif_get_current_frame_index(gif);
    bool advanced = false;
    for (unsigned i = 0; i < 40; i++) {
        step();
        changed |= frame_hash() != first;
        advanced |= lv_gif_get_current_frame_index(gif) != initial;
    }
    assert(changed && advanced);
    lv_gif_pause(gif);
    lv_refr_now(NULL);
    uint32_t paused_hash = frame_hash();
    int32_t paused_frame = lv_gif_get_current_frame_index(gif);
    for (unsigned i = 0; i < 10; i++) step();
    assert(lv_gif_get_current_frame_index(gif) == paused_frame);
    assert(frame_hash() == paused_hash);
    lv_gif_resume(gif);
    advanced = false;
    for (unsigned i = 0; i < 40; i++) {
        step();
        advanced |= lv_gif_get_current_frame_index(gif) != paused_frame;
    }
    assert(advanced);
    lv_gif_restart(gif);
    assert(lv_gif_get_current_frame_index(gif) == 0);
}
int main(int argc, char **argv)
{
    assert(argc == 2);
    gif_path = argv[1]; /* Native host path, never an LVGL drive-prefixed path. */
    FILE *file = fopen(gif_path, "rb");
    assert(file);
    assert(fseek(file, 0, SEEK_END) == 0);
    long length = ftell(file);
    assert(length > 0);
    rewind(file);
    uint8_t *raw = malloc((size_t)length);
    assert(raw && fread(raw, 1, (size_t)length, file) == (size_t)length);
    fclose(file);
    lv_init();
    lv_fs_drv_t drv;
    lv_fs_drv_init(&drv);
    drv.letter = 'G';
    drv.open_cb = open_cb; drv.close_cb = close_cb; drv.read_cb = read_cb;
    drv.seek_cb = seek_cb; drv.tell_cb = tell_cb;
    lv_fs_drv_register(&drv);
    assert(lv_fs_get_drv('G'));
    lv_display_t *display = lv_display_create(320, 240);
    assert(display);
    lv_display_set_color_format(display, LV_COLOR_FORMAT_RGB565);
    lv_display_set_buffers(display, pixels, NULL, sizeof(pixels), LV_DISPLAY_RENDER_MODE_FULL);
    lv_display_set_flush_cb(display, flush);
    lv_indev_t *indev = lv_indev_create();
    assert(indev);
    lv_indev_set_type(indev, LV_INDEV_TYPE_POINTER);
    lv_indev_set_read_cb(indev, read_pointer);
    lv_indev_set_display(indev, display);
    uint16_t w = 0, h = 0;
    assert(!lv_gif_get_size("G:/missing.gif", &w, &h));
    assert(lv_gif_get_size("G:/bulb.gif", &w, &h));
    assert(w > 0 && h > 0 && w <= 320 && h <= 240 && opens == closes);
    lv_image_dsc_t source = {0};
    source.header.magic = LV_IMAGE_HEADER_MAGIC;
    source.header.cf = LV_COLOR_FORMAT_RAW;
    source.header.w = w; source.header.h = h;
    source.data = raw; source.data_size = (uint32_t)length;
    for (unsigned cycle = 0; cycle < 20; cycle++) {
        lv_obj_t *gif = lv_gif_create(lv_screen_active());
        assert(gif);
        lv_gif_set_color_format(gif, cycle & 1 ? LV_COLOR_FORMAT_RGB565 : LV_COLOR_FORMAT_ARGB8888);
        lv_gif_set_src(gif, "G:/bulb.gif");
        assert(opens == closes + 1);
        check_animation(gif);
        lv_gif_set_src(gif, &source);
        assert(opens == closes);
        check_animation(gif);
        lv_obj_delete(gif);
        step();
        assert(lv_obj_get_child_count(lv_screen_active()) == 0);
        assert(opens == closes);
    }
    lv_obj_t *invalid = lv_gif_create(lv_screen_active());
    lv_gif_set_src(invalid, "G:/missing.gif");
    assert(!lv_gif_is_loaded(invalid));
    lv_obj_delete(invalid);
    static const uint8_t invalid_data[32] = {0};
    source.data = invalid_data; source.data_size = sizeof(invalid_data);
    invalid = lv_gif_create(lv_screen_active());
    lv_gif_set_src(invalid, &source);
    assert(!lv_gif_is_loaded(invalid));
    lv_obj_delete(invalid);
    assert(flushes > 0 && opens == closes);
    uint32_t children = lv_obj_get_child_count(lv_layer_top());
    assert(lv_aic_gif_test_show("G:/missing.gif") == LV_AIC_ERR_UNSUPPORTED);
    assert(lv_obj_get_child_count(lv_layer_top()) == children);
    for (unsigned cycle = 0; cycle < 20; cycle++) {
        assert(lv_aic_gif_test_show("G:/bulb.gif") == LV_AIC_OK);
        assert(lv_aic_gif_test_show("G:/bulb.gif") == LV_AIC_ERR_INVALID_STATE);
        assert(lv_obj_get_child_count(lv_layer_top()) == children + 1);
        for (unsigned i = 0; i < 10; i++) step();
        lv_obj_update_layout(lv_layer_top());
        lv_obj_t *panel = lv_obj_get_child(lv_layer_top(), children);
        lv_obj_t *close = lv_obj_get_child(panel, -1);
        assert(lv_obj_check_type(close, &lv_button_class));
        lv_area_t area;
        lv_obj_get_coords(close, &area);
        assert(area.x1 >= 0 && area.x2 < 320 && area.y1 >= 0 && area.y2 < 240);
        pointer.x = (area.x1 + area.x2) / 2;
        pointer.y = (area.y1 + area.y2) / 2;
        pressed = true; lv_tick_inc(40); lv_indev_read(indev);
        pressed = false; lv_tick_inc(40); lv_indev_read(indev);
        assert(lv_obj_get_child_count(lv_layer_top()) == children);
        lv_aic_gif_test_delete();
        step();
        assert(lv_obj_get_child_count(lv_layer_top()) == children);
        assert(opens == closes);
    }
    lv_indev_delete(indev);
    lv_display_delete(display);
    lv_deinit();
    free(raw);
    puts("PASS native GIF: FILE/RAW, RGB565/ARGB8888, changing pixels, pause/resume/restart, 20 lifecycles, invalid inputs, balanced files");
    return 0;
}
