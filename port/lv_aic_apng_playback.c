/* SPDX-License-Identifier: Apache-2.0 */
#include "lvgl_aic_feature_config.h"
#if defined(AIC_LVGL_USE_APNG) && AIC_LVGL_USE_APNG
#include "lv_aic_apng_playback.h"
#include "lv_aic_media_runtime.h"
#include "lv_aic_apng_stream.h"
#include "lv_aic_apng_frames.h"
#include <aic_osal.h>
#include <aic_time.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>
struct lv_aic_apng_playback {
    aicos_mutex_t mutex;
    lv_aic_apng_playback_options_t options;
    lv_aic_apng_playback_status_t status;
    lv_aic_apng_frames_t *frames;
    char path[128];
    bool closing,start,paused,preserve;
    uint32_t num,den;
};
/* Owner-thread reservations include finished workers with live readers. */
static unsigned instances;
static void lock(lv_aic_apng_playback_t *p) { aicos_mutex_take(p->mutex,AICOS_WAIT_FOREVER); }
static void unlock(lv_aic_apng_playback_t *p) { aicos_mutex_give(p->mutex); }
static uint64_t now(void *context) { (void)context;return aic_get_time_us(); }
static void fault(lv_aic_apng_playback_t *p)
{ lock(p);p->status.state=LV_AIC_APNG_FAULT;p->closing=true;p->status.restart_pending=false;unlock(p); }
static lv_aic_apng_stream_t *load(lv_aic_apng_playback_t *p)
{
    FILE *file=fopen(p->path,"rb");if(!file) return NULL;
    uint8_t *data=NULL;lv_aic_apng_stream_t *stream=NULL;
    if(fseek(file,0,SEEK_END)) goto end;
    long length=ftell(file);
    if(length<=0 || (uint64_t)length>p->options.limits.file_bytes || fseek(file,0,SEEK_SET)) goto end;
    data=malloc((size_t)length);if(!data) goto end;
    if(fread(data,1,(size_t)length,file)!=(size_t)length || fgetc(file)!=EOF || ferror(file)) goto end;
    /* Close file before opening a decoder owner: no cleanup failure can hide
     * an outstanding SDK frame on this preparation path. */
    if(fclose(file)) { file=NULL;goto end; } file=NULL;
    lv_aic_apng_stream_config_t c={.limits=p->options.limits,.cpu_budget=p->options.stream_budget,
        .frame_budget=p->options.cma_budget,.packet_limit=p->options.packet_limit,
        .minimum_delay_us=p->options.minimum_delay_us,.now_us=now};
    stream=lv_aic_apng_stream_open(data,(size_t)length,&c);
    if(stream) { lock(p);p->status.file_bytes=(uint64_t)length;unlock(p); }
end:
    if(file) fclose(file);
    free(data);return stream;
}
/* The SDK PNG codec ignores a failed ve_get_client timeout. Serialize our
 * APNG codec operations before reaching that finite-wait hardware lock. */
