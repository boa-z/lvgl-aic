/* SPDX-License-Identifier: Apache-2.0 */
/* Full-screen touch test and calibration check, opened from the smoke app's
 * navigation bar. UI thread only. */
#ifndef LV_AIC_TOUCH_PAGE_H
#define LV_AIC_TOUCH_PAGE_H

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* True when the page is compiled in (touch enabled). */
bool lv_aic_touch_page_available(void);
/* Create the overlay on the top layer; no-op when already open. */
void lv_aic_touch_page_open(void);
/* Delete it; safe when closed. Also called by the manual test teardown. */
void lv_aic_touch_page_close(void);

#ifdef __cplusplus
}
#endif

#endif
