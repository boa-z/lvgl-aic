/* SPDX-License-Identifier: Apache-2.0 */
#include "lvgl_aic_feature_config.h"
#if (defined(AIC_LVGL_USE_APNG) && AIC_LVGL_USE_APNG) ||     (defined(AIC_LVGL_USE_PLAYER_SESSION) && AIC_LVGL_USE_PLAYER_SESSION)
#include "lv_aic_media_runtime.h"
#include <ve.h>
#include <aic_osal.h>
#include <limits.h>
#include <stddef.h>
/* Serialized owner-thread calls only. Never create SDK lazy state concurrently
 * on first-use workers; SDK decoders keep their own additional references. */
static unsigned users;
static aicos_mutex_t gate;
bool lv_aic_media_runtime_acquire(void)
{
    if(users==UINT_MAX) return false;
    if(!users) {
        gate=aicos_mutex_create();if(!gate) return false;
        if(ve_open_device()<0) { aicos_mutex_delete(gate);gate=NULL;return false; }
    }
    users++;return true;
}
void lv_aic_media_runtime_release(void)
{
    if(users && --users==0) {
        ve_close_device();aicos_mutex_delete(gate);gate=NULL;
    }
}
bool lv_aic_media_runtime_enter(void)
{ return gate && aicos_mutex_take(gate,AICOS_WAIT_FOREVER)>=0; }
void lv_aic_media_runtime_leave(void)
{ if(gate) aicos_mutex_give(gate); }
#endif
