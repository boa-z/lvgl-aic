/* SPDX-License-Identifier: Apache-2.0
 * Board-only finite decoder checks, run by the LVGL owner before the UI.
 * Does not claim pixel correctness; compare the final page on real hardware. */
#include "lv_aic_manual_test.h"
#if AIC_LVGL_USE_MPP_DEC && AIC_LVGL_BSP_RTTHREAD
#define AIC_LVGL_USE_PRIVATE_API 1
#include "lvgl_aic_private.h"
#include "lv_aic_mpp_decoder.h"
#include <rtthread.h>
#define LOG_TAG "lvgl.mpp.test"
#define LOG_LVL LOG_LVL_INFO
#include <ulog.h>

static bool fs_readable(const char *path)
{
    lv_fs_file_t file;

    if (lv_fs_open(&file, path, LV_FS_MODE_RD) != LV_FS_RES_OK) {
        return false;
    }
    lv_fs_close(&file);
    return true;
}

static int check_image(const char *path, bool expected, bool report)
{
    lv_image_decoder_dsc_t dsc = {0};
    lv_image_decoder_args_t args = {0};
    args.no_cache = true;
    lv_result_t result = lv_image_decoder_open(&dsc, path, &args);
    bool opened = result == LV_RESULT_OK;
    bool ours = opened && dsc.decoder == lv_aic_get_mpp_decoder() && dsc.decoded != NULL;
    if (opened) {
        if (report && ours) {
            const lv_aic_mpp_decode_stats_t *stats = lv_aic_mpp_decoder_last_stats();
            LOG_I("%s: %ux%u stride=%u ms=%u CMA=%u", path,
                  (unsigned)dsc.header.w, (unsigned)dsc.header.h,
                  (unsigned)dsc.decoded->header.stride,
                  (unsigned)stats->decode_time_ms, (unsigned)stats->cma_bytes);
        }
        lv_image_decoder_close(&dsc);
    }
    if (opened != expected || (expected && !ours)) {
        /* fs=0 means the file never reached the decoder: /data is not mounted
         * or the asset was not packed, so no decoder verdict can be drawn. */
        LOG_E("FAIL %s open=%d expected=%d mpp=%d", path, opened, expected, ours);
        return -1;
    }
    if (report) LOG_I("PASS %s", path);
    return 0;
}

int lv_aic_mpp_test_run(void)
{
    /* One table owns fixture presence and expected decode result. Low-depth
     * palette and Adam7 are SDK limitations, not successful pixel tests. */
    static const struct {
        const char *path;
        bool present;
        bool opens;
    } cases[] = {
        {"L:/data/mpp_test/a.jpg", true, true},
        {"L:/data/mpp_test/b.png", true, true},
        {"L:/data/mpp_test/c.png", true, true},
        {"L:/data/mpp_test/aic_800x480.jpg", true, true},
        {"L:/data/mpp_test/aic_801x479.jpg", true, true},
        {"L:/data/mpp_test/missing.png", false, false},
        {"L:/data/mpp_test/empty.jpg", true, false},
        {"L:/data/mpp_test/empty.png", true, false},
        {"L:/data/mpp_test/testimgp.jpg", true, false},
        {"L:/data/mpp_test/testimgari.jpg", true, false},
        {"L:/data/mpp_test/basn0g08.png", true, false},
        {"L:/data/mpp_test/bad_crc_rgb.png", true, false},
        {"L:/data/mpp_test/basi2c08.png", true, false},
        {"L:/data/mpp_test/s33n3p04.png", true, false},
        {"L:/data/mpp_test/s37n3p04.png", true, false}
    };
    int failures = 0;
    rt_uint32_t total, used_before, used_after, peak;
    LOG_I("BEGIN MPP checks; visual/CMA acceptance is separate");
    for (unsigned i = 0; i < sizeof(cases) / sizeof(cases[0]); i++) {
        if (fs_readable(cases[i].path) != cases[i].present) {
            LOG_E("BLOCKED fixture presence: %s", cases[i].path);
            return -2;
        }
    }
    for (unsigned i = 0; i < sizeof(cases) / sizeof(cases[0]); i++) {
        failures += check_image(cases[i].path, cases[i].opens, true) != 0;
    }
    if (failures) {
        LOG_E("FAIL acceptance cases=%d; stress skipped", failures);
        return -1;
    }
    /* Measure exactly the stress loop: the acceptance cases above already
     * balanced their own CMA, so this yields a clean 1000-cycle verdict. */
    lv_aic_mpp_cma_stats_reset();
    rt_memory_info(&total, &used_before, &peak);
    for (unsigned i = 0; i < 1000; i++) {
        if (check_image(cases[i % 3].path, true, false) != 0) return -1;
        if ((i + 1) % 100 == 0) LOG_I("stress %u/1000", i + 1);
        rt_thread_mdelay(1);
    }
    rt_memory_info(&total, &used_after, &peak);
    LOG_I("PASS 1000 decode/close cycles");
    LOG_I("heap before=%u after=%u peak=%u",
          (unsigned)used_before, (unsigned)used_after, (unsigned)peak);
    {
        const lv_aic_mpp_cma_stats_t *cma = lv_aic_mpp_cma_stats();
        LOG_I("CMA current=%u peak=%u alloc=%u free=%u",
              (unsigned)cma->current_cma_bytes, (unsigned)cma->peak_cma_bytes,
              (unsigned)cma->alloc_count, (unsigned)cma->free_count);
        if (cma->current_cma_bytes != 0U || cma->alloc_count != cma->free_count) {
            LOG_E("FAIL CMA lifecycle unbalanced current=%u alloc=%u free=%u",
                  (unsigned)cma->current_cma_bytes,
                  (unsigned)cma->alloc_count, (unsigned)cma->free_count);
            return -1;
        }
        if (cma->alloc_count < 1000U) {
            /* A bypassed allocator would make the balance check vacuous. */
            LOG_E("FAIL MPP allocator exercised only %u times", (unsigned)cma->alloc_count);
            return -1;
        }
    }
    LOG_I("PASS CMA lifecycle balanced across 1000 cycles");
    return 0;
}
#endif
