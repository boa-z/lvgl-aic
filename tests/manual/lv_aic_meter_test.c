/* SPDX-License-Identifier: Apache-2.0 */
/* Shell gate for the application-owned meter cluster demo. The shell only
 * queues a request; creation/destruction runs on the LVGL owner thread via
 * lv_aic_meter_test_poll(), following the APNG test mailbox pattern. */
#include "lv_aic_meter_test.h"
#if defined(AIC_LVGL_BUILD_DEMO_METER) && AIC_LVGL_BUILD_DEMO_METER
#include "demos/meter_demo/meter_ui.h"

#if AIC_LVGL_BSP_RTTHREAD
#include <rtthread.h>
#include <rthw.h>
#include <finsh.h>
#include <string.h>

enum meter_command { METER_NONE, METER_SHOW, METER_CLOSE };
static volatile enum meter_command meter_pending;
static volatile bool meter_ready;
static char meter_asset_root[128];

void lv_aic_meter_test_poll(void)
{
    enum meter_command cmd;
    rt_base_t level = rt_hw_interrupt_disable();
    meter_ready = true;
    cmd = meter_pending;
    meter_pending = METER_NONE;
    rt_hw_interrupt_enable(level);
    if (cmd != METER_NONE) {
        if (cmd == METER_CLOSE) {
            lv_aic_meter_test_deinit();
            rt_kprintf("meter demo closed\n");
        } else {
            meter_ui_config_t config;
            memset(&config, 0, sizeof(config));
            config.asset_root = meter_asset_root[0] ? meter_asset_root : NULL;
            config.fps = NULL;
            meter_ui_configure(&config);
            meter_ui_init();
            rt_kprintf("meter demo running fires=%u speed=%d angle=%d\n",
                       (unsigned)meter_ui_timer_fires(),
                       meter_ui_get_speed_step(),
                       (int)meter_ui_get_needle_angle());
        }
    }
}

void lv_aic_meter_test_deinit(void)
{
    rt_base_t level = rt_hw_interrupt_disable();
    meter_ready = false;
    meter_pending = METER_NONE;
    rt_hw_interrupt_enable(level);
    meter_ui_destroy();
}

static void lv_aic_meter_test(int argc, char **argv)
{
    enum meter_command cmd = METER_NONE;
    if (argc >= 2) {
        if (!strcmp(argv[1], "show")) {
            cmd = METER_SHOW;
        } else if (!strcmp(argv[1], "close")) {
            cmd = METER_CLOSE;
        }
    }
    if (cmd == METER_NONE) {
        rt_kprintf("Usage: lv_aic_meter_test show|close [asset_root]\n");
        rt_kprintf("default asset_root: L:/rodata/lvgl_data\n");
        return;
    }
    rt_base_t level = rt_hw_interrupt_disable();
    bool ok = meter_ready && (meter_pending == METER_NONE);
    if (ok) {
        meter_pending = cmd;
        if (argc >= 3) {
            snprintf(meter_asset_root, sizeof(meter_asset_root), "%s", argv[2]);
        } else {
            snprintf(meter_asset_root, sizeof(meter_asset_root), "L:/rodata/lvgl_data");
        }
    }
    rt_hw_interrupt_enable(level);
    rt_kprintf(ok ? "meter request queued\n" : "meter busy or UI not ready; retry\n");
}
MSH_CMD_EXPORT(lv_aic_meter_test, Meter cluster demo controls on UI thread);
#else
void lv_aic_meter_test_poll(void) {}
void lv_aic_meter_test_deinit(void) {}
#endif
#endif
