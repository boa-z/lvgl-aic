/* SPDX-License-Identifier: Apache-2.0 */
#include "lv_aic_rtthread_os.h"
#define LV_OS_CUSTOM 255
#define LV_USE_OS LV_OS_CUSTOM
typedef enum { LV_RESULT_INVALID, LV_RESULT_OK } lv_result_t;
typedef int lv_thread_prio_t;
uint32_t lv_timer_get_idle(void);
lv_result_t lv_thread_init(lv_thread_t *, const char *, lv_thread_prio_t, void (*)(void *), size_t, void *);
lv_result_t lv_thread_delete(lv_thread_t *);
lv_result_t lv_mutex_init(lv_mutex_t *);
lv_result_t lv_mutex_lock(lv_mutex_t *);
lv_result_t lv_mutex_lock_isr(lv_mutex_t *);
lv_result_t lv_mutex_unlock(lv_mutex_t *);
lv_result_t lv_mutex_delete(lv_mutex_t *);
lv_result_t lv_thread_sync_init(lv_thread_sync_t *);
lv_result_t lv_thread_sync_wait(lv_thread_sync_t *);
lv_result_t lv_thread_sync_signal(lv_thread_sync_t *);
lv_result_t lv_thread_sync_signal_isr(lv_thread_sync_t *);
lv_result_t lv_thread_sync_delete(lv_thread_sync_t *);
