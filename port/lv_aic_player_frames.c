/* SPDX-License-Identifier: Apache-2.0 */
#include "lvgl_aic_feature_config.h"
#if defined(AIC_LVGL_USE_PLAYER_SESSION) && AIC_LVGL_USE_PLAYER_SESSION
#include "lv_aic_player_frames.h"
#include "lv_aic_yuv_mpp.h"
#include "lv_aic_rgb_mpp.h"
#include <aic_osal.h>
typedef enum { EMPTY, BUILDING, READY, PUBLISHING, PUBLISHED, RETURNING, RELEASING } slot_state_t;
typedef struct {
    struct lv_aic_player_frames *owner;
    lv_aic_yuv_frame_t frame;
    lv_aic_rgb_frame_t rgb_frame;
    bool rgb;
    uint64_t sdk_lease, pin;
    int64_t pts;
    slot_state_t state;
} slot_t;
struct lv_aic_player_frames {
    aicos_mutex_t mutex;
    lv_aic_player_session_t *session;
    lv_aic_player_allocator_t *allocator;
    lv_aic_yuv_color_space_t space;
    bool closing, quarantined;
    slot_t slots[LV_AIC_PLAYER_LEASES];
};
static void lock(lv_aic_player_frames_t *f) { aicos_mutex_take(f->mutex,AICOS_WAIT_FOREVER); }
static void unlock(lv_aic_player_frames_t *f) { aicos_mutex_give(f->mutex); }
static bool retain(void *context)
{
    slot_t *s=context; lv_aic_player_frames_t *f=s->owner;
    lock(f);
    bool accepted=!f->closing && !f->quarantined && s->state==PUBLISHING;
    if(accepted) s->state=PUBLISHED;
    unlock(f); return accepted;
}
static void release(void *context)
{
    slot_t *s=context; lv_aic_player_frames_t *f=s->owner;
    lock(f); s->state=RETURNING; unlock(f);
}
lv_aic_player_frames_t *lv_aic_player_frames_create(lv_aic_player_session_t *session,
    lv_aic_player_allocator_t *allocator,lv_aic_yuv_color_space_t space)
{
    if(!session || !allocator || space<LV_AIC_YUV_BT601_LIMITED || space>LV_AIC_YUV_BT709_FULL) return NULL;
    lv_aic_player_frames_t *f=lv_malloc_zeroed(sizeof(*f));
    if(!f) return NULL;
    f->mutex=aicos_mutex_create();
    if(!f->mutex) { lv_free(f); return NULL; }
    f->session=session; f->allocator=allocator; f->space=space;
    for(unsigned i=0;i<LV_AIC_PLAYER_LEASES;i++) f->slots[i].owner=f;
    return f;
}
bool lv_aic_player_frames_submit(lv_aic_player_frames_t *f,uint64_t lease)
{
    if(!f || !lease) return false;
    const struct mpp_frame *frame=NULL;
    /* Session belongs exclusively to this worker; no UI access to its leases. */
    for(unsigned i=0;i<LV_AIC_PLAYER_LEASES;i++)
        if((f->session->held&(1U<<i)) && f->session->tickets[i]==lease) frame=&f->session->frames[i];
    if(!frame) return false;
    lock(f);
    for(unsigned i=0;i<LV_AIC_PLAYER_LEASES;i++)
        if(f->slots[i].state!=EMPTY && f->slots[i].sdk_lease==lease) { unlock(f); return true; }
    if(f->session->faulted || (frame->flags&FRAME_FLAG_ERROR)) { unlock(f); return false; }
    slot_t *s=NULL;
    if(!f->closing && !f->quarantined) for(unsigned i=0;i<LV_AIC_PLAYER_LEASES;i++)
        if(f->slots[i].state==EMPTY) { s=&f->slots[i]; s->sdk_lease=lease; s->state=BUILDING; break; }
    unlock(f);
    if(!s) return false;
    size_t capacity[3]; uint64_t pin=0;
    bool valid=lv_aic_player_allocator_acquire(f->allocator,frame,capacity,&pin);
    s->rgb=false;
    if(valid && !lv_aic_yuv_from_mpp(&frame->buf,capacity,f->space,&s->frame)) {
        valid=lv_aic_rgb_from_mpp(&frame->buf,capacity[0],&s->rgb_frame); s->rgb=valid;
    }
    if(!valid) {
        bool released=!pin || lv_aic_player_allocator_release(f->allocator,pin);
        lock(f);
        if(released) s->state=EMPTY;
        else { f->quarantined=true; s->pin=pin; s->sdk_lease=lease; }
        unlock(f);
        /* An impossible pin-release failure transfers the SDK lease too: the
         * caller must not recycle memory whose ownership is now uncertain. */
        return !released;
    }
    s->pin=pin; s->sdk_lease=lease; s->pts=frame->pts;
    lock(f);
    for(unsigned i=0;i<LV_AIC_PLAYER_LEASES;i++)
        if(f->slots[i].state==READY) f->slots[i].state=RETURNING;
    s->state=f->closing || f->quarantined ? RETURNING : READY;
    unlock(f); return true;
}
lv_aic_yuv_image_t *lv_aic_player_frames_poll(lv_aic_player_frames_t *f,int64_t *pts)
{
    if(!f || !pts) return NULL;
    slot_t *s=NULL;
    lock(f);
    if(!f->closing && !f->quarantined) for(unsigned i=0;i<LV_AIC_PLAYER_LEASES;i++)
        if(f->slots[i].state==READY && !f->slots[i].rgb) { s=&f->slots[i]; s->state=PUBLISHING; break; }
    unlock(f);
    if(!s) return NULL;
    lv_aic_yuv_image_t *image=lv_aic_yuv_image_create(&s->frame,retain,release,s);
    if(image) *pts=s->pts;
    else { lock(f); s->state=RETURNING; unlock(f); }
    return image;
}
bool lv_aic_player_frames_poll_image(lv_aic_player_frames_t *f,lv_aic_player_image_t *out)
{
    if(!f || !out || out->yuv || out->rgb) return false;
    slot_t *s=NULL;
    lock(f);
    if(!f->closing && !f->quarantined) for(unsigned i=0;i<LV_AIC_PLAYER_LEASES;i++)
        if(f->slots[i].state==READY) { s=&f->slots[i]; s->state=PUBLISHING; break; }
    unlock(f);
    if(!s) return false;
    lv_aic_player_image_t result={.pts=s->pts};
    if(s->rgb) result.rgb=lv_aic_rgb_image_create(&s->rgb_frame,retain,release,s);
    else result.yuv=lv_aic_yuv_image_create(&s->frame,retain,release,s);
    if(!result.rgb && !result.yuv) { lock(f); s->state=RETURNING; unlock(f); return false; }
    *out=result; return true;
}
const lv_image_dsc_t *lv_aic_player_image_source(const lv_aic_player_image_t *image)
{
    if(!image) return NULL;
    return image->rgb?lv_aic_rgb_image_source(image->rgb):lv_aic_yuv_image_source(image->yuv);
}
void lv_aic_player_image_destroy(lv_aic_player_image_t *image)
{
    if(!image) return;
    if(image->rgb) lv_aic_rgb_image_destroy(image->rgb);
    if(image->yuv) lv_aic_yuv_image_destroy(image->yuv);
    *image=(lv_aic_player_image_t){0};
}
bool lv_aic_player_frames_drain(lv_aic_player_frames_t *f)
{
    if(!f) return false;
    lock(f); bool quarantined=f->quarantined; unlock(f);
    if(quarantined) return false;
    bool success=true;
    for(unsigned i=0;i<LV_AIC_PLAYER_LEASES;i++) {
        slot_t *s=&f->slots[i];
        lock(f); bool give_back=s->state==RETURNING;
        if(give_back) s->state=RELEASING;
        unlock(f);
        if(!give_back) continue;
        if(s->pin && !lv_aic_player_allocator_release(f->allocator,s->pin)) {
            lock(f); f->quarantined=true; unlock(f); return false;
        }
        s->pin=0; /* A put retry must never repeat allocator release. */
        bool returned=lv_aic_player_session_release(f->session,s->sdk_lease);
        lock(f); s->state=returned?EMPTY:RETURNING; unlock(f);
        if(!returned) success=false;
    }
    return success;
}
void lv_aic_player_frames_close(lv_aic_player_frames_t *f)
{
    if(!f) return;
    lock(f); f->closing=true;
    for(unsigned i=0;i<LV_AIC_PLAYER_LEASES;i++)
        if(f->slots[i].state==READY) f->slots[i].state=RETURNING;
    unlock(f);
}
bool lv_aic_player_frames_idle(lv_aic_player_frames_t *f)
{
    if(!f) return true;
    lock(f); bool idle=!f->quarantined;
    for(unsigned i=0;i<LV_AIC_PLAYER_LEASES;i++) if(f->slots[i].state!=EMPTY) idle=false;
    unlock(f); return idle;
}
bool lv_aic_player_frames_destroy(lv_aic_player_frames_t *f)
{
    if(!f) return true;
    if(!f->closing || !lv_aic_player_frames_idle(f)) return false;
    aicos_mutex_delete(f->mutex); lv_free(f); return true;
}
#endif
