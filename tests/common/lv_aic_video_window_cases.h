/* SPDX-License-Identifier: Apache-2.0 */
#ifndef LV_AIC_VIDEO_WINDOW_CASES_H
#define LV_AIC_VIDEO_WINDOW_CASES_H
/* 16x8 pixels, pivot (0,0), origin (32,32). Inclusive pixel-center bounds. */
static const lv_area_t video_window_rotated_bounds[]={
    {32,32,47,39},{25,32,32,47},{17,25,32,32},{32,17,39,32}
};
/* Transposed 8x16 source exercises the rightmost extent at 270 degrees. */
static const lv_area_t video_window_portrait_bounds[]={
    {32,32,39,47},{17,32,32,39},{25,17,32,32},{32,25,47,32}
};
#endif
