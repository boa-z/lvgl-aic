/* SPDX-License-Identifier: Apache-2.0
 * Manual-test-only UART snapshot. The shell never calls LVGL. */
#include "lv_aic_manual_test.h"
#if AIC_LVGL_BSP_RTTHREAD && AIC_LVGL_BSP_MPP
#include "../../port/lv_aic_display.h"
#include <rtthread.h>
#include <rthw.h>
#include <finsh.h>
#include <string.h>

enum capture_state { IDLE, REQUESTED, COPYING, READY, DUMPING };
static volatile enum capture_state state;
static lv_draw_buf_t snapshot;
static uint32_t frame_number;

/* Single-core D13x: IRQ exclusion publishes ownership, not the slow copy. */
static bool claim(enum capture_state from, enum capture_state to)
{
    rt_base_t level = rt_hw_interrupt_disable();
    bool ok = state == from;
    if (ok) state = to;
    rt_hw_interrupt_enable(level);
    return ok;
}

void lv_aic_capture_poll(void)
{
    int result;
    if (!claim(REQUESTED, COPYING)) return;
    result = lv_aic_display_snapshot(lv_display_get_default(), &snapshot, &frame_number);
    if (result == LV_AIC_OK) {
        claim(COPYING, READY);
        rt_kprintf("capture ready: %ux%u frame=%u; run lv_aic_capture dump\n",
                   snapshot.header.w, snapshot.header.h, frame_number);
    } else {
        claim(COPYING, IDLE);
        rt_kprintf("capture failed: %d\n", result);
    }
}

static uint32_t crc_byte(uint32_t crc, uint8_t byte)
{
    crc ^= byte;
    for (unsigned bit = 0; bit < 8; bit++)
        crc = (crc >> 1) ^ ((0U - (crc & 1U)) & 0xedb88320U);
    return crc;
}

/* Six RLE tokens per line keeps rt_kprintf below RT_CONSOLEBUF_SIZE=128.
 * Each token is four count hex digits followed by native pixel byte hex.
 * No stride padding is exported. Line sequence + CRC detects lost UART data. */
static void dump_snapshot(void)
{
    static const char hex[] = "0123456789abcdef";
    const char *format;
    unsigned bpp, seq = 0, used = 0, tokens = 0;
    uint32_t crc = ~0U;
    char line[80];
    switch (snapshot.header.cf) {
    case LV_COLOR_FORMAT_RGB565: format = "RGB565"; bpp = 2; break;
    case LV_COLOR_FORMAT_RGB888: format = "RGB888"; bpp = 3; break;
    case LV_COLOR_FORMAT_ARGB8888: format = "ARGB8888"; bpp = 4; break;
    case LV_COLOR_FORMAT_XRGB8888: format = "XRGB8888"; bpp = 4; break;
    default: rt_kprintf("capture failed: unsupported format\n"); return;
    }
    rt_kprintf("AICCAP BEGIN 1 %u %u %s frame=%u\n",
               snapshot.header.w, snapshot.header.h, format, frame_number);
    for (unsigned y = 0; y < snapshot.header.h; y++) {
        const uint8_t *row = snapshot.data + y * snapshot.header.stride;
        for (unsigned x = 0; x < snapshot.header.w;) {
            unsigned run = 1;
            const uint8_t *pixel = row + x * bpp;
            while (x + run < snapshot.header.w && run < 65535 &&
                   memcmp(pixel, pixel + run * bpp, bpp) == 0) run++;
            for (unsigned i = 0; i < run * bpp; i++) crc = crc_byte(crc, pixel[i]);
            if (tokens) line[used++] = ' ';
            for (int shift = 12; shift >= 0; shift -= 4)
                line[used++] = hex[(run >> shift) & 15];
            for (unsigned i = 0; i < bpp; i++) {
                line[used++] = hex[pixel[i] >> 4];
                line[used++] = hex[pixel[i] & 15];
            }
            x += run;
            if (++tokens == 6) {
                line[used] = 0;
                rt_kprintf("AICCAP DATA %u %s\n", seq++, line);
                used = tokens = 0;
                /* Allow interactive LVGL/touch tasks to progress during UART. */
                rt_thread_mdelay(1);
            }
        }
    }
    if (tokens) {
        line[used] = 0;
        rt_kprintf("AICCAP DATA %u %s\n", seq++, line);
    }
    rt_kprintf("AICCAP END lines=%u crc32=%08x\n", seq, (unsigned)~crc);
}

static int lv_aic_capture(int argc, char **argv)
{
    if (argc == 1) {
        if (claim(IDLE, REQUESTED)) rt_kprintf("capture requested; wait for capture ready\n");
        else rt_kprintf("capture pending/ready; use dump or free\n");
        return 0;
    }
    if (argc == 2 && strcmp(argv[1], "dump") == 0) {
        if (!claim(READY, DUMPING)) {
            rt_kprintf("capture not ready\n");
            return -1;
        }
        dump_snapshot();
        lv_aic_display_snapshot_free(&snapshot);
        claim(DUMPING, IDLE);
        return 0;
    }
    if (argc == 2 && strcmp(argv[1], "free") == 0) {
        if (claim(REQUESTED, IDLE)) return 0;
        if (claim(READY, DUMPING)) {
            lv_aic_display_snapshot_free(&snapshot);
            claim(DUMPING, IDLE);
            return 0;
        }
        rt_kprintf("capture idle or busy\n");
        return -1;
    }
    rt_kprintf("Usage: lv_aic_capture [dump|free]\n");
    return -1;
}
MSH_CMD_EXPORT(lv_aic_capture, Capture presented frame then dump over UART);
#endif
