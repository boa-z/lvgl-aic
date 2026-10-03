/* SPDX-License-Identifier: Apache-2.0 */
#include "lv_aic_apng_timeline.h"
#include <assert.h>
#include <string.h>
static lv_aic_apng_step_t poll(lv_aic_apng_timeline_t *c,uint64_t now)
{ lv_aic_apng_step_t s;assert(lv_aic_apng_timeline_poll(c,now,&s));return s; }
int main(void)
{
    lv_aic_apng_timeline_t c={0},before;lv_aic_apng_step_t s;
    assert(!lv_aic_apng_timeline_init(&c,0,1,true,1000,0));
    assert(lv_aic_apng_timeline_init(&c,2,2,true,1000,100));
    s=poll(&c,100);assert(s.action==LV_AIC_APNG_FRAME && s.reset_canvas && s.frame_index==0);
    assert(lv_aic_apng_timeline_commit(&c,150,1,10));
    assert(!lv_aic_apng_timeline_commit(&c,150,1,10));
    s=poll(&c,100149);assert(s.action==LV_AIC_APNG_WAIT && s.wait_us==1);
    s=poll(&c,100150);assert(s.action==LV_AIC_APNG_FRAME && s.frame_index==1 && !s.reset_canvas);
    assert(lv_aic_apng_timeline_commit(&c,100150,1,10));
    s=poll(&c,200150);assert(s.action==LV_AIC_APNG_FRAME && s.frame_index==0 && s.reset_canvas && s.completed_plays==1);
    assert(lv_aic_apng_timeline_commit(&c,200150,1,10));
    s=poll(&c,300150);assert(s.frame_index==1);assert(lv_aic_apng_timeline_commit(&c,300150,1,10));
    assert(poll(&c,400149).action==LV_AIC_APNG_WAIT);
    s=poll(&c,400150);assert(s.action==LV_AIC_APNG_ENDED && s.completed_plays==2);
    assert(lv_aic_apng_timeline_init(&c,1,0,true,1000,0));
    assert(lv_aic_apng_timeline_commit(&c,0,0,0)); /* Zero delay uses explicit minimum. */
    assert(lv_aic_apng_timeline_pause(&c,500,true));
    assert(poll(&c,500000).action==LV_AIC_APNG_PAUSED);
    assert(lv_aic_apng_timeline_rate(&c,500000,2,1));
    assert(lv_aic_apng_timeline_pause(&c,500000,false));
    s=poll(&c,500000);assert(s.wait_us==250);
    assert(poll(&c,500249).action==LV_AIC_APNG_WAIT);
    s=poll(&c,500250);assert(s.action==LV_AIC_APNG_FRAME && s.completed_plays==1);
    assert(lv_aic_apng_timeline_commit(&c,500250,1,100));
    assert(lv_aic_apng_timeline_rate(&c,500250,1,10));
    assert(poll(&c,500250).wait_us==100000);
    /* Late decode still requests every disposal-dependent intermediate frame. */
    assert(lv_aic_apng_timeline_init(&c,3,1,true,1,0));assert(lv_aic_apng_timeline_commit(&c,0,1,1000));
    assert(poll(&c,100000).frame_index==1);assert(lv_aic_apng_timeline_commit(&c,100000,1,1000));
    s=poll(&c,100000);assert(s.action==LV_AIC_APNG_FRAME && s.frame_index==2);
    assert(lv_aic_apng_timeline_commit(&c,100000,1,1000));assert(poll(&c,100000).action==LV_AIC_APNG_ENDED);
    /* 60 Hz fractional delays do not drift by whole microseconds per frame. */
    assert(lv_aic_apng_timeline_init(&c,1,0,true,1,0));
    uint64_t now=0;
    for(unsigned i=0;i<6000;i++) {
        assert(poll(&c,now).action==LV_AIC_APNG_FRAME);
        assert(lv_aic_apng_timeline_commit(&c,now,1,60));
        s=poll(&c,now);assert(s.action==LV_AIC_APNG_WAIT);now+=s.wait_us;
    }
    assert(now>=99999999 && now<=100000001);
    before=c;s=(lv_aic_apng_step_t){.wait_us=999};
    assert(!lv_aic_apng_timeline_poll(&c,c.last_us-1,&s) && !memcmp(&c,&before,sizeof(c)) && s.wait_us==999);
    assert(!lv_aic_apng_timeline_rate(&c,now,11,1) && !memcmp(&c,&before,sizeof(c)));
    assert(!lv_aic_apng_timeline_rate(&c,now,0,1));
    assert(lv_aic_apng_timeline_init(&c,1,1,false,1,0));
    assert(lv_aic_apng_timeline_commit(&c,0,65535,1));assert(poll(&c,0).action==LV_AIC_APNG_ENDED);
    assert(lv_aic_apng_timeline_init(&c,1,0,true,1,0));
    assert(lv_aic_apng_timeline_rate(&c,0,3,2));assert(lv_aic_apng_timeline_commit(&c,0,1,60));
    s=poll(&c,1);assert(s.action==LV_AIC_APNG_WAIT && s.wait_us==11111);
    assert(c.media_us==1 && c.media_fraction==1);
    before=c;assert(!lv_aic_apng_timeline_rate(&c,1,1,11) && !memcmp(&c,&before,sizeof(c)));
    assert(lv_aic_apng_timeline_pause(&c,1,true));before=c;
    assert(lv_aic_apng_timeline_pause(&c,100,true) && c.media_us==before.media_us && c.media_fraction==before.media_fraction);
    /* Wall-clock endpoints and media overflow are distinct and transactional. */
    assert(lv_aic_apng_timeline_init(&c,1,0,true,1,UINT64_MAX-1));
    assert(lv_aic_apng_timeline_commit(&c,UINT64_MAX-1,1,1000));assert(poll(&c,UINT64_MAX).action==LV_AIC_APNG_WAIT);
    assert(lv_aic_apng_timeline_init(&c,1,0,true,1,0));assert(lv_aic_apng_timeline_rate(&c,0,10,1));before=c;
    assert(!lv_aic_apng_timeline_poll(&c,UINT64_MAX,&s) && !memcmp(&c,&before,sizeof(c)));
    return 0;
}
