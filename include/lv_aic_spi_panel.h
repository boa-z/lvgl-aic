/* SPDX-License-Identifier: Apache-2.0 */
#ifndef LV_AIC_SPI_PANEL_H
#define LV_AIC_SPI_PANEL_H
#include "lv_aic_spi_sdk.h"
#ifdef __cplusplus
extern "C" {
#endif
typedef struct lv_aic_spi_panel lv_aic_spi_panel_t;
typedef struct {
    const uint8_t *data;
    size_t bytes,capacity;
    uint32_t prefix;
    unsigned prefix_bytes,prefix_lines,data_lines;
    bool data_mode;
} lv_aic_spi_panel_step_t;
/* Snapshot at most 64 steps. Borrow payloads/device/context until close succeeds.
 * Each payload occupies dedicated 64-byte-aligned DMA/cache storage; capacity
 * includes cache rounding. Immutable while in use. No pin or panel defaults.
 * set_dc is synchronous and checked; it must not start DMA. NULL supports
 * prefix-framed panels without D/C. final_data_mode is applied after all steps.
 * Exclusive single-worker use. No unmanaged bus users; no configuration changes. */
lv_aic_spi_panel_t *lv_aic_spi_panel_create(struct rt_qspi_device *device,
    const lv_aic_spi_panel_step_t *steps,size_t count,uint32_t width,uint32_t height,
    bool (*set_dc)(void *,bool),void *context,bool final_data_mode);
typedef struct {
    bool (*power)(void *context,bool enabled);
    bool (*initialize)(void *context);
    bool (*wait_te)(void *context,uint32_t timeout_ms);
    void *context;
    uint32_t te_timeout_ms;
} lv_aic_spi_panel_lifecycle_t;
/* Optional callbacks, copied before first prepare and before worker startup.
 * First prepare: power-on, initialize, TE wait, then frame commands. Subsequent
 * frames wait TE without repeating power/init. close powers off after a normal
 * drained session close. Keep unmanaged bus clients excluded through panel close.
 * All callbacks synchronous and checked; initialize must complete command DMA.
 * TE timeout must be 1..60000 ms if wait_te is present, zero otherwise. Callback
 * must honor that bound. No guessed pins/delays/commands; false is sticky fault.
 * Callback context/command memory remain alive until close or reboot on fault. */
bool lv_aic_spi_panel_set_lifecycle(lv_aic_spi_panel_t *panel,
    const lv_aic_spi_panel_lifecycle_t *lifecycle);
/* Directly usable as session config.prepare with the panel as prepare_context.
 * Sticky fault after any uncertain command/pin result; no subsequent replay. */
bool lv_aic_spi_panel_prepare(void *panel,uint32_t width,uint32_t height);
/* Close the referencing session successfully before closing this panel.
 * False while busy/faulted; retain payloads and context until reboot on fault. */
bool lv_aic_spi_panel_close(lv_aic_spi_panel_t *panel);
#ifdef __cplusplus
}
#endif
#endif
