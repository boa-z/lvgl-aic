/* SPDX-License-Identifier: Apache-2.0 */
#ifndef LV_AIC_VIDEO_PLANE_H
#define LV_AIC_VIDEO_PLANE_H
#include "lvgl.h"
#ifdef __cplusplus
extern "C" {
#endif
typedef struct lv_aic_video_plane lv_aic_video_plane_t;
/* LVGL owner-thread only. One explicit owner; refuses an already enabled SDK
 * video layer. Do not use unmanaged SDK video-layer writers concurrently.
 * Does not alter UI alpha, create a transparent window, or rotate coordinates. */
lv_aic_video_plane_t *lv_aic_video_plane_open(void);
/* Optional, before first present: ARGB8888 UI pixel-alpha lease. Saves exact
 * SDK UI alpha config, applies pixel alpha, restores on successful close.
 * No unmanaged alpha writers while held. Failed apply/restore keeps ownership;
 * retry close, never discard the handle. hide preserves the alpha lease. */
bool lv_aic_video_plane_enable_ui_alpha(lv_aic_video_plane_t *plane);
/* Native immutable RGB/YUV image source only. Physical screen rectangle,
 * entirely on screen; no rotation/crop. Producer must supply DMA-accessible,
 * CPU-coherent padded rows. A native reader is retained until scanout retires.
 * False after an ioctl attempt quarantines BOTH old and new frames: only
 * hide/close retries are accepted. False never means safe to reclaim pixels. */
bool lv_aic_video_plane_present(lv_aic_video_plane_t *plane,const void *source,
                                int32_t x,int32_t y,uint32_t width,uint32_t height);
/* Clockwise 0/90/180/270 degrees; nonzero requires GE2D and source >=8x8.
 * Explicit CMA budget includes BOTH the current and next ARGB8888 copy,
 * padded to 64-byte rows. DE scales the rotated result to the given rectangle.
 * Budget/allocation/preflight failure leaves old scanout intact and is retryable.
 * GE submit/emit/sync uncertainty permanently pins source, destination and old
 * scanout: hide/close return false until reboot. There is no unsafe force-free. */
bool lv_aic_video_plane_present_rotated(lv_aic_video_plane_t *plane,const void *source,
    int32_t x,int32_t y,uint32_t width,uint32_t height,unsigned degrees,size_t cma_budget);
bool lv_aic_video_plane_faulted(const lv_aic_video_plane_t *plane);
/* Disable + two successful VSync waits before releasing readers. Failure keeps
 * owner/storage alive. Retry from owner thread; never force-free on timeout. */
bool lv_aic_video_plane_hide(lv_aic_video_plane_t *plane);
bool lv_aic_video_plane_close(lv_aic_video_plane_t *plane);
#ifdef __cplusplus
}
#endif
#endif