static bool stream_tick(lv_aic_apng_stream_t *stream,lv_aic_apng_stream_view_t *view)
{
    if(!lv_aic_media_runtime_enter()) return false;
    bool ok=lv_aic_apng_stream_tick(stream,view);
    lv_aic_media_runtime_leave();return ok;
}
static bool stream_close(lv_aic_apng_stream_t *stream)
{
    if(!lv_aic_media_runtime_enter()) return false;
    bool ok=lv_aic_apng_stream_close(stream);
    lv_aic_media_runtime_leave();return ok;
}
static void worker(void *argument)
{
    lv_aic_apng_playback_t *p=argument;
    lv_aic_apng_stream_t *stream=load(p);
    lv_aic_apng_stream_view_t view={0};bool pending=false;
    if(!stream || !lv_aic_apng_stream_pause(stream,true) || !stream_tick(stream,&view)) fault(p);
    else {
        lv_aic_apng_frames_t *frames=lv_aic_apng_frames_create(view.width,view.height,
            p->options.snapshots,p->options.snapshot_budget);
        lock(p);p->frames=frames;lv_aic_apng_frames_preserve(frames,p->preserve);p->status.width=view.width;p->status.height=view.height;
        if(!p->closing) p->status.state=LV_AIC_APNG_READY;
        unlock(p);if(!frames) fault(p);
    }
    uint32_t applied_num=1,applied_den=1;bool applied_pause=true;
    for(;;) {
        lock(p);bool closing=p->closing,start=p->start,paused=p->paused,restart=p->status.restart_pending;
        uint32_t num=p->num,den=p->den;bool preserve=p->preserve;unlock(p);
        if(closing) {
            lv_aic_apng_frames_close(p->frames);
            if(stream_close(stream)) {
                lock(p);p->status.restart_pending=false;
                if(p->status.state!=LV_AIC_APNG_FAULT) p->status.state=LV_AIC_APNG_CLOSED;
                p->status.finished=true;unlock(p);return;
            }
            aicos_msleep(5);continue;
        }
        if(restart) {
            if(!lv_aic_apng_stream_restart(stream)) { fault(p);continue; }
            lv_aic_apng_frames_discard(p->frames);pending=false;
            lock(p);p->status.restart_pending=false;p->status.restarts++;
            p->status.completed_plays=0;p->status.frame_index=0;unlock(p);
        }
        bool stop_clock=!start || paused;
        if((num!=applied_num || den!=applied_den) && !lv_aic_apng_stream_rate(stream,num,den)) { fault(p);continue; }
        applied_num=num;applied_den=den;
        if(stop_clock!=applied_pause && !lv_aic_apng_stream_pause(stream,stop_clock)) { fault(p);continue; }
        applied_pause=stop_clock;
        /* Keep the composed canvas intact until it can enter the mailbox.
         * A blocked mailbox must not advance the stream's disposal/timeline.
         * Rate/pause/replay/close above still run on every bounded iteration. */
        bool advance=!(preserve && (pending || lv_aic_apng_frames_blocked(p->frames)));
        if(advance) {
            if(!stream_tick(stream,&view)) { fault(p);continue; }
            if(view.step.action==LV_AIC_APNG_FRAME) pending=true;
        }
        lock(p);bool publish=!p->closing && !p->status.restart_pending;unlock(p);
        bool published=pending && publish && lv_aic_apng_frames_publish(p->frames,view.rgba,view.stride,view.bytes,view.sequence);
        if(published) pending=false;
        lock(p);
        p->status.rate_num=num;p->status.rate_den=den;
        p->status.composed=view.sequence;
        if(published) p->status.published++;
        p->status.frame_index=view.step.frame_index;p->status.completed_plays=view.step.completed_plays;
        if(!p->closing) p->status.state=view.step.action==LV_AIC_APNG_ENDED?LV_AIC_APNG_TERMINAL:
            !start?LV_AIC_APNG_READY:paused?LV_AIC_APNG_PLAYBACK_PAUSED:LV_AIC_APNG_PLAYING;
        unlock(p);
        /* Bounded sleeps keep commands responsive; late frames still advance
         * one by one with a yield. A final unpublished canvas is retried. */
        uint64_t us=view.step.action==LV_AIC_APNG_WAIT?view.step.wait_us:5000;
        uint32_t ms=us>5000?5:(uint32_t)((us+999)/1000);
        aicos_msleep(ms?ms:1);
    }
}
lv_aic_apng_playback_t *lv_aic_apng_playback_prepare(const char *path,const lv_aic_apng_playback_options_t *o)
{
    if(instances>=LV_AIC_APNG_PLAYBACK_INSTANCES || !path || !path[0] || strlen(path)>=128 || !o || !o->limits.file_bytes ||
       o->limits.file_bytes>LONG_MAX || !o->limits.frame_png_bytes || !o->limits.canvas_pixels || !o->limits.frames ||
       !o->stream_budget || !o->snapshot_budget || !o->cma_budget || o->packet_limit<256 ||
       o->packet_limit>INT_MAX-255U || o->snapshots<2 || o->snapshots>8 ||
       !o->minimum_delay_us || o->minimum_delay_us>1000000) return NULL;
    lv_aic_apng_playback_t *p=calloc(1,sizeof(*p));if(!p) return NULL;
    p->mutex=aicos_mutex_create();if(!p->mutex) { free(p);return NULL; }
    if(!lv_aic_media_runtime_acquire()) { aicos_mutex_delete(p->mutex);free(p);return NULL; }
    p->options=*o;memcpy(p->path,path,strlen(path)+1);p->num=p->den=1;
    p->status=(lv_aic_apng_playback_status_t){.state=LV_AIC_APNG_OPENING,.rate_num=1,.rate_den=1};instances++;
    if(!aicos_thread_create("aic_apng",8192,20,worker,p)) {
        instances--;lv_aic_media_runtime_release();aicos_mutex_delete(p->mutex);free(p);return NULL;
    }
    return p;
}
bool lv_aic_apng_playback_preserve(lv_aic_apng_playback_t *p,bool enabled)
{
    if(!p) return false;
    lock(p);bool ok=!p->closing && !p->status.finished;
    if(ok) { p->preserve=enabled;lv_aic_apng_frames_preserve(p->frames,enabled); }
    unlock(p);return ok;
}
bool lv_aic_apng_playback_start(lv_aic_apng_playback_t *p)
{
    if(!p) return false;
    lock(p);bool ok=!p->closing;
    if(ok) p->start=true;
    unlock(p);return ok;
}
bool lv_aic_apng_playback_pause(lv_aic_apng_playback_t *p,bool paused)
{
    if(!p) return false;
    lock(p);bool ok=!p->closing;if(ok) p->paused=paused;unlock(p);return ok;
}
bool lv_aic_apng_playback_rate(lv_aic_apng_playback_t *p,uint32_t n,uint32_t d)
{
    if(!p || !n || !d || n>1000000 || d>1000000 || (uint64_t)n*10<d || (uint64_t)d*10<n) return false;
    lock(p);bool ok=!p->closing;if(ok) { p->num=n;p->den=d; } unlock(p);return ok;
}
bool lv_aic_apng_playback_restart(lv_aic_apng_playback_t *p)
{
    if(!p) return false;
    lock(p);bool ok=!p->closing && p->status.state!=LV_AIC_APNG_OPENING &&
        !p->status.restart_pending && p->status.restarts<UINT64_MAX;
    if(ok) p->status.restart_pending=true;
    unlock(p);return ok;
}
bool lv_aic_apng_playback_poll(lv_aic_apng_playback_t *p,lv_aic_rgb_image_t **image,uint64_t *sequence)
{
    if(!p) return false;
    lock(p);bool ok=!p->closing && !p->status.restart_pending &&
        lv_aic_apng_frames_poll(p->frames,image,sequence);unlock(p);return ok;
}
lv_aic_apng_playback_status_t lv_aic_apng_playback_status(lv_aic_apng_playback_t *p)
{
    if(!p) return (lv_aic_apng_playback_status_t){.state=LV_AIC_APNG_CLOSED,.finished=true};
    lock(p);lv_aic_apng_playback_status_t s=p->status;unlock(p);return s;
}
void lv_aic_apng_playback_close(lv_aic_apng_playback_t *p)
{
    if(!p) return;
    lock(p);p->closing=true;
    if(!p->status.finished && p->status.state!=LV_AIC_APNG_FAULT) p->status.state=LV_AIC_APNG_CLOSING;
    unlock(p);
}
bool lv_aic_apng_playback_destroy(lv_aic_apng_playback_t *p)
{
    if(!p) return true;
    if(!lv_aic_apng_playback_status(p).finished || !lv_aic_apng_frames_destroy(p->frames)) return false;
    aicos_mutex_delete(p->mutex);instances--;lv_aic_media_runtime_release();free(p);return true;
}
#endif
