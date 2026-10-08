/* SPDX-License-Identifier: Apache-2.0 */
/* Shell gate for the official SDK demos (demos/official): the shell only
 * queues a request; lv_aic_demo_test_poll() acts on the LVGL owner thread. */
#include "lv_aic_demo_test.h"
#if defined(AIC_LVGL_OFFICIAL_DEMOS) && AIC_LVGL_OFFICIAL_DEMOS
#include "demos/official/lv_aic_official_demo.h"
#include "../../port/lv_aic_display.h"
#if AIC_LVGL_USE_GE2D
#include "../../draw/ge2d/lv_draw_aic_ge2d.h"
#endif

#if AIC_LVGL_BSP_RTTHREAD
#include <rtthread.h>
#include <rthw.h>
#include <finsh.h>
#include <stdio.h>
#include <string.h>

enum demo_command { DEMO_NONE, DEMO_SHOW, DEMO_CLOSE, DEMO_STATUS };
static volatile enum demo_command demo_pending;
static volatile bool demo_ready;
static char demo_name[24];
#if AIC_LVGL_USE_MPP_DEC
/* Official demos redraw over full-screen JPEG backgrounds (1024x600 RGB is
 * ~1.8 MB decoded) that must stay cached beside their animated parts; the
 * default 512 KiB budget cannot hold one. Raised while a demo runs. */
#define DEMO_CACHE_BYTES (4U * 1024U * 1024U)
static uint32_t demo_cache_saved;
static bool demo_cache_raised;
#endif

static void demo_cache_restore(void)
{
#if AIC_LVGL_USE_MPP_DEC
    if (demo_cache_raised) {
        lv_aic_mpp_cache_set_limit(demo_cache_saved);
        demo_cache_raised = false;
    }
#endif
}

static void demo_status(void)
{
    const char *active = lv_aic_official_demo_active();
    rt_kprintf("demo active=%s timers=%u fps=%d res=%dx%d\n", active ? active : "-",
               (unsigned)lv_aic_official_demo_timer_count(), lv_aic_display_fps(),
               (int)lv_display_get_horizontal_resolution(lv_display_get_default()),
               (int)lv_display_get_vertical_resolution(lv_display_get_default()));
#if AIC_LVGL_USE_MPP_DEC
    {
        const lv_aic_mpp_cache_stats_t *st = lv_aic_mpp_cache_stats();
        rt_kprintf("mpp cache hits=%u misses=%u entries=%u bytes=%u limit=%u\n",
                   (unsigned)st->hits, (unsigned)st->misses, (unsigned)st->entries,
                   (unsigned)st->bytes, (unsigned)st->limit_bytes);
    }
#endif
#if AIC_LVGL_USE_GE2D
    {
        /* Interval counters, reset after each report. */
        const lv_draw_aic_ge2d_stats_t *ge = lv_draw_aic_ge2d_stats();
        rt_kprintf("ge2d img acc=%u sw=%u scaled=%u fill acc=%u declined=%u err=%u\n",
                   (unsigned)ge->image_accepted, (unsigned)ge->image_sw_fallback,
                   (unsigned)ge->scaled_image_engine, (unsigned)ge->fill_accepted,
                   (unsigned)ge->fallback, (unsigned)ge->errors);
        lv_draw_aic_ge2d_stats_reset();
    }
#endif
}

void lv_aic_demo_test_poll(void)
{
    enum demo_command cmd;
    char name[sizeof(demo_name)];
    rt_base_t level = rt_hw_interrupt_disable();
    demo_ready = true;
    cmd = demo_pending;
    demo_pending = DEMO_NONE;
    memcpy(name, demo_name, sizeof(name));
    rt_hw_interrupt_enable(level);

    if (cmd == DEMO_STATUS) {
        demo_status();
    } else if (cmd == DEMO_CLOSE) {
        lv_aic_official_demo_close();
        demo_cache_restore();
        rt_kprintf("demo closed\n");
    } else if (cmd == DEMO_SHOW) {
        int result;
#if AIC_LVGL_USE_MPP_DEC
        if (!demo_cache_raised) {
            demo_cache_saved = lv_aic_mpp_cache_stats()->limit_bytes;
            demo_cache_raised = true;
            if (demo_cache_saved < DEMO_CACHE_BYTES) {
                lv_aic_mpp_cache_set_limit(DEMO_CACHE_BYTES);
            }
        }
#endif
        result = lv_aic_official_demo_show(name);
        if (result != 0) {
            demo_cache_restore();
        }
        rt_kprintf("demo %s: %s (timers=%u)\n", name,
                   result == 0 ? "running" : result == -1 ? "unknown" : "out of memory",
                   (unsigned)lv_aic_official_demo_timer_count());
    }
}

void lv_aic_demo_test_deinit(void)
{
    rt_base_t level = rt_hw_interrupt_disable();
    demo_ready = false;
    demo_pending = DEMO_NONE;
    rt_hw_interrupt_enable(level);
    lv_aic_official_demo_close();
    demo_cache_restore();
}

static void lv_aic_demo(int argc, char **argv)
{
    enum demo_command cmd = DEMO_NONE;
    if (argc >= 2 && !strcmp(argv[1], "list")) {
        for (size_t i = 0; i < lv_aic_official_demo_count(); i++) {
            const lv_aic_official_demo_t *d = lv_aic_official_demo_get(i);
            rt_kprintf("  %-12s SDK %s\n", d->name, d->sdk);
        }
        return;
    }
    if (argc >= 3 && !strcmp(argv[1], "show")) {
        cmd = DEMO_SHOW;
    } else if (argc >= 2 && !strcmp(argv[1], "close")) {
        cmd = DEMO_CLOSE;
    } else if (argc >= 2 && !strcmp(argv[1], "status")) {
        cmd = DEMO_STATUS;
    }
    if (cmd == DEMO_NONE) {
        rt_kprintf("Usage: lv_aic_demo list | show <name> | close | status\n");
        return;
    }
    if (cmd == DEMO_SHOW && lv_aic_official_demo_find(argv[2]) == NULL) {
        rt_kprintf("unknown demo '%s'; see lv_aic_demo list\n", argv[2]);
        return;
    }
    {
        char name[sizeof(demo_name)] = {0};
        if (cmd == DEMO_SHOW) {
            snprintf(name, sizeof(name), "%s", argv[2]);
        }
        rt_base_t level = rt_hw_interrupt_disable();
        bool ok = demo_ready && demo_pending == DEMO_NONE;
        if (ok) {
            demo_pending = cmd;
            memcpy(demo_name, name, sizeof(demo_name));
        }
        rt_hw_interrupt_enable(level);
        rt_kprintf(ok ? "demo request queued\n" : "demo busy or UI not ready; retry\n");
    }
}
MSH_CMD_EXPORT(lv_aic_demo, Official SDK demos: list/show/close/status);
#else
void lv_aic_demo_test_poll(void) {}
void lv_aic_demo_test_deinit(void) {}
#endif
#endif
