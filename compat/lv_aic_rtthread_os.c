/* SPDX-License-Identifier: Apache-2.0 */
#include "lv_os_private.h"
#if LV_USE_OS == LV_OS_CUSTOM
#include <rtthread.h>

static lv_result_t result(rt_err_t error)
{
    return error == RT_EOK ? LV_RESULT_OK : LV_RESULT_INVALID;
}

lv_result_t lv_thread_init(lv_thread_t *thread, const char *const name,
                          lv_thread_prio_t prio, void (*callback)(void *),
                          size_t stack_size, void *user_data)
{
    if (!thread || !callback) return LV_RESULT_INVALID;
    thread->thread = rt_thread_create(name, callback, user_data, stack_size, prio, 20);
    if (!thread->thread) return LV_RESULT_INVALID;
    rt_err_t error = rt_thread_startup(thread->thread);
    if (error != RT_EOK) {
        rt_thread_delete(thread->thread);
        thread->thread = RT_NULL;
    }
    return result(error);
}

lv_result_t lv_thread_delete(lv_thread_t *thread)
{
    if (!thread || !thread->thread) return LV_RESULT_INVALID;
    rt_err_t error = rt_thread_delete(thread->thread);
    if (error == RT_EOK) thread->thread = RT_NULL;
    return result(error);
}
lv_result_t lv_mutex_init(lv_mutex_t *mutex)
{
    if (!mutex) return LV_RESULT_INVALID;
    mutex->mutex = rt_mutex_create("lv_mutex", RT_IPC_FLAG_PRIO);
    return mutex->mutex ? LV_RESULT_OK : LV_RESULT_INVALID;
}
lv_result_t lv_mutex_lock(lv_mutex_t *mutex)
{
    return mutex && mutex->mutex ? result(rt_mutex_take(mutex->mutex, RT_WAITING_FOREVER)) : LV_RESULT_INVALID;
}
lv_result_t lv_mutex_lock_isr(lv_mutex_t *mutex)
{
    /* RT-Thread 互斥锁不允许在中断中阻塞。 */
    (void)mutex;
    return LV_RESULT_INVALID;
}
lv_result_t lv_mutex_unlock(lv_mutex_t *mutex)
{
    return mutex && mutex->mutex ? result(rt_mutex_release(mutex->mutex)) : LV_RESULT_INVALID;
}
lv_result_t lv_mutex_delete(lv_mutex_t *mutex)
{
    if (!mutex || !mutex->mutex) return LV_RESULT_INVALID;
    rt_err_t error = rt_mutex_delete(mutex->mutex);
    if (error == RT_EOK) mutex->mutex = RT_NULL;
    return result(error);
}
lv_result_t lv_thread_sync_init(lv_thread_sync_t *sync)
{
    if (!sync) return LV_RESULT_INVALID;
    sync->event = rt_event_create("lv_sync", RT_IPC_FLAG_PRIO);
    return sync->event ? LV_RESULT_OK : LV_RESULT_INVALID;
}
lv_result_t lv_thread_sync_wait(lv_thread_sync_t *sync)
{
    rt_uint32_t received = 0;
    if (!sync || !sync->event) return LV_RESULT_INVALID;
    /* 同一位合并重复通知，CLEAR 在内核临界区内消费，避免清零竞态。 */
    return result(rt_event_recv(sync->event, 1U, RT_EVENT_FLAG_OR | RT_EVENT_FLAG_CLEAR,
                               RT_WAITING_FOREVER, &received));
}
lv_result_t lv_thread_sync_signal(lv_thread_sync_t *sync)
{
    return sync && sync->event ? result(rt_event_send(sync->event, 1U)) : LV_RESULT_INVALID;
}
lv_result_t lv_thread_sync_signal_isr(lv_thread_sync_t *sync)
{
    /* 原厂 RT-Thread 的事件发送支持中断上下文。 */
    return lv_thread_sync_signal(sync);
}
lv_result_t lv_thread_sync_delete(lv_thread_sync_t *sync)
{
    if (!sync || !sync->event) return LV_RESULT_INVALID;
    rt_err_t error = rt_event_delete(sync->event);
    if (error == RT_EOK) sync->event = RT_NULL;
    return result(error);
}
uint32_t lv_os_get_idle_percent(void) { return lv_timer_get_idle(); }
void lv_sleep_ms(uint32_t ms) { rt_thread_mdelay(ms); }
#endif
