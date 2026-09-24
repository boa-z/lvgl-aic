/**
 * @file lv_aic_mpp_decoder.c
 * @brief Phase 2 boundary: decoder registration without pixel output yet.
 *
 * This commit only establishes ownership, lifecycle, and the GE2D-free
 * boundary. info/open deliberately claim nothing so the Gate 1 SW baseline
 * cannot regress. JPEG bring-up fills the decode path next.
 *
 * LVGL 9.6 private decoder structs are accessed only through
 * compat/lvgl_aic_private.h (see docs/compatibility.md).
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "lv_aic_mpp_decoder.h"
#include "lv_aic_mpp_format.h"
#include "lv_aic_mpp_stream.h"

#if AIC_LVGL_USE_MPP_DEC && AIC_LVGL_BSP_MPP

#define AIC_LVGL_USE_PRIVATE_API 1
#include "lvgl_aic_private.h"

#include <string.h>

static lv_image_decoder_t *g_aic_mpp_decoder;
static lv_aic_mpp_decode_stats_t g_aic_mpp_last_stats;

static lv_result_t lv_aic_mpp_info_cb(lv_image_decoder_t *decoder,
                                      lv_image_decoder_dsc_t *dsc,
                                      lv_image_header_t *header)
{
    (void)decoder;
    (void)dsc;
    (void)header;
    /* Boundary: claim nothing yet. JPEG/PNG commits add FILE support. */
    return LV_RESULT_INVALID;
}

static lv_result_t lv_aic_mpp_open_cb(lv_image_decoder_t *decoder,
                                      lv_image_decoder_dsc_t *dsc)
{
    (void)decoder;
    (void)dsc;
    return LV_RESULT_INVALID;
}

static void lv_aic_mpp_close_cb(lv_image_decoder_t *decoder,
                                lv_image_decoder_dsc_t *dsc)
{
    (void)decoder;
    (void)dsc;
}

int lv_aic_mpp_decoder_init(lv_image_decoder_t **decoder)
{
    if (decoder == NULL) {
        return LV_AIC_ERR_INVALID_STATE;
    }
    *decoder = NULL;

    if (g_aic_mpp_decoder != NULL) {
        return LV_AIC_ERR_INVALID_STATE;
    }

    g_aic_mpp_decoder = lv_image_decoder_create();
    if (g_aic_mpp_decoder == NULL) {
        return LV_AIC_ERR_NO_MEMORY;
    }

    lv_image_decoder_set_info_cb(g_aic_mpp_decoder, lv_aic_mpp_info_cb);
    lv_image_decoder_set_open_cb(g_aic_mpp_decoder, lv_aic_mpp_open_cb);
    lv_image_decoder_set_close_cb(g_aic_mpp_decoder, lv_aic_mpp_close_cb);

    memset(&g_aic_mpp_last_stats, 0, sizeof(g_aic_mpp_last_stats));
    *decoder = g_aic_mpp_decoder;
    return LV_AIC_OK;
}

void lv_aic_mpp_decoder_deinit(lv_image_decoder_t *decoder)
{
    if (decoder == NULL || decoder != g_aic_mpp_decoder) {
        return;
    }
    lv_image_decoder_delete(decoder);
    g_aic_mpp_decoder = NULL;
}

const lv_aic_mpp_decode_stats_t *lv_aic_mpp_decoder_last_stats(void)
{
    return &g_aic_mpp_last_stats;
}

#else /* AIC_LVGL_USE_MPP_DEC && AIC_LVGL_BSP_MPP */

int lv_aic_mpp_decoder_init(lv_image_decoder_t **decoder)
{
    if (decoder != NULL) {
        *decoder = NULL;
    }
    return LV_AIC_ERR_NO_BSP;
}

void lv_aic_mpp_decoder_deinit(lv_image_decoder_t *decoder)
{
    (void)decoder;
}

#endif /* AIC_LVGL_USE_MPP_DEC && AIC_LVGL_BSP_MPP */
