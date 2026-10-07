/* SPDX-License-Identifier: Apache-2.0 */
/* Framebuffer streaming over classic CAN for fast screenshots.
 * The shell only queues a request; the snapshot runs on the LVGL owner
 * thread via lv_aic_can_capture_poll(), and a worker streams RLE frames
 * so the UI thread never blocks on the bus. The bus needs a second node
 * (the PC adapter) for ACK; see docs/can-capture-stage.md. */
#include "lv_aic_can_capture.h"
#if defined(AIC_LVGL_USE_CAN_CAPTURE) && AIC_LVGL_USE_CAN_CAPTURE
#include "lv_aic_manual_test.h"
#include "../../port/lv_aic_display.h"
#include <rtdevice.h>

#if AIC_LVGL_BSP_RTTHREAD
#include <rtthread.h>
#include <rthw.h>
#include <finsh.h>
#include <stdio.h>
#include <string.h>

enum can_cap_state { CAP_IDLE, CAP_REQUESTED, CAP_STREAMING };

static volatile enum can_cap_state cap_state;
static volatile bool cap_start_pending;
static volatile bool cap_ready;
static char cap_devname[16];
static lv_draw_buf_t cap_snapshot;
static uint32_t cap_frame;
static volatile bool cap_abort;
static uint32_t cap_retries;

static uint32_t cap_crc_byte(uint32_t crc, uint8_t byte)
{
    crc ^= byte;
    for (unsigned bit = 0; bit < 8; bit++) {
        crc = (crc >> 1) ^ ((0U - (crc & 1U)) & 0xedb88320U);
    }
    return crc;
}

/* data = seq(2) a(2) b0 b1 b2 flags; a/count/width/height per header.
 * Every frame (BEGIN/DATA/END) goes through here: hdr=-1 under HDR mode,
 * and a bounded retry because the TX mailbox holds a single frame. */
static int cap_send(rt_device_t can, uint16_t seq, uint16_t a, uint8_t b0,
                    uint8_t b1, uint8_t b2, uint8_t flags)
{
    struct rt_can_msg msg;
    memset(&msg, 0, sizeof(msg));
    msg.id = LV_AIC_CAN_CAPTURE_ID;
    msg.ide = RT_CAN_STDID;
    msg.rtr = RT_CAN_DTR;
    msg.len = 8;
    msg.hdr = -1;
    msg.data[0] = (uint8_t)(seq & 0xFF);
    msg.data[1] = (uint8_t)((seq >> 8) & 0xFF);
    msg.data[2] = (uint8_t)(a & 0xFF);
    msg.data[3] = (uint8_t)((a >> 8) & 0xFF);
    msg.data[4] = b0;
    msg.data[5] = b1;
    msg.data[6] = b2;
    msg.data[7] = flags;
    for (unsigned attempt = 0; attempt < 8u; attempt++) {
        if (rt_device_write(can, 0, &msg, sizeof(msg)) == sizeof(msg)) {
            return 0;
        }
        cap_retries++;
        rt_thread_mdelay(2);
    }
    return -1;
}

