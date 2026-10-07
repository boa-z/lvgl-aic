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
void lv_draw_aic_ge2d_prepare_dst_cache(const lv_draw_buf_t *b, const lv_area_t *a)
{ assert(stage++ == 1); assert(a->x2 == b->header.w - 1 && a->y2 == b->header.h - 1); }
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
    lv_deinit();
    return 0;
}
