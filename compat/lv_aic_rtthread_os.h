/* SPDX-License-Identifier: Apache-2.0 */
#ifndef LV_AIC_RTTHREAD_OS_H
#define LV_AIC_RTTHREAD_OS_H
#include <rtthread.h>
typedef struct { rt_thread_t thread; } lv_thread_t;
typedef struct { rt_mutex_t mutex; } lv_mutex_t;
typedef struct { rt_event_t event; } lv_thread_sync_t;
#endif
