/* SPDX-License-Identifier: Apache-2.0
 * Fake RT API contract tests. Scheduler/ISR behavior needs board validation.
 */
#include "lv_os_private.h"
#include <assert.h>
#include <stdio.h>
struct test_object { uint32_t bits; int live; };
static struct test_object object;
static int fail_create, fail_start, fail_delete, deleted, takes;
static struct test_object *create(void) {
    if (fail_create) return NULL;
    object.bits = 0; object.live = 1; return &object;
}
static int destroy(struct test_object *p) {
    assert(p && p->live);
    if (fail_delete) return -1;
    p->live = 0; ++deleted; return 0;
}
rt_thread_t rt_thread_create(const char *n, void (*f)(void *), void *u, size_t s, int p, int t) {
    assert(n && f && u == &object && s == 1024 && p == 3 && t == 20); return create();
}
int rt_thread_startup(rt_thread_t p) { assert(p && p->live); return fail_start ? -1 : 0; }
int rt_thread_delete(rt_thread_t p) { return destroy(p); }
rt_mutex_t rt_mutex_create(const char *n, int f) { assert(n && f == RT_IPC_FLAG_PRIO); return create(); }
int rt_mutex_take(rt_mutex_t p, int t) { assert(p->live && t == RT_WAITING_FOREVER); ++takes; return 0; }
int rt_mutex_release(rt_mutex_t p) { assert(p->live); return 0; }
int rt_mutex_delete(rt_mutex_t p) { return destroy(p); }
rt_event_t rt_event_create(const char *n, int f) { assert(n && f == RT_IPC_FLAG_PRIO); return create(); }
int rt_event_send(rt_event_t p, uint32_t b) { assert(p->live); p->bits |= b; return 0; }
int rt_event_recv(rt_event_t p, uint32_t b, int f, int t, uint32_t *r) {
    assert(p->live && b == 1 && f == (RT_EVENT_FLAG_OR | RT_EVENT_FLAG_CLEAR) && t == RT_WAITING_FOREVER);
    *r = p->bits & b;
    if (!*r) return -2;
    p->bits &= ~*r; return 0;
}
int rt_event_delete(rt_event_t p) { return destroy(p); }
void rt_thread_mdelay(uint32_t ms) { (void)ms; }
uint32_t lv_timer_get_idle(void) { return 0; }
static void callback(void *p) { (void)p; }
int main(void) {
    lv_thread_sync_t sync = {0}; lv_thread_t thread = {0}; lv_mutex_t mutex = {0};
    assert(lv_thread_sync_init(NULL) == LV_RESULT_INVALID);
    assert(lv_thread_sync_wait(&sync) == LV_RESULT_INVALID);
    fail_create = 1;
    assert(lv_thread_sync_init(&sync) == LV_RESULT_INVALID);
    assert(lv_mutex_init(&mutex) == LV_RESULT_INVALID);
    assert(lv_thread_init(&thread, "test", 3, callback, 1024, &object) == LV_RESULT_INVALID);
    fail_create = 0;
    assert(lv_thread_sync_init(&sync) == LV_RESULT_OK);
    assert(lv_thread_sync_signal(&sync) == LV_RESULT_OK);
    assert(lv_thread_sync_signal_isr(&sync) == LV_RESULT_OK);
    assert(lv_thread_sync_wait(&sync) == LV_RESULT_OK);
    assert(object.bits == 0);
    assert(lv_thread_sync_wait(&sync) == LV_RESULT_INVALID);
    assert(lv_thread_sync_signal_isr(&sync) == LV_RESULT_OK);
    assert(lv_thread_sync_wait(&sync) == LV_RESULT_OK);
    fail_delete = 1;
    assert(lv_thread_sync_delete(&sync) == LV_RESULT_INVALID && sync.event);
    fail_delete = 0;
    assert(lv_thread_sync_delete(&sync) == LV_RESULT_OK && !sync.event);
    assert(lv_thread_sync_delete(&sync) == LV_RESULT_INVALID);
    assert(lv_thread_sync_signal_isr(&sync) == LV_RESULT_INVALID);
    fail_start = 1;
    int before = deleted;
    assert(lv_thread_init(&thread, "test", 3, callback, 1024, &object) == LV_RESULT_INVALID);
    assert(!thread.thread && deleted == before + 1);
    fail_start = 0;
    assert(lv_thread_init(&thread, "test", 3, callback, 1024, &object) == LV_RESULT_OK);
    assert(lv_thread_delete(&thread) == LV_RESULT_OK && !thread.thread);
    assert(lv_thread_delete(&thread) == LV_RESULT_INVALID);
    assert(lv_mutex_init(&mutex) == LV_RESULT_OK);
    assert(lv_mutex_lock_isr(&mutex) == LV_RESULT_INVALID && takes == 0);
    assert(lv_mutex_lock(&mutex) == LV_RESULT_OK && takes == 1);
    assert(lv_mutex_unlock(&mutex) == LV_RESULT_OK);
    assert(lv_mutex_delete(&mutex) == LV_RESULT_OK && !mutex.mutex);
    assert(lv_mutex_lock(&mutex) == LV_RESULT_INVALID);
    puts("Application OS contract: PASS (mock API, not board)");
    return 0;
}
