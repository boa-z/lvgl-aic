/* SPDX-License-Identifier: Apache-2.0 */
#include "lv_aic_gif_test.h"
#if LV_USE_GIF
static lv_obj_t *overlay;
static lv_obj_t *animation;

void lv_aic_gif_test_delete(void)
{
    if (overlay) lv_obj_delete(overlay);
    overlay = animation = NULL;
}
static void close_panel(lv_event_t *event)
{
    (void)event;
    lv_aic_gif_test_delete();
}
int lv_aic_gif_test_show(const char *path)
{
    if (!path || !lv_display_get_default()) return LV_AIC_ERR_INVALID_STATE;
    if (overlay) return LV_AIC_ERR_INVALID_STATE;
    overlay = lv_obj_create(lv_layer_top());
    if (!overlay) return LV_AIC_ERR_NO_MEMORY;
    lv_obj_remove_style_all(overlay);
    lv_obj_set_size(overlay, lv_display_get_horizontal_resolution(NULL),
                    lv_display_get_vertical_resolution(NULL));
    lv_obj_set_style_bg_color(overlay, lv_color_hex(0x182430), 0);
    lv_obj_set_style_bg_opa(overlay, LV_OPA_COVER, 0);
    lv_obj_set_scrollable(overlay, false);
    lv_obj_set_clickable(overlay, true);
    animation = lv_gif_create(overlay);
    if (!animation) goto no_memory;
    lv_gif_set_src(animation, path);
    if (!lv_gif_is_loaded(animation)) {
        lv_aic_gif_test_delete();
        return LV_AIC_ERR_UNSUPPORTED;
    }
    lv_gif_set_loop_count(animation, 0);
    lv_obj_center(animation);
    lv_obj_t *title = lv_label_create(overlay);
    if (!title) goto no_memory;
    lv_label_set_text(title, "Native GIF");
    lv_obj_set_style_text_color(title, lv_color_white(), 0);
    lv_obj_set_pos(title, 16, 20);
    lv_obj_t *close = lv_button_create(overlay);
    if (!close) goto no_memory;
    lv_obj_set_size(close, 88, 44);
    lv_obj_align(close, LV_ALIGN_TOP_RIGHT, -16, 10);
    lv_obj_add_event_cb(close, close_panel, LV_EVENT_CLICKED, NULL);
    title = lv_label_create(close);
    if (!title) goto no_memory;
    lv_label_set_text(title, "Close");
    lv_obj_center(title);
    return LV_AIC_OK;
no_memory:
    lv_aic_gif_test_delete();
    return LV_AIC_ERR_NO_MEMORY;
}

#if AIC_LVGL_BSP_RTTHREAD
#include <rtthread.h>
#include <rthw.h>
#include <finsh.h>
#include <string.h>
enum gif_command { NONE, SHOW, PAUSE, RESUME, RESTART, STATUS, CLOSE };
static volatile enum gif_command pending;
static volatile bool ready;

void lv_aic_gif_test_poll(void)
{
    rt_base_t level = rt_hw_interrupt_disable();
    ready = true;
    enum gif_command command = pending;
    pending = NONE;
    rt_hw_interrupt_enable(level);
    if (command == NONE) return;
    if (command == SHOW) {
        int result = lv_aic_gif_test_show("L:/data/mpp_test/bulb.gif");
        rt_kprintf("GIF show result=%d (0=OK)\n", result);
    } else if (command == CLOSE) {
        lv_aic_gif_test_delete();
        rt_kprintf("GIF closed\n");
    } else if (!animation) {
        rt_kprintf("GIF not open; run lv_aic_gif show\n");
    } else {
        if (command == PAUSE) lv_gif_pause(animation);
        if (command == RESUME) lv_gif_resume(animation);
        if (command == RESTART) {
            lv_gif_restart(animation);
            lv_gif_set_loop_count(animation, 0);
        }
        rt_kprintf("GIF command=%d loaded=%d frame=%d frames=%d\n", command,
                   lv_gif_is_loaded(animation), lv_gif_get_current_frame_index(animation),
                   lv_gif_get_frame_count(animation));
    }
}
void lv_aic_gif_test_deinit(void)
{
    rt_base_t level = rt_hw_interrupt_disable();
    ready = false;
    pending = NONE;
    rt_hw_interrupt_enable(level);
    lv_aic_gif_test_delete();
}
static void lv_aic_gif(int argc, char **argv)
{
    static const char *names[] = {"show", "pause", "resume", "restart", "status", "close"};
    enum gif_command command = NONE;
    if (argc == 2) {
        for (unsigned i = 0; i < sizeof(names) / sizeof(names[0]); i++)
            if (!strcmp(argv[1], names[i])) command = (enum gif_command)(i + 1);
    }
    if (command == NONE) {
        rt_kprintf("lv_aic_gif show|pause|resume|restart|status|close\n");
        return;
    }
    /* Single-core board: publish one command; all LVGL work stays in UI thread. */
    rt_base_t level = rt_hw_interrupt_disable();
    bool accepted = ready && pending == NONE;
    if (accepted) pending = command;
    rt_hw_interrupt_enable(level);
    rt_kprintf(accepted ? "GIF request queued\n" : "GIF busy or UI not ready; retry\n");
}
MSH_CMD_EXPORT(lv_aic_gif, Native GIF acceptance commands);
#endif
#endif
