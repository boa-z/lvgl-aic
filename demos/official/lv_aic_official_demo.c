/* SPDX-License-Identifier: Apache-2.0 */
#include "demos/official/lv_aic_official_demo.h"
#include "lvgl_private.h" /* lv_display_t screen list, lv_image_cache_drop */
#if defined(AIC_LVGL_USE_MPP_DEC) && AIC_LVGL_USE_MPP_DEC
#include "lvgl_aic.h"     /* lv_aic_mpp_cache_drop */
#endif

#include <string.h>

/* compat/lv_aic_v8_compat.h maps the v8 lv_img_cache_invalidate_src() here. */
void lv_aic_official_image_drop(const void *src)
{
    if (src == NULL) {
        return;
    }
#if defined(AIC_LVGL_USE_MPP_DEC) && AIC_LVGL_USE_MPP_DEC
    /* Drops the decoded entry keyed by this source and LVGL's header cache. */
    lv_aic_mpp_cache_drop(src);
#endif
    lv_image_cache_drop(src);
}

/* Each official demo's ui_init() is renamed at build time (per-demo
 * -Dui_init=...), so several can link into one image. Kconfig bools reach
 * C as empty macros: test definedness only. */
#ifdef AIC_LVGL_OFFICIAL_DEMO_METER
void lv_aic_official_meter_ui_init(void);
#endif
#ifdef AIC_LVGL_OFFICIAL_DEMO_DASHBOARD
void lv_aic_official_dashboard_ui_init(void);
#endif
#ifdef AIC_LVGL_OFFICIAL_DEMO_SLIDE
void lv_aic_official_slide_ui_init(void);
#endif
#ifdef AIC_LVGL_OFFICIAL_DEMO_MULTI_LANG
void lv_aic_official_multi_lang_ui_init(void);
#endif
#ifdef AIC_LVGL_OFFICIAL_DEMO_DEMO_HUB
void lv_aic_official_demo_hub_ui_init(void);
#endif
#ifdef AIC_LVGL_OFFICIAL_DEMO_IMAGE
void lv_aic_official_image_ui_init(void);
#endif

static const lv_aic_official_demo_t demos[] = {
#ifdef AIC_LVGL_OFFICIAL_DEMO_METER
    {"meter", "aic_demo/meter_demo", lv_aic_official_meter_ui_init},
#endif
#ifdef AIC_LVGL_OFFICIAL_DEMO_DASHBOARD
    {"dashboard", "aic_demo/dashboard_demo", lv_aic_official_dashboard_ui_init},
#endif
#ifdef AIC_LVGL_OFFICIAL_DEMO_SLIDE
    {"slide", "aic_demo/slide_demo", lv_aic_official_slide_ui_init},
#endif
#ifdef AIC_LVGL_OFFICIAL_DEMO_MULTI_LANG
    {"multi_lang", "aic_demo/multi_lang_demo", lv_aic_official_multi_lang_ui_init},
#endif
#ifdef AIC_LVGL_OFFICIAL_DEMO_DEMO_HUB
    {"demo_hub", "aic_demo/demo_hub", lv_aic_official_demo_hub_ui_init},
#endif
#ifdef AIC_LVGL_OFFICIAL_DEMO_IMAGE
    {"image", "aic_demo/image_demo", lv_aic_official_image_ui_init},
#endif
    {NULL, NULL, NULL},
};

#define HIDDEN_MAX 16

static struct {
    const lv_aic_official_demo_t *active;
    /* screen: created by the runner. Demos may create and load screens of
     * their own (slide_demo, multi_lang_demo's white/dark pair): close deletes
     * every screen that was not on the display before show. */
    lv_obj_t *screen, *previous;
    lv_obj_t **screens_before;
    uint32_t screens_before_count;
    lv_timer_t **timers;
    uint32_t timer_count;
    lv_obj_t *hidden[HIDDEN_MAX];
    uint32_t hidden_count;
} run;

size_t lv_aic_official_demo_count(void)
{
    return sizeof(demos) / sizeof(demos[0]) - 1U;
}

const lv_aic_official_demo_t *lv_aic_official_demo_get(size_t index)
{
    return index < lv_aic_official_demo_count() ? &demos[index] : NULL;
}

const lv_aic_official_demo_t *lv_aic_official_demo_find(const char *name)
{
    for (size_t i = 0; name != NULL && i < lv_aic_official_demo_count(); i++) {
        if (strcmp(demos[i].name, name) == 0) {
            return &demos[i];
        }
    }
    return NULL;
}

static uint32_t timer_total(void)
{
    uint32_t n = 0;
    for (lv_timer_t *t = lv_timer_get_next(NULL); t != NULL; t = lv_timer_get_next(t)) {
        n++;
    }
    return n;
}

static bool timer_in(lv_timer_t *const *list, uint32_t count, const lv_timer_t *timer)
{
    for (uint32_t i = 0; i < count; i++) {
        if (list[i] == timer) {
            return true;
        }
    }
    return false;
}

/* The host UI keeps navigation on lv_layer_top(), which covers every screen. */
static void overlays_hide(void)
{
    lv_obj_t *top = lv_layer_top();
    uint32_t count = lv_obj_get_child_count(top);
    run.hidden_count = 0;
    for (uint32_t i = 0; i < count && run.hidden_count < HIDDEN_MAX; i++) {
        lv_obj_t *child = lv_obj_get_child(top, (int32_t)i);
        if (!lv_obj_is_hidden(child)) {
            lv_obj_set_hidden(child, true);
            run.hidden[run.hidden_count++] = child;
        }
    }
}

