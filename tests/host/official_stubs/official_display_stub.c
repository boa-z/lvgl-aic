/* SPDX-License-Identifier: Apache-2.0 */
/* Host stand-ins for target services the official demos link against. */
#include "lvgl.h"

#include <stdbool.h>
#include <stdint.h>

/* fbdev_draw_fps() compat source (port/lv_aic_display.c on target). */
int lv_aic_display_fps(void)
{
    return 60;
}

/* demo_hub's audio/video apps use the media player widget (component
 * widgets/ on target, AIC_LVGL_USE_PLAYER). The host contract only runs the
 * launcher, so an inert object is enough. */
lv_obj_t *lv_aic_player_create(lv_obj_t *parent)
{
    return lv_obj_create(parent);
}

lv_result_t lv_aic_player_set_src(lv_obj_t *obj, const char *uri)
{
    (void)obj;
    (void)uri;
    return LV_RESULT_INVALID;
}

lv_result_t lv_aic_player_set_auto_restart(lv_obj_t *obj, bool enabled)
{
    (void)obj;
    (void)enabled;
    return LV_RESULT_OK;
}

lv_result_t lv_aic_player_set_width(lv_obj_t *obj, uint32_t width)
{
    (void)obj;
    (void)width;
    return LV_RESULT_OK;
}

lv_result_t lv_aic_player_set_height(lv_obj_t *obj, uint32_t height)
{
    (void)obj;
    (void)height;
    return LV_RESULT_OK;
}

void lv_aic_player_set_cmd(lv_obj_t *obj, int command, void *data)
{
    (void)obj;
    (void)command;
    (void)data;
}
