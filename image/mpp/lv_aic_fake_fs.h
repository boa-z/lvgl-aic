/* SPDX-License-Identifier: Apache-2.0 */
#ifndef LV_AIC_FAKE_FS_H
#define LV_AIC_FAKE_FS_H
#include <stdbool.h>
bool lv_aic_fake_fs_install(void);
bool lv_aic_fake_fs_idle(void);
void lv_aic_fake_fs_restore(void);
#endif
