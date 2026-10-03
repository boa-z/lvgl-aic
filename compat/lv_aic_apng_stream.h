/* SPDX-License-Identifier: Apache-2.0 */
#ifndef LV_AIC_APNG_STREAM_H
#define LV_AIC_APNG_STREAM_H
#include "lv_aic_apng.h"
#include "lv_aic_apng_timeline.h"
typedef struct lv_aic_apng_stream lv_aic_apng_stream_t;
typedef struct {
    lv_aic_apng_limits_t limits;
    size_t cpu_budget,frame_budget,packet_limit;
    uint32_t minimum_delay_us;
    uint64_t (*now_us)(void *context);
    void *clock_context;
} lv_aic_apng_stream_config_t;
typedef struct {
    lv_aic_apng_step_t step;
    const uint8_t *rgba;
    uint32_t width,height;
    size_t stride,bytes;
    uint64_t sequence;
} lv_aic_apng_stream_view_t;
/* Worker-owned serialized stream. Copies immutable PNG/APNG bytes at open;
 * bounds its context/file/extracted PNG/rectangle/canvas/PREVIOUS allocations
 * with cpu_budget (excluding malloc overhead and decoder/allocator bookkeeping).
 * frame_budget and packet_limit are passed to the bounded MPP decoder.
 * Clock must be monotonic; read again after blocking decode/composition. */
lv_aic_apng_stream_t *lv_aic_apng_stream_open(const void *data,size_t bytes,
    const lv_aic_apng_stream_config_t *config);
/* Decode/compose at most one frame. Every FRAME is composed, even when late.
 * A FRAME view is borrowed only until the next tick/restart/close: copy into an
 * immutable publication before the UI/GE consumes it. Non-FRAME views retain
 * the last canvas, or NULL before the first frame. Never hand this canvas to
 * an asynchronous reader. Output is unchanged on false; decode/clock failure
 * latches a fault and requires close. Failed cleanup remains retryable. */
bool lv_aic_apng_stream_tick(lv_aic_apng_stream_t *s,lv_aic_apng_stream_view_t *view);
bool lv_aic_apng_stream_pause(lv_aic_apng_stream_t *s,bool paused);
bool lv_aic_apng_stream_rate(lv_aic_apng_stream_t *s,uint32_t numerator,uint32_t denominator);
/* Replay from frame zero preserving pause/rate; next FRAME resets the canvas.
 * Sequence numbers remain monotonic across restart. Cannot clear a fault. */
bool lv_aic_apng_stream_restart(lv_aic_apng_stream_t *s);
bool lv_aic_apng_stream_close(lv_aic_apng_stream_t *s);
#endif
