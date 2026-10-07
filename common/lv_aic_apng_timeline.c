/* SPDX-License-Identifier: Apache-2.0 */
#include "lv_aic_apng_timeline.h"
#include <stddef.h>
static bool advance(lv_aic_apng_timeline_t *c,uint64_t now)
{
    if(!c->initialized || now<c->last_us) return false;
    uint64_t elapsed=c->paused?0:now-c->last_us;
    uint64_t whole=elapsed/c->rate_den,tail=(elapsed%c->rate_den)*c->rate_num+c->media_fraction;
    if(whole>(UINT64_MAX-c->media_us)/c->rate_num) return false;
    whole*=c->rate_num;
    if(tail/c->rate_den>UINT64_MAX-c->media_us-whole) return false;
    c->media_us+=whole+tail/c->rate_den;c->media_fraction=(uint32_t)(tail%c->rate_den);c->last_us=now;
    return true;
}
static bool due(const lv_aic_apng_timeline_t *c)
{
    return c->media_us>c->deadline_us || (c->media_us==c->deadline_us &&
        ((uint64_t)c->media_fraction<<32)>=(uint64_t)c->deadline_fraction*c->rate_den);
}
bool lv_aic_apng_timeline_init(lv_aic_apng_timeline_t *c,uint32_t frames,uint32_t plays,
    bool animated,uint32_t minimum,uint64_t now)
{
    if(!c || !frames || (!animated && frames!=1) || !minimum || minimum>1000000) return false;
    *c=(lv_aic_apng_timeline_t){.last_us=now,.rate_num=1,.rate_den=1,.frames=frames,
        .plays=animated?plays:1,.minimum_delay_us=minimum,.initialized=true,
        .animated=animated,.awaiting_frame=true};return true;
}
bool lv_aic_apng_timeline_poll(lv_aic_apng_timeline_t *clock,uint64_t now,lv_aic_apng_step_t *out)
{
    if(!clock || !out) return false;
    lv_aic_apng_timeline_t c=*clock;if(!advance(&c,now)) return false;
    lv_aic_apng_step_t step={.frame_index=c.index,.completed_plays=c.completed_plays};
    if(c.ended) step.action=LV_AIC_APNG_ENDED;
    else if(c.paused) step.action=LV_AIC_APNG_PAUSED;
    else {
        if(!c.awaiting_frame && due(&c)) {
            if(c.index+1<c.frames) c.index++;
            else {
                if(c.completed_plays==UINT64_MAX) return false;
                c.completed_plays++;
                if(c.plays && c.completed_plays>=c.plays) c.ended=true;
                else c.index=0;
            }
            c.awaiting_frame=!c.ended;
        }
        if(c.ended) step.action=LV_AIC_APNG_ENDED;
        else if(c.awaiting_frame) { step.action=LV_AIC_APNG_FRAME;step.reset_canvas=c.index==0; }
        else {
            step.action=LV_AIC_APNG_WAIT;
            /* Conservative integer-us wakeup, never earlier than the fractional
             * media deadline. Round up by less than one media microsecond,
             * then at most one wall-clock microsecond. */
            uint64_t delta=c.deadline_us-c.media_us;
            if((uint64_t)c.deadline_fraction*c.rate_den>((uint64_t)c.media_fraction<<32)) {
                if(delta==UINT64_MAX) return false;
                delta++;
            }
            uint64_t whole=delta/c.rate_num,tail=(delta%c.rate_num)*c.rate_den;
            if(whole>UINT64_MAX/c.rate_den) return false;
            whole*=c.rate_den;tail=tail/c.rate_num+(tail%c.rate_num!=0);
            if(tail>UINT64_MAX-whole) return false;
            step.wait_us=whole+tail;
        }
    }
    step.frame_index=c.index;step.completed_plays=c.completed_plays;
    *clock=c;*out=step;return true;
}
bool lv_aic_apng_timeline_commit(lv_aic_apng_timeline_t *clock,uint64_t now,uint16_t num,uint16_t den)
{
    if(!clock || !clock->awaiting_frame || clock->ended) return false;
    lv_aic_apng_timeline_t c=*clock;if(!advance(&c,now)) return false;
    if(!c.started) {
        c.deadline_us=c.media_us;c.deadline_fraction=(uint32_t)(((uint64_t)c.media_fraction<<32)/c.rate_den);
    }
    if(!c.animated) { c.ended=true;c.completed_plays=1; }
    else {
        if(!den) den=100;
        uint64_t units=(uint64_t)num*1000000;
        uint64_t duration=units/den,fraction=((units%den)<<32)/den;
        if(duration<c.minimum_delay_us) { duration=c.minimum_delay_us;fraction=0; }
        fraction+=c.deadline_fraction;
        duration+=fraction>>32;
        if(duration>UINT64_MAX-c.deadline_us) return false;
        c.deadline_us+=duration;c.deadline_fraction=(uint32_t)fraction;
    }
    c.started=true;c.awaiting_frame=false;*clock=c;return true;
}
bool lv_aic_apng_timeline_pause(lv_aic_apng_timeline_t *clock,uint64_t now,bool paused)
{
    if(!clock) return false;
    lv_aic_apng_timeline_t c=*clock;if(!advance(&c,now)) return false;
    c.paused=paused;*clock=c;return true;
}
bool lv_aic_apng_timeline_rate(lv_aic_apng_timeline_t *clock,uint64_t now,uint32_t num,uint32_t den)
{
    if(!clock || !num || !den || num>1000000 || den>1000000 || (uint64_t)num*10<den || (uint64_t)den*10<num) return false;
    lv_aic_apng_timeline_t c=*clock;if(!advance(&c,now)) return false;
    c.media_fraction=(uint32_t)((uint64_t)c.media_fraction*den/c.rate_den);
    c.rate_num=num;c.rate_den=den;*clock=c;return true;
}
