/* SPDX-License-Identifier: Apache-2.0 */
#include <assert.h>
#include <stdint.h>
#include <string.h>
#include <limits.h>
/* Native SDK ulong is pointer-sized; Windows unsigned long is not. */
#define ulong uintptr_t
#include "../../draw/ge2d/lv_draw_aic_ge2d.c"
#include "../../draw/ge2d/lv_draw_aic_ge2d_image.c"

static struct ge_bitblt captured;
static int submits, fail_at;
static const void *allowed_src, *allowed_dst;
struct mpp_ge *mpp_ge_open(void) { return (struct mpp_ge *)(uintptr_t)1; }
void mpp_ge_close(struct mpp_ge *ge) { (void)ge; }
int mpp_ge_bitblt(struct mpp_ge *ge, struct ge_bitblt *b)
{ (void)ge; captured = *b; submits++; return fail_at == 1 ? -1 : 0; }
int mpp_ge_emit(struct mpp_ge *ge) { (void)ge; return fail_at == 2 ? -1 : 0; }
int mpp_ge_sync(struct mpp_ge *ge) { (void)ge; return fail_at == 3 ? -1 : 0; }
bool lv_draw_aic_ge2d_buf_address_valid(const lv_draw_buf_t *b)
{ return b && (b->data == allowed_src || b->data == allowed_dst); }
bool lv_draw_aic_ge2d_dst_format_supported(lv_color_format_t cf)
{ return lv_aic_pixel_format_is_ge2d_dst(cf); }
void lv_draw_aic_ge2d_prepare_src_cache(const lv_draw_buf_t *b, const lv_area_t *a)
{ (void)b; (void)a; }
void lv_draw_aic_ge2d_prepare_dst_cache(const lv_draw_buf_t *b, const lv_area_t *a)
{ (void)b; (void)a; }
lv_result_t lv_draw_aic_ge2d_fill(lv_draw_task_t *t) { (void)t; return LV_RESULT_OK; }

