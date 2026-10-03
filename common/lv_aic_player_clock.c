/* SPDX-License-Identifier: Apache-2.0 */
#include "lv_aic_player_clock.h"
void lv_aic_player_clock_reset(lv_aic_player_clock_t *c,int64_t pts,uint64_t now)
{
    if(c) *c=(lv_aic_player_clock_t){pts,now,now,true,false};
}
static bool value(const lv_aic_player_clock_t *c,uint64_t now,int64_t *pts)
{
    if(!c || !c->valid || now<c->last_us || now<c->reference_us) return false;
    uint64_t elapsed=c->paused?0:now-c->reference_us;
    if(elapsed>INT64_MAX || c->reference_pts>INT64_MAX-(int64_t)elapsed) return false;
    *pts=c->reference_pts+(int64_t)elapsed; return true;
}
bool lv_aic_player_clock_sync(lv_aic_player_clock_t *c,int64_t pts,uint64_t now)
{
    if(!c || (c->valid && (c->paused || now<c->last_us))) return false;
    lv_aic_player_clock_reset(c,pts,now); return true;
}
bool lv_aic_player_clock_time(lv_aic_player_clock_t *c,uint64_t now,int64_t *pts)
{
    int64_t next;
    if(!pts || !value(c,now,&next)) return false;
    c->last_us=now; *pts=next; return true;
}
bool lv_aic_player_clock_pause(lv_aic_player_clock_t *c,bool paused,uint64_t now)
{
    int64_t pts;
    if(!value(c,now,&pts)) return false;
    c->reference_pts=pts; c->reference_us=c->last_us=now; c->paused=paused; return true;
}
bool lv_aic_player_clock_schedule(lv_aic_player_clock_t *c,int64_t frame_pts,
    uint64_t now,uint64_t late,lv_aic_player_clock_action_t *action,uint64_t *delay)
{
    int64_t pts;
    if(!action || !delay || !value(c,now,&pts)) return false;
    lv_aic_player_clock_action_t next; uint64_t wait=0;
    if(c->paused) next=LV_AIC_PLAYER_CLOCK_PAUSED;
    else if(frame_pts>pts) {
        wait=(uint64_t)frame_pts-(uint64_t)pts; next=LV_AIC_PLAYER_CLOCK_WAIT;
    } else next=(uint64_t)pts-(uint64_t)frame_pts>late?LV_AIC_PLAYER_CLOCK_DROP:LV_AIC_PLAYER_CLOCK_PRESENT;
    c->last_us=now; *action=next; *delay=wait; return true;
}