static void overlays_restore(void)
{
    lv_obj_t *top = lv_layer_top();
    for (uint32_t i = 0; i < run.hidden_count; i++) {
        /* Only restore objects that still exist as top-layer children. */
        if (lv_obj_get_parent(run.hidden[i]) == top) {
            lv_obj_set_hidden(run.hidden[i], false);
        }
    }
    run.hidden_count = 0;
}

int lv_aic_official_demo_show(const char *name)
{
    const lv_aic_official_demo_t *demo = lv_aic_official_demo_find(name);
    lv_timer_t **before;
    uint32_t before_count, after_count, owned = 0;

    if (demo == NULL) {
        return -1;
    }
    lv_aic_official_demo_close();

    before_count = timer_total();
    before = lv_malloc(sizeof(*before) * (before_count + 1U));
    if (before == NULL) {
        return -2;
    }
    {
        uint32_t i = 0;
        for (lv_timer_t *t = lv_timer_get_next(NULL); t != NULL && i < before_count;
             t = lv_timer_get_next(t)) {
            before[i++] = t;
        }
        before_count = i;
    }

    {
        lv_display_t *display = lv_display_get_default();
        run.screens_before_count = display->screen_cnt;
        run.screens_before = lv_malloc(sizeof(lv_obj_t *) * (display->screen_cnt + 1U));
        if (run.screens_before == NULL) {
            lv_free(before);
            return -2;
        }
        memcpy(run.screens_before, display->screens, sizeof(lv_obj_t *) * display->screen_cnt);
    }
    run.previous = lv_screen_active();
    run.screen = lv_obj_create(NULL);
    if (run.screen == NULL) {
        lv_free(before);
        lv_free(run.screens_before);
        run.screens_before = NULL;
        return -2;
    }
    lv_screen_load(run.screen);
    overlays_hide();
    demo->entry();

    after_count = timer_total();
    run.timers = lv_malloc(sizeof(*run.timers) * (after_count + 1U));
    if (run.timers != NULL) {
        for (lv_timer_t *t = lv_timer_get_next(NULL); t != NULL && owned < after_count;
             t = lv_timer_get_next(t)) {
            if (!timer_in(before, before_count, t)) {
                run.timers[owned++] = t;
            }
        }
    }
    run.timer_count = owned;
    lv_free(before);
    run.active = demo;
    return run.timers != NULL ? 0 : -2;
}

/* Deleting an object deletes its animations and calls their completed/deleted
 * callbacks. Demos chain animations from those (image_demo's deleted_cb draws
 * the next line and starts animations on the other canvas, which is deleted
 * next), so a demo being torn down must not get them. */
static lv_obj_tree_walk_res_t silence_animations(lv_obj_t *obj, void *user_data)
{
    lv_ll_t *anims = &LV_GLOBAL_DEFAULT()->anim_state.anim_ll;

    LV_UNUSED(user_data);
    for (lv_anim_t *a = lv_ll_get_head(anims); a != NULL; a = lv_ll_get_next(anims, a)) {
        if (a->var == obj) {
            a->completed_cb = NULL;
            a->deleted_cb = NULL;
        }
    }
    return LV_OBJ_TREE_WALK_NEXT;
}

void lv_aic_official_demo_close(void)
{
    if (run.screen == NULL) {
        return;
    }
    /* Leave through a normal screen load first, so the demo's unload handlers
     * run while its objects and timers are still alive. */
    if (run.previous != NULL) {
        lv_screen_load(run.previous);
    }
    {
        /* Collect first: deleting a screen compacts the display's list. */
        lv_display_t *display = lv_display_get_default();
        uint32_t count = display->screen_cnt, added = 0;
        lv_obj_t **doomed = lv_malloc(sizeof(lv_obj_t *) * (count + 1U));
        for (uint32_t i = 0; doomed != NULL && i < count; i++) {
            lv_obj_t *screen = display->screens[i];
            bool existed = false;
            for (uint32_t j = 0; j < run.screens_before_count; j++) {
                existed = existed || run.screens_before[j] == screen;
            }
            if (!existed && screen != run.previous) {
                doomed[added++] = screen;
            }
        }
        for (uint32_t i = 0; i < added; i++) {
            lv_obj_tree_walk(doomed[i], silence_animations, NULL);
        }
        for (uint32_t i = 0; i < added; i++) {
            lv_obj_delete(doomed[i]);
        }
        if (doomed == NULL) {
            /* Out of memory: at least our own. */
            lv_obj_tree_walk(run.screen, silence_animations, NULL);
            lv_obj_delete(run.screen);
        }
        lv_free(doomed);
    }
    /* Timers last: the demo's own SCREEN_UNLOAD/DELETE handlers above may
     * delete some of them (demo_hub's navigator does), so delete only the ones
     * still registered. No timer runs in between: this is all on the LVGL
     * thread. */
    for (uint32_t i = 0; i < run.timer_count; i++) {
        for (lv_timer_t *t = lv_timer_get_next(NULL); t != NULL; t = lv_timer_get_next(t)) {
            if (t == run.timers[i]) {
                lv_timer_delete(t);
                break;
            }
        }
    }
    lv_free(run.timers);
    run.timers = NULL;
    run.timer_count = 0;
    lv_free(run.screens_before);
    run.screens_before = NULL;
    run.screens_before_count = 0;
    overlays_restore();
    run.screen = run.previous = NULL;
    run.active = NULL;
}

const char *lv_aic_official_demo_active(void)
{
    return run.active != NULL ? run.active->name : NULL;
}

uint32_t lv_aic_official_demo_timer_count(void)
{
    return run.timer_count;
}