/* Same RLE/CRC as the UART capture: count + native pixel bytes. */
static void cap_stream(void *parameter)
{
    rt_device_t can = (rt_device_t)parameter;
    unsigned bpp = 0;
    uint32_t crc = ~0U;
    /* 16-bit wire sequence wraps modulo 65536 (a busy 800x480 frame
     * exceeds 65535 runs); the host tracks it the same way. */
    uint16_t seq = 1;
    uint32_t frames = 0;
    uint8_t fmt = 0;
    rt_tick_t start = rt_tick_get();
    uint16_t h = (uint16_t)cap_snapshot.header.h;

    cap_retries = 0;
    switch (cap_snapshot.header.cf) {
    case LV_COLOR_FORMAT_RGB565: bpp = 2; fmt = 0; break;
    case LV_COLOR_FORMAT_RGB888: bpp = 3; fmt = 1; break;
    default:
        rt_kprintf("can capture failed: unsupported format\n");
        goto done;
    }
    if (cap_send(can, 0, (uint16_t)cap_snapshot.header.w, (uint8_t)(h & 0xFF),
                 (uint8_t)(h >> 8), fmt, LV_AIC_CAN_CAP_BEGIN) != 0) {
        rt_kprintf("can capture failed: begin not acked; check adapter/bitrate\n");
        goto done;
    }
    for (unsigned y = 0; y < cap_snapshot.header.h && !cap_abort; y++) {
        const uint8_t *row = cap_snapshot.data + y * cap_snapshot.header.stride;
        for (unsigned x = 0; x < cap_snapshot.header.w && !cap_abort;) {
            unsigned run = 1;
            const uint8_t *pixel = row + x * bpp;
            while (x + run < cap_snapshot.header.w && run < 65535 &&
                   memcmp(pixel, pixel + run * bpp, bpp) == 0) {
                run++;
            }
            for (unsigned i = 0; i < run * bpp; i++) {
                crc = cap_crc_byte(crc, pixel[i]);
            }
            if (cap_send(can, seq++, (uint16_t)run, pixel[0], pixel[1],
                         bpp > 2 ? pixel[2] : 0, LV_AIC_CAN_CAP_DATA) != 0) {
                rt_kprintf("can capture failed at frame %u: bus error\n",
                           (unsigned)seq - 1);
                goto done;
            }
            frames++;
            x += run;
            if ((frames & 63) == 0) {
                rt_thread_mdelay(1);
            }
        }
    }
    if (cap_abort) {
        rt_kprintf("can capture aborted after %u frames\n", frames);
        goto done;
    }
    /* END carries the finalized CRC32 (zlib.crc32 of the raw pixels) and
     * the data frame count modulo 65536. */
    crc = ~crc;
    if (cap_send(can, (uint16_t)frames, (uint16_t)(crc & 0xFFFF),
                 (uint8_t)((crc >> 16) & 0xFF), (uint8_t)((crc >> 24) & 0xFF), 0,
                 LV_AIC_CAN_CAP_END) == 0) {
        rt_kprintf("can capture done: %u frames %ums crc=%08x retries=%u\n",
                   (unsigned)frames, (unsigned)(rt_tick_get() - start),
                   (unsigned)crc, (unsigned)cap_retries);
    } else {
        rt_kprintf("can capture failed: end not acked\n");
    }
done:
    rt_device_close(can);
    lv_aic_display_snapshot_free(&cap_snapshot);
    {
        rt_base_t level = rt_hw_interrupt_disable();
        cap_state = CAP_IDLE;
        rt_hw_interrupt_enable(level);
    }
}

