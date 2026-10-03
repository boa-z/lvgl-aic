/* SPDX-License-Identifier: Apache-2.0 */
#include "lv_aic_player_session.h"
#include <assert.h>
#include <frame_allocator.h>
#include <string.h>
struct aic_player { int live; };
static struct aic_player instance;
static int created, destroyed, stops, prepares, starts, pauses, resumes, seeks, gets, puts;
static int failure, returned_volume=70, controls;
static struct frame_allocator allocator;
static int64_t position=123456;
static unsigned frame_id;
static struct av_media_info media={.has_video=1,.has_audio=1,.seek_able=1,.duration=1000000,
    .video_stream={32,16}};
enum { CREATE=1,URI,PREPARE,INFO,START,PAUSE,PLAY,STOP,DESTROY,GET,PUT,SEEK,VOLUME,ALLOCATOR,COUNT };
struct aic_player *aic_player_create(char *uri)
{ assert(!uri && !instance.live); if(failure==CREATE) return NULL; instance.live=1; created++; return &instance; }
s32 aic_player_set_uri(struct aic_player *p,char *uri)
{ assert(p->live && !strcmp(uri,"/media/test.mp4")); return failure==URI ? -1 : 0; }
s32 aic_player_prepare_sync(struct aic_player *p)
{ assert(p->live); prepares++; return failure==PREPARE ? -1 : 0; }
s32 aic_player_get_media_info(struct aic_player *p,struct av_media_info *info)
{ assert(p->live); *info=media; return failure==INFO ? -1 : 0; }
s32 aic_player_start(struct aic_player *p)
{ assert(p->live); starts++; return failure==START ? -1 : 0; }
s32 aic_player_pause(struct aic_player *p)
{ assert(p->live); pauses++; return failure==PAUSE ? -1 : 0; }
s32 aic_player_play(struct aic_player *p)
{ assert(p->live); resumes++; return failure==PLAY ? -1 : 0; }
s32 aic_player_stop(struct aic_player *p)
{ assert(p->live); stops++; return failure==STOP ? -1 : 0; }
s32 aic_player_destroy(struct aic_player *p)
{ assert(p->live); if(failure==DESTROY) return -1; p->live=0; destroyed++; return 0; }
s32 aic_player_get_frame(struct aic_player *p,struct mpp_frame *frame)
{ assert(p->live); gets++; frame->id=frame_id++; frame->pts=123; return failure==GET ? -1 : 0; }
s32 aic_player_put_frame(struct aic_player *p,struct mpp_frame *frame)
{ assert(p->live && frame->pts==123); puts++; return failure==PUT ? -1 : 0; }
s32 aic_player_seek(struct aic_player *p,u64 us)
{ assert(p->live && us<=1000000); seeks++; return failure==SEEK ? -1 : 0; }
s32 aic_player_set_volum(struct aic_player *p,s32 volume)
{ assert(p->live && volume>=0 && volume<=100); return failure==VOLUME ? -1 : 0; }
s32 aic_player_get_volum(struct aic_player *p,s32 *volume)
{ assert(p->live); *volume=returned_volume; return failure==VOLUME ? -1 : 0; }
s64 aic_player_get_play_time(struct aic_player *p) { assert(p->live); return position; }
s32 aic_player_control(struct aic_player *p,enum aic_player_command command,void *data)
{
    assert(p->live); controls++;
    if(command==AIC_PLAYER_CMD_SET_VDEC_EXT_FRAME_ALLOCATOR) {
        assert(data==&allocator); return failure==ALLOCATOR?-1:0;
    }
    assert(command==AIC_PLAYER_CMD_SET_VDEC_EXT_FRAME_NUM && *(s32 *)data==3);
    return failure==COUNT?-1:0;
}
int main(void)
{
    lv_aic_player_session_t s={0};
    const struct mpp_frame *frame=(void *)(uintptr_t)1;
    uint64_t lease=99, old;
    assert(!lv_aic_player_session_open(&s,NULL));
    char long_path[129]; memset(long_path,'x',128); long_path[128]=0;
    assert(!lv_aic_player_session_open(&s,long_path));
    for(failure=CREATE;failure<=INFO;failure++) {
        assert(!lv_aic_player_session_open(&s,"/media/test.mp4"));
        assert(!s.player && !instance.live);
    }
    for(failure=ALLOCATOR;failure<=COUNT;failure++) {
        assert(lv_aic_player_session_open(&s,"/media/test.mp4"));
        assert(!lv_aic_player_session_allocator(&s,&allocator,3));
        assert(s.faulted && !lv_aic_player_session_start(&s));
        assert(lv_aic_player_session_close(&s));
    }
    failure=0; media.video_stream.width=0;
    assert(!lv_aic_player_session_open(&s,"/media/test.mp4")); media.video_stream.width=32;
    assert(lv_aic_player_session_open(&s,"/media/test.mp4"));
    assert(!lv_aic_player_session_open(&s,"/media/test.mp4"));
    assert(!lv_aic_player_session_acquire(&s,&lease,&frame));
    assert(!lv_aic_player_session_allocator(&s,NULL,3));
    assert(!lv_aic_player_session_allocator(&s,&allocator,0));
    assert(!lv_aic_player_session_allocator(&s,&allocator,9));
    int previous_controls=controls;
    assert(lv_aic_player_session_allocator(&s,&allocator,3) && controls==previous_controls+2);
    assert(lv_aic_player_session_start(&s)); int before=starts;
    assert(!lv_aic_player_session_allocator(&s,&allocator,3));
    assert(lv_aic_player_session_start(&s) && starts==before);
    assert(lv_aic_player_session_pause(&s,true)); before=pauses;
    assert(lv_aic_player_session_pause(&s,true) && pauses==before);
    assert(!lv_aic_player_session_acquire(&s,&lease,&frame));
    assert(lv_aic_player_session_pause(&s,false)); before=resumes;
    assert(lv_aic_player_session_pause(&s,false) && resumes==before);
    failure=GET; assert(!lv_aic_player_session_acquire(&s,&lease,&frame));
    assert(lease==99 && frame==(void *)(uintptr_t)1 && !s.held); failure=0;
    assert(lv_aic_player_session_acquire(&s,&lease,&frame)); assert(frame->pts==123);
    before=stops;
    assert(!lv_aic_player_session_stop(&s) && !lv_aic_player_session_close(&s));
    assert(!lv_aic_player_session_seek(&s,500) && stops==before && !seeks);
    old=lease; assert(lv_aic_player_session_release(&s,lease));
    assert(!lv_aic_player_session_release(&s,lease));
    assert(lv_aic_player_session_acquire(&s,&lease,&frame));
    assert(lease!=old && !lv_aic_player_session_release(&s,old));
    failure=PUT; assert(!lv_aic_player_session_release(&s,lease) && s.held && s.faulted);
    assert(!lv_aic_player_session_close(&s)); failure=0;
    assert(lv_aic_player_session_release(&s,lease));
    assert(!lv_aic_player_session_start(&s)); assert(lv_aic_player_session_close(&s));
    assert(lv_aic_player_session_open(&s,"/media/test.mp4"));
    assert(lv_aic_player_session_start(&s));
    assert(lv_aic_player_session_acquire(&s,&lease,&frame)); assert(lease>old);
    assert(!lv_aic_player_session_release(&s,old)); assert(lv_aic_player_session_release(&s,lease));
    assert(lv_aic_player_session_seek(&s,500000));
    assert(!lv_aic_player_session_seek(&s,1000001));
    assert(lv_aic_player_session_volume(&s,100)); assert(!lv_aic_player_session_volume(&s,101));
    int vol=22; assert(lv_aic_player_session_get_volume(&s,&vol) && vol==70);
    returned_volume=101; assert(!lv_aic_player_session_get_volume(&s,&vol) && vol==70);
    int64_t us=9; assert(lv_aic_player_session_time(&s,&us) && us==123456);
    position=-1; assert(!lv_aic_player_session_time(&s,&us) && us==123456);
    assert(lv_aic_player_session_stop(&s)); before=prepares;
    assert(lv_aic_player_session_start(&s) && prepares==before+1);
    failure=STOP; assert(!lv_aic_player_session_close(&s) && s.player);
    failure=DESTROY; assert(!lv_aic_player_session_close(&s) && s.player);
    failure=0; assert(lv_aic_player_session_close(&s)); assert(created==destroyed);
    const int faults[]={START,PAUSE,PLAY,SEEK};
    for(unsigned i=0;i<sizeof(faults)/sizeof(faults[0]);i++) {
        assert(lv_aic_player_session_open(&s,"/media/test.mp4"));
        if(faults[i]!=START) assert(lv_aic_player_session_start(&s));
        if(faults[i]==PLAY) assert(lv_aic_player_session_pause(&s,true));
        failure=faults[i];
        bool ok=failure==START ? lv_aic_player_session_start(&s) :
            failure==SEEK ? lv_aic_player_session_seek(&s,1) :
            lv_aic_player_session_pause(&s,failure==PAUSE);
        assert(!ok && s.faulted && s.player);
        failure=0; assert(lv_aic_player_session_close(&s));
    }
    /* Saturation must not dequeue a ninth frame. */
    assert(lv_aic_player_session_open(&s,"/media/test.mp4")); assert(lv_aic_player_session_start(&s));
    uint64_t tickets[LV_AIC_PLAYER_LEASES];
    for(unsigned i=0;i<LV_AIC_PLAYER_LEASES;i++) assert(lv_aic_player_session_acquire(&s,&tickets[i],&frame));
    before=gets; assert(!lv_aic_player_session_acquire(&s,&lease,&frame) && gets==before);
    for(unsigned i=0;i<LV_AIC_PLAYER_LEASES;i++) assert(lv_aic_player_session_release(&s,tickets[i]));
    assert(lv_aic_player_session_close(&s)); assert(created==destroyed);
    /* Duplicate decoder ID makes ownership ambiguous: quarantine, never put
     * or destroy either potentially shared frame while readers may exist. */
    assert(lv_aic_player_session_open(&s,"/media/test.mp4")); assert(lv_aic_player_session_start(&s));
    assert(lv_aic_player_session_acquire(&s,&lease,&frame)); frame_id=frame->id;
    old=lease; before=puts;
    assert(!lv_aic_player_session_acquire(&s,&lease,&frame) && lease==old);
    assert(s.quarantined && !lv_aic_player_session_release(&s,lease));
    assert(!lv_aic_player_session_close(&s) && puts==before && instance.live);
    return 0;
}
