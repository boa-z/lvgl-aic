/* SPDX-License-Identifier: Apache-2.0 */
#ifndef LV_AIC_TEST_LOG_H
#define LV_AIC_TEST_LOG_H
#include <ulog.h>
/* Finite board probes only. Drain each record to avoid overflowing the async
 * ring during bursts; do not change application-wide logging configuration.
 * Do not call these macros from an IRQ or a timed rendering interval. */
#define AIC_TEST_I(...) do { LOG_I(__VA_ARGS__); ulog_flush(); } while (0)
#define AIC_TEST_E(...) do { LOG_E(__VA_ARGS__); ulog_flush(); } while (0)
#define AIC_TEST_W(...) do { LOG_W(__VA_ARGS__); ulog_flush(); } while (0)
#endif
