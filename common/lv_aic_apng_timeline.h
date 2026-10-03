/* SPDX-License-Identifier: Apache-2.0 */
#ifndef LV_AIC_APNG_TIMELINE_H
#define LV_AIC_APNG_TIMELINE_H
#include <stdbool.h>
#include <stdint.h>
/* Serialized worker-side APNG clock. No sleeping or automatic frame skipping:
 * every requested frame must be decoded/composed in order for disposal.
 * Caller may omit intermediate publication when catching up, but not compose.
 * Microsecond monotonic wall time; rational rate num/den in [0.1,10], each
 * operand <=1,000,000. Submicrosecond deadline precision uses 32 fraction bits.
 * Explicit nonzero minimum frame delay prevents zero-delay busy loops. */
typedef struct {
    uint64_t last_us,media_us,deadline_us,completed_plays;
    uint32_t rate_num,rate_den,media_fraction,deadline_fraction;
    uint32_t frames,plays,index,minimum_delay_us;
    bool initialized,animated,started,awaiting_frame,paused,ended;
} lv_aic_apng_timeline_t;
typedef enum { LV_AIC_APNG_FRAME,LV_AIC_APNG_WAIT,LV_AIC_APNG_PAUSED,LV_AIC_APNG_ENDED } lv_aic_apng_action_t;
typedef struct {
    lv_aic_apng_action_t action;
    uint32_t frame_index;
    uint64_t completed_plays,wait_us;
    bool reset_canvas; /* First frame of every play starts on transparency. */
} lv_aic_apng_step_t;
/* plays=0 is infinite for APNG; a static PNG ends after its single commit.
 * A finite animation ends only after displaying its final frame for its delay.
 * Init/reset begins at frame zero and clears play count/pause/rate (1x). */
bool lv_aic_apng_timeline_init(lv_aic_apng_timeline_t *clock,uint32_t frames,uint32_t plays,
    bool animated,uint32_t minimum_delay_us,uint64_t now_us);
bool lv_aic_apng_timeline_poll(lv_aic_apng_timeline_t *clock,uint64_t now_us,lv_aic_apng_step_t *step);
/* Commit the currently requested decoded/composed frame at presentation time.
 * delay_den=0 means 100; numerator=0 uses minimum delay. Failure is transactional.
 * Slow decode retains the timeline; subsequent frames may immediately be due. */
bool lv_aic_apng_timeline_commit(lv_aic_apng_timeline_t *clock,uint64_t now_us,uint16_t delay_num,uint16_t delay_den);
bool lv_aic_apng_timeline_pause(lv_aic_apng_timeline_t *clock,uint64_t now_us,bool paused);
bool lv_aic_apng_timeline_rate(lv_aic_apng_timeline_t *clock,uint64_t now_us,uint32_t numerator,uint32_t denominator);
#endif
