/* SPDX-License-Identifier: Apache-2.0 */
#include <assert.h>
#include <stdint.h>
#define AIC_LVGL_USE_PRIVATE_API 1
#include "lv_draw_aic_ge2d.h"
#include "lv_draw_aic_ge2d_display.h"
#include <mpp_ge.h>
static int stage, fail_at;
static bool available = true, quarantined;
bool lv_draw_aic_ge2d_faulted(void) { return quarantined; }
void lv_draw_aic_ge2d_quarantine(void) { quarantined=true; }
static struct ge_bitblt captured;
struct mpp_ge *lv_draw_aic_ge2d_device(void)
{ return available ? (struct mpp_ge *)(uintptr_t)1 : NULL; }
bool lv_draw_aic_ge2d_buf_address_valid(const lv_draw_buf_t *b)
{
    uintptr_t address = (uintptr_t)b->data;
    return address >= 0x40000000U && address <= UINT32_MAX &&
           (uint64_t)b->data_size <= (uint64_t)UINT32_MAX + 1U - address;
}
void lv_draw_aic_ge2d_prepare_src_cache(const lv_draw_buf_t *b, const lv_area_t *a)
{ assert(stage++ == 0); assert(a->x2 == b->header.w - 1 && a->y2 == b->header.h - 1); }
/* Whole destination for rotation; the fit rectangle for the scale. */
static bool expect_crop;
static lv_area_t expect_dst;
void lv_draw_aic_ge2d_prepare_dst_cache(const lv_draw_buf_t *b, const lv_area_t *a)
{
    assert(stage++ == 1);
    if (expect_crop)
        assert(a->x1 == expect_dst.x1 && a->y1 == expect_dst.y1 &&
               a->x2 == expect_dst.x2 && a->y2 == expect_dst.y2);
    else
        assert(a->x2 == b->header.w - 1 && a->y2 == b->header.h - 1);
}
int mpp_ge_bitblt(struct mpp_ge *ge, struct ge_bitblt *b)
{ assert(ge && stage++ == 2); captured = *b; return fail_at == 1 ? -1 : 0; }
int mpp_ge_emit(struct mpp_ge *ge)
{ assert(ge && stage++ == 3); return fail_at == 2 ? -1 : 0; }
int mpp_ge_sync(struct mpp_ge *ge)
{ assert(ge && stage++ == 4); return fail_at == 3 ? -1 : 0; }
int main(void)
{
    lv_init();
    lv_draw_buf_t src = {0}, dst = {0};
    src.data = (void *)(uintptr_t)0x40000000U;
    dst.data = (void *)(uintptr_t)0x41000000U;
    src.header.w = 80; src.header.h = 48;
    src.data_size = dst.data_size = 65536;
    lv_color_format_t formats[] = {LV_COLOR_FORMAT_RGB565, LV_COLOR_FORMAT_RGB888,
                                   LV_COLOR_FORMAT_XRGB8888, LV_COLOR_FORMAT_ARGB8888};
    unsigned flags[] = {MPP_ROTATION_270, MPP_ROTATION_180, MPP_ROTATION_90};
    for (unsigned f = 0; f < 4; f++) {
        src.header.cf = dst.header.cf = formats[f];
        src.header.stride = 80 * lv_color_format_get_size(formats[f]) + 16;
        for (int rot = 1; rot <= 3; rot++) {
            dst.header.w = rot == 2 ? 80 : 48;
            dst.header.h = rot == 2 ? 48 : 80;
            dst.header.stride = dst.header.w * lv_color_format_get_size(formats[f]) + 16;
            for (fail_at = 0; fail_at <= 3; fail_at++) {
                stage = 0;quarantined=false;
                assert(lv_draw_aic_ge2d_display_rotate(&src, &dst, rot) == (fail_at ? -1 : 1));
                assert(stage == (fail_at ? fail_at + 2 : 5));
                assert(quarantined==(fail_at!=0));
                if(fail_at) {
                    int before=stage;
                    assert(lv_draw_aic_ge2d_display_rotate(&src,&dst,rot)==-1 && stage==before);
                }
                assert(captured.ctrl.flags == flags[rot - 1]);
                assert(!captured.ctrl.alpha_en);
                assert(captured.dst_buf.size.width == (int)dst.header.w);
                assert(captured.src_buf.stride[0] == (int)src.header.stride);
            }
        }
    }
    fail_at = stage = 0;quarantined=false;
    assert(lv_draw_aic_ge2d_display_rotate(&src, &dst, 0) == 0);
    available = false;
    assert(lv_draw_aic_ge2d_display_rotate(&src, &dst, 1) == 0);
    available = true;
    dst.data = src.data;
    assert(lv_draw_aic_ge2d_display_rotate(&src, &dst, 1) == 0);
    dst.data = (void *)(uintptr_t)0x41000000U;
    dst.data_size = 10;
    assert(lv_draw_aic_ge2d_display_rotate(&src, &dst, 1) == 0);
    dst.data_size = 65536;
    uint32_t dst_stride = dst.header.stride;
    dst.header.stride = dst.header.w * lv_color_format_get_size((lv_color_format_t)dst.header.cf) - 1;
    assert(lv_draw_aic_ge2d_display_rotate(&src, &dst, 1) == 0);
    dst.header.stride = 0;
    assert(lv_draw_aic_ge2d_display_rotate(&src, &dst, 1) == 0);
    dst.header.stride = dst_stride;
    src.data = (void *)(uintptr_t)0xfffffff0U;
    assert(lv_draw_aic_ge2d_display_rotate(&src, &dst, 1) == 0);
    src.data = (void *)(uintptr_t)0x10000000U;
    assert(lv_draw_aic_ge2d_display_rotate(&src, &dst, 1) == 0);
    assert(stage == 0);

    /* Virtual-resolution present: 1024x600 RGB888 into the 800x469 fit of
     * an 800x480 panel at (0,5); plain copy, crop on the destination only. */
    {
        lv_draw_buf_t vs = {0}, fb = {0};
        vs.data = (void *)(uintptr_t)0x40000000U;
        fb.data = (void *)(uintptr_t)0x42000000U;
        vs.header.cf = fb.header.cf = LV_COLOR_FORMAT_RGB888;
        vs.header.w = 1024; vs.header.h = 600; vs.header.stride = 1024 * 3;
        fb.header.w = 800; fb.header.h = 480; fb.header.stride = 800 * 3;
        vs.data_size = vs.header.stride * 600U;
        fb.data_size = fb.header.stride * 480U;
        expect_crop = true;
        expect_dst = (lv_area_t){0, 5, 799, 5 + 469 - 1};
        for (fail_at = 0; fail_at <= 3; fail_at++) {
            stage = 0; quarantined = false;
            assert(lv_draw_aic_ge2d_display_scale(&vs, &fb, 0, 5, 800, 469) == (fail_at ? -1 : 1));
            assert(stage == (fail_at ? fail_at + 2 : 5));
            assert(quarantined == (fail_at != 0));
            assert(captured.ctrl.flags == 0 && !captured.ctrl.alpha_en);
            assert(!captured.src_buf.crop_en);
            assert(captured.src_buf.size.width == 1024 && captured.src_buf.size.height == 600);
            assert(captured.dst_buf.crop_en && captured.dst_buf.crop.x == 0 &&
                   captured.dst_buf.crop.y == 5 && captured.dst_buf.crop.width == 800 &&
                   captured.dst_buf.crop.height == 469);
            assert(captured.dst_buf.size.width == 800 && captured.dst_buf.size.height == 480);
        }
        /* Declined before any cache work or submission. */
        fail_at = 0; quarantined = false; stage = 0;
        assert(lv_draw_aic_ge2d_display_scale(&vs, &fb, 0, 12, 800, 469) == 0); /* out of fb */
        assert(lv_draw_aic_ge2d_display_scale(&vs, &fb, 0, 0, 3, 469) == 0);    /* < 4 px */
        assert(lv_draw_aic_ge2d_display_scale(&vs, &fb, 0, 0, 60, 469) == 0);   /* > 16x */
        vs.header.w = 760; vs.header.stride = 760 * 3;                          /* near-unity */
        assert(lv_draw_aic_ge2d_display_scale(&vs, &fb, 0, 0, 800, 469) == 0);
        vs.header.w = 1024; vs.header.stride = 1024 * 3;
        fb.header.cf = LV_COLOR_FORMAT_RGB565;                                  /* format */
        assert(lv_draw_aic_ge2d_display_scale(&vs, &fb, 0, 5, 800, 469) == 0);
        assert(stage == 0);
        expect_crop = false;
    }
    lv_deinit();
    return 0;
}
