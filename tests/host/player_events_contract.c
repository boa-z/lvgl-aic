/* SPDX-License-Identifier: Apache-2.0 */
#include "lv_aic_player_events.h"
#include <aic_osal.h>
#include <assert.h>
#include <pthread.h>
#include <stdatomic.h>
#include <stdlib.h>
#include <string.h>
static event_handler callback;
static void *context;
static int fail_registration,fail_mutex;
static uint64_t wall;
static atomic_int running;
struct aic_player { int unused; };
static struct aic_player player;
aicos_mutex_t aicos_mutex_create(void)
{ if(fail_mutex) return NULL; pthread_mutex_t *m=malloc(sizeof(*m)); assert(m && !pthread_mutex_init(m,NULL)); return m; }
void aicos_mutex_delete(aicos_mutex_t m) { assert(!pthread_mutex_destroy(m)); free(m); }
int aicos_mutex_take(aicos_mutex_t m,uint32_t timeout) { (void)timeout; return pthread_mutex_lock(m); }
int aicos_mutex_give(aicos_mutex_t m) { return pthread_mutex_unlock(m); }
uint64_t aic_get_time_us(void) { return wall; }
s32 aic_player_set_event_callback(struct aic_player *p,void *data,event_handler handler)
{ assert(p==&player); context=data; callback=handler; return fail_registration?-1:0; }
static void send(int64_t pts,uint64_t now)
{
    wall=now; uint64_t bits=(uint64_t)pts;
    assert(!callback(context,AIC_PLAYER_EVENT_PLAY_TIME,(s32)(bits>>32),(s32)bits));
}
static void *emit(void *unused)
{
    (void)unused;
    for(unsigned i=0;i<10000;i++) send(i&1?-1234567890123LL:1234567890123LL,i&1?111:222);
    atomic_store(&running,0); return NULL;
}
int main(void)
{
    lv_aic_player_session_t session={.player=&player};
    lv_aic_player_event_snapshot_t snapshot;
    fail_mutex=1; assert(!lv_aic_player_events_create()); fail_mutex=0;
    lv_aic_player_events_t *events=lv_aic_player_events_create(); assert(events);
    session.started=true; assert(!lv_aic_player_events_attach(events,&session)); session.started=false;
    assert(lv_aic_player_events_attach(events,&session));
    assert(!lv_aic_player_events_attach(events,&session));
    assert(!lv_aic_player_events_destroy(events));
    assert(lv_aic_player_events_snapshot(events,&snapshot) && !snapshot.audio_valid && !snapshot.sequence);
    assert(!callback(context,99,0,0));
    assert(lv_aic_player_events_snapshot(events,&snapshot) && !snapshot.sequence);
    send(INT64_MIN,100);
    assert(lv_aic_player_events_snapshot(events,&snapshot) && snapshot.audio_pts==INT64_MIN && snapshot.audio_sample_us==100);
    send(INT64_MAX,101);
    assert(lv_aic_player_events_snapshot(events,&snapshot) && snapshot.audio_pts==INT64_MAX);
    send(-1,102);
    assert(lv_aic_player_events_snapshot(events,&snapshot) && snapshot.audio_pts==-1);
    assert(!callback(context,AIC_PLAYER_EVENT_DEMUXER_FORMAT_DETECTED,0,0));
    assert(!callback(context,AIC_PLAYER_EVENT_DEMUXER_FORMAT_NOT_DETECTED,0,0));
    assert(!callback(context,AIC_PLAYER_EVENT_PLAY_END,0,0));
    assert(lv_aic_player_events_snapshot(events,&snapshot) && snapshot.sequence==6 &&
           snapshot.terminal && snapshot.format_detected && snapshot.format_failed);
    send(1234567890123LL,222);
    atomic_store(&running,1); pthread_t producer; assert(!pthread_create(&producer,NULL,emit,NULL));
    do {
        assert(lv_aic_player_events_snapshot(events,&snapshot));
        assert((snapshot.audio_pts==-1234567890123LL && snapshot.audio_sample_us==111) ||
               (snapshot.audio_pts==1234567890123LL && snapshot.audio_sample_us==222));
        assert(snapshot.terminal && snapshot.format_failed);
    } while(atomic_load(&running));
    pthread_join(producer,NULL);
    assert(lv_aic_player_events_snapshot(events,&snapshot) && snapshot.sequence==10007);
    /* Simulate completed SDK destruction, after callbacks have stopped. */
    session.player=NULL; assert(lv_aic_player_events_destroy(events));
    events=lv_aic_player_events_create(); session.player=&player; fail_registration=1;
    assert(!lv_aic_player_events_attach(events,&session));
    assert(!lv_aic_player_events_destroy(events));
    send(42,123); /* Registration failure must not release a possibly installed callback. */
    assert(lv_aic_player_events_snapshot(events,&snapshot) && snapshot.registration_failed && snapshot.audio_pts==42);
    assert(!lv_aic_player_events_snapshot(events,NULL));
    session.player=NULL; assert(lv_aic_player_events_destroy(events));
    assert(lv_aic_player_events_destroy(NULL));
    return 0;
}
