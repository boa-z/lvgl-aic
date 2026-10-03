/* SPDX-License-Identifier: Apache-2.0 */
#include <assert.h>
#include <stdlib.h>
/* Host-only unique ioctl IDs; actual framebuffer structures use SDK headers. */
#define _IOR(type, nr, data) ((type << 8) | (nr))
#define _IOW(type, nr, data) ((type << 8) | (nr))
#define AIC_LVGL_USE_PRIVATE_API 1
#include "lvgl_aic_private.h"
#include "../../port/lv_aic_display.c"

static uint8_t framebuffer[64 * 32 * 4 * 2];
static int ge_result, pans, syncs, cleans, allocations;
static int failed_ioctl;
struct mpp_fb *mpp_fb_open(void) { return (struct mpp_fb *)(uintptr_t)1; }
void mpp_fb_close(struct mpp_fb *fb) { assert(fb); }
int mpp_fb_ioctl(struct mpp_fb *fb, int cmd, void *args)
{
    assert(fb);
    if (cmd == AICFB_GET_SCREENINFO) {
        struct aicfb_screeninfo *info = args;
        memset(info, 0, sizeof(*info));
        info->width = 64; info->height = 32; info->stride = 256;
        info->format = MPP_FMT_ARGB_8888;
        info->framebuffer = framebuffer;
        info->smem_len = sizeof(framebuffer);
    }
    if (cmd == AICFB_PAN_DISPLAY) pans++;
    if (cmd == AICFB_WAIT_FOR_VSYNC) syncs++;
    return cmd == failed_ioctl ? -1 : 0;
}
void *aicos_malloc_align(unsigned int type, size_t size, size_t align)
{ (void)type; (void)align; void *p = malloc(size); if (p) allocations++; return p; }
void aicos_free_align(unsigned int type, void *ptr)
{ (void)type; if (ptr) allocations--; free(ptr); }
void aicos_dcache_clean_invalid_range(unsigned long *addr, unsigned long size)
{ assert(addr && size); cleans++; }
void aicos_dcache_invalid_range(unsigned long *addr, unsigned long size)
{ assert(addr && size); }
int lv_draw_aic_ge2d_display_rotate(const lv_draw_buf_t *src, lv_draw_buf_t *dst,
                                   lv_display_rotation_t rotation)
{
    assert(rotation == LV_DISPLAY_ROTATION_90);
    assert(src->header.w == 32 && src->header.h == 64);
    assert(dst->header.w == 64 && dst->header.h == 32);
    return ge_result;
}
static void flush(lv_display_t *d)
{
    d->flushing = 1;
    d->flushing_last = 1;
    lv_aic_flush_cb(d, NULL, NULL);
    assert(!d->flushing);
}
int main(void)
{
    lv_init();
    lv_display_t *d = NULL;
    assert(lv_aic_display_init(&d) == LV_AIC_OK);
    lv_aic_display_ctx_t *ctx = lv_display_get_driver_data(d);
    assert(ctx && ctx->use_rotation && allocations == 1);
    ge_result = 1;
    flush(d);
    assert(pans == 1 && syncs == 1 && cleans == 0);
    assert(ctx->last_presented_valid && ctx->present_index == 1);
    unsigned count = lv_aic_display_flush_count_get();
    ge_result = -1;
    flush(d);
    assert(pans == 1 && syncs == 1 && cleans == 0);
    assert(!ctx->last_presented_valid && ctx->present_index == 1);
    assert(lv_aic_display_flush_count_get() == count);
    lv_draw_buf_t copy;
    uint32_t frame;
    assert(lv_aic_display_snapshot(d, &copy, &frame) == LV_AIC_ERR_DISPLAY);
    ge_result = 0;
    flush(d);
    assert(pans == 2 && syncs == 2 && cleans == 2);
    assert(ctx->last_presented_valid && ctx->present_index == 0);
    assert(lv_aic_display_snapshot(d, &copy, &frame) == LV_AIC_OK);
    lv_aic_display_snapshot_free(&copy);
    ge_result = 1;
    int failures[] = {AICFB_PAN_DISPLAY, AICFB_WAIT_FOR_VSYNC};
    for (unsigned i = 0; i < 2; i++) {
        failed_ioctl = failures[i];
        flush(d);
        assert(!ctx->last_presented_valid);
        assert(lv_aic_display_snapshot(d, &copy, &frame) == LV_AIC_ERR_DISPLAY);
        failed_ioctl = 0;
        flush(d);
        assert(ctx->last_presented_valid);
    }
    lv_aic_display_deinit(d);
    assert(allocations == 0);
    lv_deinit();
    return 0;
}
