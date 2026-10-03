/* SPDX-License-Identifier: Apache-2.0 */
#ifndef LV_AIC_MEDIA_RUNTIME_H
#define LV_AIC_MEDIA_RUNTIME_H
#include <stdbool.h>
/* LVGL owner only. Acquire before launching any media/APNG worker. A shared
 * SDK VE reference initializes lazy SDK state serially and remains held until
 * all registered workers AND their native readers have finished. This does not
 * replace SDK hardware arbitration or manage unrelated external VE users. */
bool lv_aic_media_runtime_acquire(void);
void lv_aic_media_runtime_release(void);
/* Registered APNG workers serialize SDK tick/close operations here. Do not
 * hold this gate during sleeps, UI calls or while waiting for image readers.
 * This does not serialize SDK media player's internal codec threads. */
bool lv_aic_media_runtime_enter(void);
void lv_aic_media_runtime_leave(void);
/* Worker-only, registered runtime user. Current SDK audio device is global.
 * One owner may reserve it before SDK start; retain through seek/reopen and
 * release only after SDK session teardown. Wrong-owner release is rejected. */
bool lv_aic_media_audio_acquire(const void *owner);
bool lv_aic_media_audio_release(const void *owner);
#endif
