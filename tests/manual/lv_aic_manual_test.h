/**
 * @file lv_aic_manual_test.h
 * @brief Optional platform-only manual test page.
 */

#ifndef LV_AIC_MANUAL_TEST_H
#define LV_AIC_MANUAL_TEST_H

#include "lvgl_aic.h"
#include "lv_aic_native_widgets_test.h"

#ifdef __cplusplus
extern "C" {
#endif

int lv_aic_manual_test_create(void);
/* Zero-based pages; current returns -1 while the test UI is not created. */
int lv_aic_manual_page_count(void);
int lv_aic_manual_page_current(void);
void lv_aic_manual_page_request(int page);
void lv_aic_manual_page_poll(void);
#if AIC_LVGL_BSP_RTTHREAD && AIC_LVGL_BSP_MPP
void lv_aic_capture_poll(void);
#endif
#if AIC_LVGL_USE_MPP_DEC
int lv_aic_mpp_test_run(void);
int lv_aic_mpp_resource_test_run(void);
#endif
#if AIC_LVGL_USE_GE2D
int lv_aic_ge2d_test_run(void);
#if LV_USE_VECTOR_GRAPHIC && AIC_LVGL_BSP_RTTHREAD
int lv_aic_vector_surface_test_run(void);
#endif
#if LV_USE_SVG && AIC_LVGL_BSP_RTTHREAD
int lv_aic_svg_document_test_run(void);
#endif
#if AIC_LVGL_USE_VIDEO_WINDOW && AIC_LVGL_BSP_RTTHREAD
int lv_aic_video_window_test_run(void);
#endif
/* Returns 0 when every fill probe passed, 1 when only the RGB565 color-key
 * measurement was inconclusive (engine healthy; production key disabled) and
 * -1 on a hard probe failure. */
int lv_aic_ge2d_fill_test_run(void);
#if AIC_LVGL_USE_CANVAS
int lv_aic_native_fill_test_run(void);
#endif
int lv_aic_ge2d_scale_test_run(void);
int lv_aic_yuv_test_run(void);
#endif
void lv_aic_manual_test_deinit(void);
const char *lv_aic_manual_test_status_text(void);

#ifdef __cplusplus
}
#endif

#endif /* LV_AIC_MANUAL_TEST_H */
