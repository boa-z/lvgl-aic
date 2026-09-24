/**
 * @file lv_aic_mpp_stream.c
 * @brief LVGL-FS stream helpers (FILE only).
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "lv_aic_mpp_stream.h"

lv_fs_res_t lv_aic_mpp_stream_open_file(lv_aic_mpp_stream_t *stream, const char *path)
{
    lv_fs_res_t res;
    uint32_t pos = 0U;

    if ((stream == NULL) || (path == NULL) || (path[0] == '\0')) {
        return LV_FS_RES_INV_PARAM;
    }

    stream->opened = false;
    stream->size = 0U;
    stream->cursor = 0U;

    res = lv_fs_open(&stream->file, path, LV_FS_MODE_RD);
    if (res != LV_FS_RES_OK) {
        return res;
    }

    /* lv_fs_seek to END is optional in some drivers; size is best-effort. */
    if (lv_fs_seek(&stream->file, 0U, LV_FS_SEEK_END) == LV_FS_RES_OK &&
        lv_fs_tell(&stream->file, &pos) == LV_FS_RES_OK) {
        stream->size = pos;
    }
    if (lv_fs_seek(&stream->file, 0U, LV_FS_SEEK_SET) != LV_FS_RES_OK) {
        lv_fs_close(&stream->file);
        return LV_FS_RES_FS_ERR;
    }

    stream->opened = true;
    stream->cursor = 0U;
    return LV_FS_RES_OK;
}

lv_fs_res_t lv_aic_mpp_stream_read(lv_aic_mpp_stream_t *stream, void *buf, uint32_t bytes,
                                   uint32_t *read_bytes)
{
    uint32_t done = 0U;
    lv_fs_res_t res;

    if ((stream == NULL) || !stream->opened || (buf == NULL && bytes != 0U)) {
        return LV_FS_RES_INV_PARAM;
    }
    if (bytes == 0U) {
        if (read_bytes != NULL) {
            *read_bytes = 0U;
        }
        return LV_FS_RES_OK;
    }

    res = lv_fs_read(&stream->file, buf, bytes, &done);
    if (res != LV_FS_RES_OK) {
        return res;
    }
    stream->cursor += done;
    if (read_bytes != NULL) {
        *read_bytes = done;
    }
    return LV_FS_RES_OK;
}

lv_fs_res_t lv_aic_mpp_stream_seek(lv_aic_mpp_stream_t *stream, uint32_t offset)
{
    lv_fs_res_t res;

    if ((stream == NULL) || !stream->opened) {
        return LV_FS_RES_INV_PARAM;
    }
    res = lv_fs_seek(&stream->file, offset, LV_FS_SEEK_SET);
    if (res != LV_FS_RES_OK) {
        return res;
    }
    stream->cursor = offset;
    return LV_FS_RES_OK;
}

void lv_aic_mpp_stream_close(lv_aic_mpp_stream_t *stream)
{
    if ((stream == NULL) || !stream->opened) {
        return;
    }
    lv_fs_close(&stream->file);
    stream->opened = false;
}

uint32_t lv_aic_mpp_stream_size(const lv_aic_mpp_stream_t *stream)
{
    if (stream == NULL) {
        return 0U;
    }
    return stream->size;
}

uint16_t lv_aic_mpp_stream_u16_be(const uint8_t *buf)
{
    return (uint16_t)(((uint16_t)buf[0] << 8) | (uint16_t)buf[1]);
}

uint32_t lv_aic_mpp_stream_u32_be(const uint8_t *buf)
{
    return ((uint32_t)buf[0] << 24) | ((uint32_t)buf[1] << 16) |
           ((uint32_t)buf[2] << 8) | (uint32_t)buf[3];
}
