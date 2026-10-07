/* SPDX-License-Identifier: Apache-2.0 */
#ifndef LV_AIC_CAN_CAPTURE_H
#define LV_AIC_CAN_CAPTURE_H

/* Pulls lvgl_aic.h -> lv_conf.h so bare rtconfig bools are normalized
 * to 0/1 before the feature guard below is evaluated. */
#include "lv_aic_manual_test.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Capture CAN ID (standard 11-bit) and bus rate. 500 kbit/s matches the
 * product wiring convention (forklift-meter-platform boots CAN there). */
#define LV_AIC_CAN_CAPTURE_ID 0x1CA
#define LV_AIC_CAN_CAPTURE_BAUD CAN500kBaud
/* Host trigger: a standard frame with this ID and payload "CAP" asks the
 * board to capture. It is received by the smoke app's CAN OTA endpoint RX
 * thread (the only can0 reader; lvgl-aic-smoke ota/, AIC_LVGL_SMOKE_CAN_OTA),
 * which calls lv_aic_can_capture_request(). Without it, start the capture
 * from the shell. */
#define LV_AIC_CAN_CAPTURE_TRIGGER_ID 0x1CB

/* Frame layout, little-endian, 8 bytes, one RLE run per DATA frame:
 *   BEGIN flags=0x01: seq=0 | w | h | fmt
 *   DATA  flags=0x00: seq | count | p0 p1 p2   (seq 1,2,... modulo 65536)
 *   END   flags=0x02: seq=data frame count mod 65536 | crc32 (zlib)
 * fmt: 0=RGB565 1=RGB888 2=ARGB8888 3=XRGB8888. RLE runs and the CRC32
 * match the UART capture so the host PNG path is shared. */
#define LV_AIC_CAN_CAP_BEGIN 0x01
#define LV_AIC_CAN_CAP_DATA 0x00
#define LV_AIC_CAN_CAP_END 0x02

/* Runs on the LVGL owner thread from the manual-test timer. */
void lv_aic_can_capture_poll(void);
void lv_aic_can_capture_deinit(void);
/* Queue a capture on `device` (NULL = can0); thread-safe, never blocks.
 * Returns false while a capture is pending/streaming or the UI is down. */
bool lv_aic_can_capture_request(const char *device);

#ifdef __cplusplus
}
#endif

#endif /* LV_AIC_CAN_CAPTURE_H */
