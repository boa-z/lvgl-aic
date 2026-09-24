/**
 * @file lv_aic_mpp_stream.h
 * @brief Minimal LVGL-FS stream helpers for MPP header/packet feeding.
 *
 * Phase 2A only needs FILE sources. VARIABLE/AICP/BMP/fake paths are out of
 * scope and must return errors rather than partial behavior.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef LV_AIC_MPP_STREAM_H
#define LV_AIC_MPP_STREAM_H

#include "lvgl_aic.h"
#include "lvgl_aic_compat.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    lv_fs_file_t file;
    bool opened;
    uint32_t size;
    uint32_t cursor;
} lv_aic_mpp_stream_t;

lv_fs_res_t lv_aic_mpp_stream_open_file(lv_aic_mpp_stream_t *stream, const char *path);
lv_fs_res_t lv_aic_mpp_stream_read(lv_aic_mpp_stream_t *stream, void *buf, uint32_t bytes,
                                   uint32_t *read_bytes);
lv_fs_res_t lv_aic_mpp_stream_seek(lv_aic_mpp_stream_t *stream, uint32_t offset);
void lv_aic_mpp_stream_close(lv_aic_mpp_stream_t *stream);
uint32_t lv_aic_mpp_stream_size(const lv_aic_mpp_stream_t *stream);

uint16_t lv_aic_mpp_stream_u16_be(const uint8_t *buf);
uint32_t lv_aic_mpp_stream_u32_be(const uint8_t *buf);

#ifdef __cplusplus
}
#endif

#endif /* LV_AIC_MPP_STREAM_H */
