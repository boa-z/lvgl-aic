/* SPDX-License-Identifier: Apache-2.0 */
#include "lvgl.h"
#if LV_USE_LOTTIE
/* Native lv_lottie_set_draw_buf accepts ARGB8888 then flags it premultiplied.
 * LVGL 9.6 software blending selects by color format, not by that flag alone.
 * Normalize before canvas/image header caching; both GE and SW then see the
 * correct pixel representation. No buffer allocation or ownership changes. */
static void lv_aic_lottie_set_canvas_draw_buf(lv_obj_t *obj, lv_draw_buf_t *buf)
{
    buf->header.cf = LV_COLOR_FORMAT_ARGB8888_PREMULTIPLIED;
    lv_canvas_set_draw_buf(obj, buf);
}
#define lv_canvas_set_draw_buf lv_aic_lottie_set_canvas_draw_buf
#include "../../lvgl/src/widgets/lottie/lv_lottie.c"
#undef lv_canvas_set_draw_buf
/* A caller can delete/finish a native lv_anim_t without deleting the widget. */
bool lv_aic_lottie_animation_is_active(lv_obj_t *obj)
{
    return lv_anim_get(obj,anim_exec_cb)==((lv_lottie_t *)obj)->anim;
}
#endif
