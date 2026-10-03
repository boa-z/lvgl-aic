/* SPDX-License-Identifier: Apache-2.0 */
#include "lv_aic_player_clock.h"
#include <assert.h>
#include <string.h>
static void schedule(lv_aic_player_clock_t *c,int64_t pts,uint64_t now,uint64_t late,
                     lv_aic_player_clock_action_t expected,uint64_t wait)
{
    lv_aic_player_clock_action_t action=99; uint64_t delay=99;
    assert(lv_aic_player_clock_schedule(c,pts,now,late,&action,&delay));
    assert(action==expected && delay==wait);
}
int main(void)
{
    lv_aic_player_clock_t c={0},saved;
    int64_t pts=99;
    assert(!lv_aic_player_clock_time(&c,0,&pts) && pts==99);
    lv_aic_player_clock_reset(&c,1000000,500);
    schedule(&c,1040000,500,5000,LV_AIC_PLAYER_CLOCK_WAIT,40000);
    schedule(&c,1040000,40499,5000,LV_AIC_PLAYER_CLOCK_WAIT,1);
    schedule(&c,1040000,40500,5000,LV_AIC_PLAYER_CLOCK_PRESENT,0);
    schedule(&c,1040000,45500,5000,LV_AIC_PLAYER_CLOCK_PRESENT,0);
    schedule(&c,1040000,45501,5000,LV_AIC_PLAYER_CLOCK_DROP,0);
    assert(lv_aic_player_clock_pause(&c,true,50000));
    assert(lv_aic_player_clock_time(&c,150000,&pts) && pts==1049500);
    assert(lv_aic_player_clock_pause(&c,true,160000));
    assert(!lv_aic_player_clock_sync(&c,0,160001));
    schedule(&c,0,170000,0,LV_AIC_PLAYER_CLOCK_PAUSED,0);
    assert(lv_aic_player_clock_pause(&c,false,200000));
    assert(lv_aic_player_clock_time(&c,200001,&pts) && pts==1049501);
    assert(lv_aic_player_clock_sync(&c,-10000,200002));
    assert(lv_aic_player_clock_time(&c,205002,&pts) && pts==-5000);
    schedule(&c,0,205002,0,LV_AIC_PLAYER_CLOCK_WAIT,5000);
    saved=c;
    lv_aic_player_clock_action_t action=99; uint64_t delay=99;
    assert(!lv_aic_player_clock_schedule(&c,0,205001,0,&action,&delay));
    assert(action==99 && delay==99 && !memcmp(&saved,&c,sizeof(c)));
    assert(!lv_aic_player_clock_sync(&c,0,205001));
    assert(!lv_aic_player_clock_pause(&c,false,205001));
    lv_aic_player_clock_reset(&c,INT64_MAX-1,0);
    assert(lv_aic_player_clock_time(&c,1,&pts) && pts==INT64_MAX);
    saved=c; assert(!lv_aic_player_clock_time(&c,2,&pts));
    assert(pts==INT64_MAX && !memcmp(&saved,&c,sizeof(c)));
    lv_aic_player_clock_reset(&c,INT64_MIN,0);
    schedule(&c,INT64_MAX,0,0,LV_AIC_PLAYER_CLOCK_WAIT,UINT64_MAX);
    lv_aic_player_clock_reset(&c,INT64_MAX,0);
    schedule(&c,INT64_MIN,0,UINT64_MAX-1,LV_AIC_PLAYER_CLOCK_DROP,0);
    schedule(&c,INT64_MIN,0,UINT64_MAX,LV_AIC_PLAYER_CLOCK_PRESENT,0);
    assert(!lv_aic_player_clock_time(&c,UINT64_MAX,&pts));
    /* Explicit seek/source reset starts a new timeline, including backwards PTS. */
    lv_aic_player_clock_reset(&c,40000,40000);
    schedule(&c,80000,40000,0,LV_AIC_PLAYER_CLOCK_WAIT,40000);
    assert(!lv_aic_player_clock_time(&c,40000,NULL));
    assert(!lv_aic_player_clock_schedule(&c,0,40000,0,NULL,&delay));
    assert(!lv_aic_player_clock_schedule(&c,0,40000,0,&action,NULL));
    assert(!lv_aic_player_clock_sync(NULL,0,0));
    lv_aic_player_clock_reset(NULL,0,0);
    return 0;
}
