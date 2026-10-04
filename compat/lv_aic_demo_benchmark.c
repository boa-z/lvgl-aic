/* SPDX-License-Identifier: Apache-2.0 */
#include "lvgl.h"
#if LV_USE_DEMO_BENCHMARK
/* Load private headers through the short include search path first. The old
 * Windows E907 compiler cannot open the deep relative path from the upstream
 * demo. Its include guard then skips that repeated private-header traversal. */
#include <lvgl_private.h>
#include <demos/benchmark/lv_demo_benchmark.c>
#endif
