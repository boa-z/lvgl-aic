/* SPDX-License-Identifier: Apache-2.0 */
#include "lvgl_aic_feature_config.h"
#if (defined(AIC_LVGL_USE_APNG) && AIC_LVGL_USE_APNG) || (defined(AIC_LVGL_USE_PLAYER_SESSION) && AIC_LVGL_USE_PLAYER_SESSION)
#include <aic_osal.h>
#include <ve.h>
/* GNU ld --wrap redirects SDK codec calls without modifying SDK sources.
 * SDK PNG/JPEG/H264/zlib/encoder callers ignore arbitration failure. Never
 * return to those register writers until the real driver owns the client.
 * A permanently unavailable device stalls its worker, preserving resources;
 * do not fabricate success, terminate that worker or recycle its DMA buffers.
 * SDK owns release and its normal hardware timeout/reset policy. */
int __real_ve_get_client(void);
int __wrap_ve_get_client(void)
{
    while(__real_ve_get_client()!=0) aicos_msleep(5);
    return 0;
}
#endif