int main(void)
{
    static uint8_t pixels[32 * 128], output[128 * 512];
    lv_draw_buf_t src, dst;
    lv_draw_image_dsc_t d;
    lv_image_decoder_dsc_t decoder = {0};
    lv_layer_t layer = {0};
    lv_draw_task_t task = {0};
    lv_area_t origin = {100, 200, 131, 231};
    lv_area_t clip = {101, 202, 120, 215};
    lv_aic_ge2d_scale_axis_t a;
    const uint16_t ratios[] = {128, 384, 512};
    const lv_color_format_t formats[] = {LV_COLOR_FORMAT_RGB565, LV_COLOR_FORMAT_RGB888,
                                       LV_COLOR_FORMAT_ARGB8888, LV_COLOR_FORMAT_XRGB8888};
    lv_init();
    allowed_src = pixels;
    allowed_dst = output;
    assert(lv_draw_buf_init(&dst, 128, 128, LV_COLOR_FORMAT_ARGB8888, 512,
                            output, sizeof(output)) == LV_RESULT_OK);
    layer.draw_buf = &dst;
    layer.buf_area = (lv_area_t){90, 190, 217, 317};
    task.target_layer = &layer;
    task.type = LV_DRAW_TASK_TYPE_IMAGE;
    task.draw_dsc = &d;
    g_ge2d_dev = mpp_ge_open();
    g_ge2d_ready = true;
    for (unsigned f = 0; f < sizeof(formats) / sizeof(formats[0]); f++) {
        assert(lv_draw_buf_init(&src, 32, 32, formats[f], 128, pixels, sizeof(pixels)) == LV_RESULT_OK);
        decoder.decoded = &src;
        for (unsigned i = 0; i < 3; i++) {
            lv_area_t area;
            lv_draw_image_dsc_init(&d);
            d.src = &src;
            d.scale_x = d.scale_y = ratios[i];
            d.pivot = (lv_point_t){0, 0};
            d.opa = 128;
            lv_image_buf_get_transformed_area(&area, 32, 32, 0, d.scale_x, d.scale_y, &d.pivot);
            lv_area_move(&area, 100, 200);
            assert(lv_draw_aic_ge2d_accepts_image(&task));
            assert(lv_draw_aic_ge2d_blit(&task, &d, &decoder, &origin, &area));
            assert(captured.scale_phase.channel_num == 1 && captured.scale_phase.scaler_en == 1);
            assert(captured.scale_phase.scale_phase_en == 1);
            assert(captured.scale_phase.dx_16[0] == 16777216 / ratios[i]);
            assert(captured.dst_buf.crop.x == 10 && captured.dst_buf.crop.y == 10);
            assert(captured.dst_buf.crop.width == lv_area_get_width(&area));
            assert(captured.src_buf.crop.width <= 32 && captured.src_buf.crop.height <= 32);
            assert(captured.ctrl.alpha_rules == GE_PD_NONE && captured.ctrl.src_alpha_mode == 2);
            assert(captured.ctrl.src_global_alpha == 128);
            /* Exercise the actual bin decoder, not just the blit callback.
             * A decoder-created heap copy is outside the GE memory window. */
            {
                lv_draw_aic_ge2d_outcome_t outcome;
                task.area = origin;
                task.clip_area = layer.buf_area;
                assert(lv_draw_aic_ge2d_image(&task, &outcome) == LV_RESULT_OK);
                assert(outcome == LV_DRAW_AIC_GE2D_OUTCOME_ENGINE);
                assert(captured.src_buf.stride[0] == 128);
                lv_image_cache_drop(&src);
            }
        }
    }
    d.scale_x = 384; d.scale_y = 192; d.pivot = (lv_point_t){7, 9};
    clip = (lv_area_t){101, 203, 120, 215};
    assert(lv_draw_aic_ge2d_blit(&task, &d, &decoder, &origin, &clip));
    assert(captured.scale_phase.dx_16[0] == 43690 && captured.scale_phase.dy_16[0] == 87381);
    assert(captured.src_buf.crop.x == 3 && captured.src_buf.crop.y == 1);
    assert(captured.scale_phase.h_phase_16[0] == 4 && captured.scale_phase.v_phase_16[0] == 2);
    assert(captured.dst_buf.crop.x == 11 && captured.dst_buf.crop.y == 13);
    assert(captured.dst_buf.crop.width == 20 && captured.dst_buf.crop.height == 13);
    for (fail_at = 1; fail_at <= 3; fail_at++) {
        lv_draw_aic_ge2d_outcome_t outcome;
        task.area = origin; task.clip_area = clip;
        assert(lv_draw_aic_ge2d_image(&task, &outcome) == LV_RESULT_INVALID);
        assert(outcome != LV_DRAW_AIC_GE2D_OUTCOME_SOFTWARE);
    }
    fail_at = 0;
    {
        int before = submits;
        lv_area_t tiny = {100,200,102,215};
        d.pivot = (lv_point_t){0,0}; d.scale_x = d.scale_y = 512;
        assert(!lv_draw_aic_ge2d_blit(&task,&d,&decoder,&origin,&tiny));
        d.scale_x = 264; d.scale_y = 256;
        assert(!lv_draw_aic_ge2d_blit(&task,&d,&decoder,&origin,&origin));
        d.scale_x = 4097;
        assert(!lv_draw_aic_ge2d_blit(&task,&d,&decoder,&origin,&origin));
        assert(submits == before);
    }
    {
        int before = submits;
        d.scale_x = 0; assert(!lv_draw_aic_ge2d_accepts_image(&task));
        d.scale_x = 15; assert(!lv_draw_aic_ge2d_accepts_image(&task));
        d.scale_x = 4097; assert(!lv_draw_aic_ge2d_accepts_image(&task));
        d.scale_x = 384; d.rotation = 170; assert(!lv_draw_aic_ge2d_accepts_image(&task));
        d.rotation = 0; d.tile = 1; assert(!lv_draw_aic_ge2d_accepts_image(&task));
        d.tile = 0; d.recolor_opa = 128; assert(!lv_draw_aic_ge2d_accepts_image(&task));
        assert(submits == before);
    }
    assert(!lv_aic_ge2d_scale_axis(32, 0, 3, 0, 512, &a));
    assert(!lv_aic_ge2d_scale_axis(32, -1, 8, 0, 384, &a));
    assert(!lv_aic_ge2d_scale_axis(32, 60, 8, 0, 512, &a));
    assert(!lv_aic_ge2d_scale_axis(32, INT_MAX, INT_MAX, INT_MIN, 16, &a));
    assert(lv_aic_ge2d_scale_split_risk(16777216 / 264, 64));
    assert(!lv_aic_ge2d_scale_split_risk(16777216 / 384, 64));
    assert(!lv_aic_ge2d_scale_split_risk(65536, 64));
    assert(!lv_aic_ge2d_scale_split_risk(63488, 31));
    lv_deinit();
    return 0;
}