void lv_aic_can_capture_poll(void)
{
    rt_device_t can;
    rt_thread_t thread;
    bool start = false;
    rt_base_t level = rt_hw_interrupt_disable();
    cap_ready = true;
    if (cap_start_pending && cap_state == CAP_REQUESTED) {
        cap_start_pending = false;
        cap_state = CAP_STREAMING;
        start = true;
    }
    rt_hw_interrupt_enable(level);
    if (!start) {
        return;
    }
    if (lv_aic_display_snapshot(lv_display_get_default(), &cap_snapshot,
                                &cap_frame) != LV_AIC_OK) {
        rt_kprintf("can capture failed: snapshot\n");
        level = rt_hw_interrupt_disable();
        cap_state = CAP_IDLE;
        rt_hw_interrupt_enable(level);
        return;
    }
    can = rt_device_find(cap_devname[0] ? cap_devname : "can0");
    if (can == RT_NULL) {
        rt_kprintf("can capture failed: no %s device\n",
                   cap_devname[0] ? cap_devname : "can0");
        lv_aic_display_snapshot_free(&cap_snapshot);
        level = rt_hw_interrupt_disable();
        cap_state = CAP_IDLE;
        rt_hw_interrupt_enable(level);
        return;
    }
    if (!(can->open_flag & RT_DEVICE_OFLAG_OPEN)) {
        /* Configure only an idle controller: the CAN OTA endpoint may own
         * can0 at the same rate, and re-tuning it would reset a live bus.
         * NOTE: SET_BAUD takes the rate as a pointer-sized VALUE, not a
         * pointer (see drv_can.c); passing &baud silently mistunes. */
        rt_device_control(can, RT_CAN_CMD_SET_BAUD,
                          (void *)(uintptr_t)LV_AIC_CAN_CAPTURE_BAUD);
        /* SET_INT enables CAN interrupts; HDR builds need hdr=-1 reads. */
        rt_device_control(can, RT_DEVICE_CTRL_SET_INT, RT_NULL);
    }
    /* CAN read/write paths require the INT_RX/INT_TX open flags. */
    if (rt_device_open(can, RT_DEVICE_OFLAG_RDWR | RT_DEVICE_FLAG_INT_RX |
                            RT_DEVICE_FLAG_INT_TX) != RT_EOK) {
        rt_kprintf("can capture failed: open %s\n", cap_devname);
        lv_aic_display_snapshot_free(&cap_snapshot);
        level = rt_hw_interrupt_disable();
        cap_state = CAP_IDLE;
        rt_hw_interrupt_enable(level);
        return;
    }
    cap_abort = false;
    rt_kprintf("can capture streaming %ux%u frame=%u on %s\n",
               cap_snapshot.header.w, cap_snapshot.header.h, cap_frame, cap_devname);
    thread = rt_thread_create("can_cap", cap_stream, can, 4096, 25, 10);
    if (thread == RT_NULL || rt_thread_startup(thread) != RT_EOK) {
        rt_kprintf("can capture failed: thread\n");
        rt_device_close(can);
        lv_aic_display_snapshot_free(&cap_snapshot);
        level = rt_hw_interrupt_disable();
        cap_state = CAP_IDLE;
        rt_hw_interrupt_enable(level);
    }
}

void lv_aic_can_capture_deinit(void)
{
    rt_base_t level = rt_hw_interrupt_disable();
    cap_ready = false;
    cap_start_pending = false;
    if (cap_state == CAP_STREAMING) {
        cap_abort = true;
    } else {
        cap_state = CAP_IDLE;
    }
    rt_hw_interrupt_enable(level);
}

bool lv_aic_can_capture_request(const char *device)
{
    char name[sizeof(cap_devname)];
    snprintf(name, sizeof(name), "%s", device ? device : "can0");
    rt_base_t level = rt_hw_interrupt_disable();
    bool ok = cap_ready && cap_state == CAP_IDLE && !cap_start_pending;
    if (ok) {
        cap_state = CAP_REQUESTED;
        cap_start_pending = true;
        memcpy(cap_devname, name, sizeof(cap_devname));
    }
    rt_hw_interrupt_enable(level);
    return ok;
}

static void lv_aic_can_capture(int argc, char **argv)
{
    if (argc >= 2 && !strcmp(argv[1], "status")) {
        rt_kprintf("can capture state=%d device=%s last_retries=%u\n",
                   (int)cap_state, cap_devname[0] ? cap_devname : "can0",
                   (unsigned)cap_retries);
        return;
    }
    if (argc >= 2 && !strcmp(argv[1], "stop")) {
        rt_base_t level = rt_hw_interrupt_disable();
        bool active = cap_state == CAP_STREAMING;
        if (active) {
            cap_abort = true;
        }
        rt_hw_interrupt_enable(level);
        rt_kprintf(active ? "can capture stopping\n" : "can capture idle\n");
        return;
    }
    if (argc < 2 || strcmp(argv[1], "start") != 0) {
        rt_kprintf("Usage: lv_aic_can_capture start|stop|status [can0|can1]\n");
        return;
    }
    rt_kprintf(lv_aic_can_capture_request(argc >= 3 ? argv[2] : "can0")
                   ? "can capture request queued\n"
                   : "can capture busy or UI not ready; retry\n");
}
MSH_CMD_EXPORT(lv_aic_can_capture, Stream framebuffer RLE over CAN);
#else
void lv_aic_can_capture_poll(void) {}
void lv_aic_can_capture_deinit(void) {}
bool lv_aic_can_capture_request(const char *device)
{
    (void)device;
    return false;
}
#endif
#endif
