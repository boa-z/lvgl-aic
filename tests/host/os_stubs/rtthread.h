/* SPDX-License-Identifier: Apache-2.0 */
#ifndef TEST_RTTHREAD_H
#define TEST_RTTHREAD_H
#include <stddef.h>
#include <stdint.h>
typedef int rt_err_t;
typedef uint32_t rt_uint32_t;
typedef struct test_object *rt_thread_t;
typedef struct test_object *rt_mutex_t;
typedef struct test_object *rt_event_t;
#define RT_NULL NULL
#define RT_EOK 0
#define RT_IPC_FLAG_PRIO 1
#define RT_WAITING_FOREVER (-1)
#define RT_EVENT_FLAG_OR 1
#define RT_EVENT_FLAG_CLEAR 4
rt_thread_t rt_thread_create(const char *, void (*)(void *), void *, size_t, int, int);
rt_err_t rt_thread_startup(rt_thread_t);
rt_err_t rt_thread_delete(rt_thread_t);
rt_mutex_t rt_mutex_create(const char *, int);
rt_err_t rt_mutex_take(rt_mutex_t, int);
rt_err_t rt_mutex_release(rt_mutex_t);
rt_err_t rt_mutex_delete(rt_mutex_t);
rt_event_t rt_event_create(const char *, int);
rt_err_t rt_event_recv(rt_event_t, rt_uint32_t, int, int, rt_uint32_t *);
rt_err_t rt_event_send(rt_event_t, rt_uint32_t);
rt_err_t rt_event_delete(rt_event_t);
void rt_thread_mdelay(uint32_t);
#endif
