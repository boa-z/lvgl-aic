/* SPDX-License-Identifier: Apache-2.0 */
#include "lv_aic_media_runtime.h"
#include <assert.h>
#include <aic_osal.h>
#include <stddef.h>
static unsigned opened,closed;
static int fail,mutex_live,locked;
aicos_mutex_t aicos_mutex_create(void) { assert(!mutex_live);mutex_live=1;return &mutex_live; }
void aicos_mutex_delete(aicos_mutex_t m) { assert(m==&mutex_live && !locked);mutex_live=0; }
int aicos_mutex_take(aicos_mutex_t m,uint32_t timeout)
{ assert(m==&mutex_live && mutex_live && !locked && timeout==AICOS_WAIT_FOREVER);locked=1;return 0; }
int aicos_mutex_give(aicos_mutex_t m) { assert(m==&mutex_live && locked);locked=0;return 0; }
int ve_open_device(void) { opened++;return fail?-1:0; }
void ve_close_device(void) { closed++; }
int main(void)
{
    fail=1;assert(!lv_aic_media_runtime_acquire());assert(opened==1 && !closed);
    lv_aic_media_runtime_release();assert(!closed); /* Failed acquire owns nothing. */
    fail=0;
    for(unsigned i=0;i<8;i++) assert(lv_aic_media_runtime_acquire());
    assert(opened==2 && !closed);
    assert(lv_aic_media_runtime_enter());lv_aic_media_runtime_leave();
    int a,b;
    assert(!lv_aic_media_audio_acquire(NULL));
    assert(lv_aic_media_audio_acquire(&a) && lv_aic_media_audio_acquire(&a));
    assert(!lv_aic_media_audio_acquire(&b) && !lv_aic_media_audio_release(&b));
    assert(lv_aic_media_audio_release(&a) && !lv_aic_media_audio_release(&a));
    assert(lv_aic_media_audio_acquire(&b) && lv_aic_media_audio_release(&b)); /* Mixed managed users share one SDK reference. */
    for(unsigned i=0;i<7;i++) lv_aic_media_runtime_release();
    assert(!closed);
    lv_aic_media_runtime_release();assert(closed==1);
    lv_aic_media_runtime_release();assert(closed==1);
    assert(lv_aic_media_runtime_acquire());assert(opened==3);
    lv_aic_media_runtime_release();assert(closed==2 && !mutex_live);
    assert(!lv_aic_media_runtime_enter());
    return 0;
}
