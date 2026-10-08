/* SPDX-License-Identifier: Apache-2.0 */
/* Shell probe for the touch path: `lv_aic_touch` prints the indev counters,
 * `lv_aic_touch watch [seconds]` samples them while a person touches the
 * panel and reports the coordinate extent LVGL received, so corner coverage
 * and axis mapping can be judged from numbers. Read-only: it only calls the
 * diagnostics getter, which takes the driver's own lock. */
#include "lvgl_aic.h"

#if AIC_LVGL_USE_TOUCH && AIC_LVGL_BSP_RTTHREAD
#include "../../port/lv_aic_indev.h"

#include <rtthread.h>
#include <finsh.h>
#include <stdlib.h>
#include <string.h>

#define WATCH_DEFAULT_S 20
#define WATCH_MAX_S 120
#define WATCH_PERIOD_MS 10

static void print_snapshot(const char *tag, const lv_aic_touch_diagnostics_t *d)
{
    rt_kprintf("touch %s: irqs=%u reads=%u events=%u empty=%u invalid=%u deliveries=%u "
               "recovered=%u range=%dx%d last=(%d,%d) %s\n",
               tag, (unsigned)d->irqs, (unsigned)d->reads, (unsigned)d->events,
               (unsigned)d->empty_reads, (unsigned)d->invalid_reads, (unsigned)d->deliveries,
               (unsigned)d->recovered, (int)d->range_x, (int)d->range_y, (int)d->x, (int)d->y,
               d->state == LV_INDEV_STATE_PRESSED ? "pressed" : "released");
}

static int touch_watch(int seconds)
{
    lv_indev_t *indev = lv_aic_get_pointer_indev();
    lv_aic_touch_diagnostics_t first, now;
    int32_t min_x = INT32_MAX, min_y = INT32_MAX, max_x = INT32_MIN, max_y = INT32_MIN;
    uint32_t presses = 0, samples = 0, run = 0;
    int64_t sum_x = 0, sum_y = 0;
    bool was_pressed = false;

    if (indev == NULL || lv_aic_indev_get_diagnostics(indev, &first) != LV_AIC_OK) {
        rt_kprintf("touch: no pointer indev\n");
        return -1;
    }
    print_snapshot("start", &first);
    rt_kprintf("touch: watching %d s - touch the four corners and the centre\n", seconds);
    for (int elapsed = 0; elapsed < seconds * 1000; elapsed += WATCH_PERIOD_MS) {
        rt_thread_mdelay(WATCH_PERIOD_MS);
        if (lv_aic_indev_get_diagnostics(indev, &now) != LV_AIC_OK) {
            break;
        }
        bool pressed = now.state == LV_INDEV_STATE_PRESSED;
        if (pressed) {
            samples++;
            if (now.x < min_x) min_x = now.x;
            if (now.x > max_x) max_x = now.x;
            if (now.y < min_y) min_y = now.y;
            if (now.y > max_y) max_y = now.y;
            if (!was_pressed) {
                presses++;
                run = 0;
                sum_x = sum_y = 0;
            }
            run++;
            sum_x += now.x;
            sum_y += now.y;
        } else if (was_pressed && run) {
            /* One line per touch: where it landed (mean of its samples). */
            rt_kprintf("touch press %u: x=%d y=%d samples=%u\n", (unsigned)presses,
                       (int)(sum_x / run), (int)(sum_y / run), (unsigned)run);
        }
        was_pressed = pressed;
    }
    print_snapshot("end", &now);
    rt_kprintf("touch: irqs+%u reads+%u events+%u deliveries+%u presses=%u pressed_samples=%u\n",
               (unsigned)(now.irqs - first.irqs), (unsigned)(now.reads - first.reads),
               (unsigned)(now.events - first.events),
               (unsigned)(now.deliveries - first.deliveries), (unsigned)presses,
               (unsigned)samples);
    if (samples == 0) {
        rt_kprintf("touch: no press seen\n");
    } else {
        rt_kprintf("touch: extent x=%d..%d y=%d..%d\n", (int)min_x, (int)max_x, (int)min_y,
                   (int)max_y);
    }
    return 0;
}

static int lv_aic_touch(int argc, char **argv)
{
    lv_indev_t *indev = lv_aic_get_pointer_indev();
    lv_aic_touch_diagnostics_t d;

    if (argc >= 2 && strcmp(argv[1], "watch") == 0) {
        int seconds = argc >= 3 ? atoi(argv[2]) : WATCH_DEFAULT_S;
        if (seconds < 1 || seconds > WATCH_MAX_S) {
            rt_kprintf("usage: lv_aic_touch watch [1..%d]\n", WATCH_MAX_S);
            return -1;
        }
        return touch_watch(seconds);
    }
    if (indev == NULL || lv_aic_indev_get_diagnostics(indev, &d) != LV_AIC_OK) {
        rt_kprintf("touch: no pointer indev\n");
        return -1;
    }
    print_snapshot("now", &d);
    return 0;
}
MSH_CMD_EXPORT(lv_aic_touch, Touch counters and watch [seconds]);
#endif
