/* SPDX-License-Identifier: Apache-2.0 */
#include <assert.h>
#include <stdint.h>
#include <string.h>
#include <limits.h>
#include <math.h>
/* Native SDK ulong is pointer-sized; Windows unsigned long is not. */
#define ulong uintptr_t
#include "../../draw/ge2d/lv_draw_aic_ge2d.c"
#include "../../draw/ge2d/lv_draw_aic_ge2d_image.c"

static struct ge_bitblt captured;
static struct ge_rotation captured_rotation;
static int submits, rotate_submits, fail_at, rotate_fail;
static int fail_submission;
static const void *allowed_src, *allowed_dst;
struct mpp_ge *mpp_ge_open(void) { return (struct mpp_ge *)(uintptr_t)1; }
void mpp_ge_close(struct mpp_ge *ge) { (void)ge; }
int mpp_ge_bitblt(struct mpp_ge *ge, struct ge_bitblt *b)
{ (void)ge; captured = *b; submits++; return fail_at == 1 || submits == fail_submission ? -1 : 0; }
int mpp_ge_rotate(struct mpp_ge *ge, struct ge_rotation *r)
{ (void)ge; captured_rotation = *r; rotate_submits++; return rotate_fail ? -1 : 0; }
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
    lv_layer_t child_layer = {0};
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
    child_layer.draw_buf = &src;
    task.target_layer = &layer;
    task.type = LV_DRAW_TASK_TYPE_IMAGE;
    task.draw_dsc = &d;
    g_ge2d_dev = mpp_ge_open();
    g_ge2d_ready = true;
    /* Independent floating-point oracle for every LVGL tenth of a degree.
     * Integer-degree rounding used to lose up to 36 Q12 units here. */
    for (int angle = 0; angle < 3600; angle++) {
        double radians = angle * 3.14159265358979323846 / 1800.0;
        assert(fabs(lv_draw_aic_ge2d_angle_2_12(angle, false) - sin(radians) * 4096) < 2);
        assert(fabs(lv_draw_aic_ge2d_angle_2_12(angle, true) - cos(radians) * 4096) < 2);
    }
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
        int before = rotate_submits;
        lv_area_t rot_img = {100, 200, 131, 231};
        lv_area_t rot_clip = {101, 202, 120, 215};
        lv_draw_image_dsc_init(&d);
        d.src = &src;
        d.scale_x = d.scale_y = LV_SCALE_NONE;
        d.rotation = 170;
        d.pivot = (lv_point_t){16, 16};
        d.opa = 128;
        assert(lv_draw_aic_ge2d_accepts_image(&task));
        assert(lv_draw_aic_ge2d_blit(&task, &d, &decoder, &rot_img, &rot_clip));
        assert(rotate_submits == before + 1);
        assert(captured_rotation.src_rot_center.x == 16 &&
               captured_rotation.src_rot_center.y == 16);
        assert(captured_rotation.dst_rot_center.x == 15 &&
               captured_rotation.dst_rot_center.y == 14);
        assert(captured_rotation.src_buf.crop_en == 0);
        assert(captured_rotation.dst_buf.crop.x == 11 &&
               captured_rotation.dst_buf.crop.y == 12);
        assert(captured_rotation.dst_buf.crop.width == 20 &&
               captured_rotation.dst_buf.crop.height == 14);
        assert(captured_rotation.angle_sin > 1190 && captured_rotation.angle_sin < 1210);
        assert(captured_rotation.angle_cos > 3910 && captured_rotation.angle_cos < 3930);
        assert(captured_rotation.ctrl.alpha_rules == GE_PD_NONE &&
               captured_rotation.ctrl.src_alpha_mode == 2 &&
               captured_rotation.ctrl.src_global_alpha == 128);
        {
            lv_draw_aic_ge2d_outcome_t outcome;
            task.area = rot_img;
            task.clip_area = layer.buf_area;
            rotate_fail = 1;
            assert(lv_draw_aic_ge2d_image(&task, &outcome) == LV_RESULT_INVALID);
            assert(outcome != LV_DRAW_AIC_GE2D_OUTCOME_SOFTWARE);
            rotate_fail = 0;
        }
    }
    {
        int before = submits;
        d.scale_x = 0; assert(!lv_draw_aic_ge2d_accepts_image(&task));
        d.scale_x = 15; assert(!lv_draw_aic_ge2d_accepts_image(&task));
        d.scale_x = 4097; assert(!lv_draw_aic_ge2d_accepts_image(&task));
        d.scale_x = 384; d.scale_y = 384; d.rotation = 170; assert(!lv_draw_aic_ge2d_accepts_image(&task));
        d.scale_x = d.scale_y = LV_SCALE_NONE; assert(lv_draw_aic_ge2d_accepts_image(&task));
        d.rotation = 900; assert(lv_draw_aic_ge2d_accepts_image(&task));
        d.rotation = 0; d.tile = 1; assert(lv_draw_aic_ge2d_accepts_image(&task));
        d.rotation = 900; assert(!lv_draw_aic_ge2d_accepts_image(&task));
        d.rotation = 0;
        d.tile = 0; d.recolor_opa = 128; assert(!lv_draw_aic_ge2d_accepts_image(&task));
        assert(submits == before);
    }
    {
        lv_draw_image_dsc_init(&d);
        d.src = &child_layer;
        d.scale_x = 384;
        d.scale_y = 512;
        d.rotation = 900;
        d.opa = 128;
        task.type = LV_DRAW_TASK_TYPE_LAYER;
        assert(lv_draw_aic_ge2d_accepts_layer(&task));
        d.scale_x = 4097;
        assert(!lv_draw_aic_ge2d_accepts_layer(&task));
        d.scale_x = d.scale_y = LV_SCALE_NONE;
        d.rotation = 175;
        d.pivot = (lv_point_t){16, 16};
        task.area = origin;
        task.clip_area = clip;
        assert(lv_draw_aic_ge2d_accepts_layer(&task));
        for (unsigned f = 0; f < sizeof(formats) / sizeof(formats[0]); f++) {
            lv_draw_aic_ge2d_outcome_t outcome;
            int before = rotate_submits;
            assert(lv_draw_buf_init(&src, 32, 32, formats[f], 128,
                                   pixels, sizeof(pixels)) == LV_RESULT_OK);
            assert(lv_draw_aic_ge2d_image(&task, &outcome) == LV_RESULT_OK);
            assert(outcome == LV_DRAW_AIC_GE2D_OUTCOME_ENGINE);
            assert(rotate_submits == before + 1);
            assert(captured_rotation.src_buf.stride[0] == 128);
            assert(captured_rotation.dst_buf.crop.x == 11);
            assert(captured_rotation.dst_rot_center.x == 15);
            assert(captured_rotation.dst_rot_center.y == 13);
            assert(captured_rotation.angle_sin >= 1230 && captured_rotation.angle_sin <= 1232);
            assert(captured_rotation.ctrl.src_global_alpha == 128);
            lv_image_cache_drop(&src);
        }
        for (fail_at = 1; fail_at <= 3; fail_at++) {
            lv_draw_aic_ge2d_outcome_t outcome;
            rotate_fail = fail_at == 1;
            assert(lv_draw_aic_ge2d_image(&task, &outcome) == LV_RESULT_INVALID);
            assert(outcome != LV_DRAW_AIC_GE2D_OUTCOME_SOFTWARE);
        }
        rotate_fail = fail_at = 0;
        {
            lv_draw_aic_ge2d_outcome_t outcome;
            int before = rotate_submits;
            /* Narrow destination clips fall back BEFORE GE submission. */
            task.clip_area = (lv_area_t){101, 203, 103, 215};
            assert(lv_draw_aic_ge2d_image(&task, &outcome) == LV_RESULT_OK);
            assert(outcome == LV_DRAW_AIC_GE2D_OUTCOME_SOFTWARE);
            assert(rotate_submits == before);
            task.clip_area = clip;
            allowed_src = NULL;
            assert(lv_draw_aic_ge2d_image(&task, &outcome) == LV_RESULT_OK);
            assert(outcome == LV_DRAW_AIC_GE2D_OUTCOME_SOFTWARE);
            assert(rotate_submits == before);
            allowed_src = pixels;
            child_layer.draw_buf = NULL;
            assert(lv_draw_aic_ge2d_image(&task, &outcome) == LV_RESULT_OK);
            assert(outcome == LV_DRAW_AIC_GE2D_OUTCOME_NOTHING);
            child_layer.draw_buf = &src;
            assert(rotate_submits == before);
        }
        d.scale_x = 384;
        assert(!lv_draw_aic_ge2d_accepts_layer(&task));
        d.scale_x = LV_SCALE_NONE;
        d.skew_x = 1;
        assert(!lv_draw_aic_ge2d_accepts_layer(&task));
        task.type = LV_DRAW_TASK_TYPE_IMAGE;
    }
    assert(!lv_aic_ge2d_scale_axis(32, 0, 3, 0, 512, &a));
    {
        lv_image_colorkey_t key = {0};
        key.low = key.high = lv_color_make(255, 0, 255);
        lv_draw_image_dsc_init(&d);
        d.src = &src;
        d.colorkey = &key;
        d.antialias = 0;
        d.opa = 128;
        task.area = origin;
        task.clip_area = clip;
        for (unsigned f = 1; f < sizeof(formats) / sizeof(formats[0]); f++) {
            lv_draw_aic_ge2d_outcome_t outcome;
            assert(lv_draw_buf_init(&src, 32, 32, formats[f], 128,
                                   pixels, sizeof(pixels)) == LV_RESULT_OK);
            assert(lv_draw_aic_ge2d_accepts_image(&task));
            assert(lv_draw_aic_ge2d_image(&task, &outcome) == LV_RESULT_OK);
            assert(outcome == LV_DRAW_AIC_GE2D_OUTCOME_ENGINE);
            assert(captured.ctrl.ck_en == 1 && captured.ctrl.ck_value == 0xff00ff);
            assert(captured.ctrl.alpha_en == 1 && captured.ctrl.src_global_alpha == 128);
            lv_image_cache_drop(&src);
        }
        key.high = lv_color_make(255, 1, 255);
        assert(!lv_draw_aic_ge2d_accepts_image(&task));
        key.high = key.low;
        d.scale_x = 512;
        assert(!lv_draw_aic_ge2d_accepts_image(&task));
        d.scale_x = LV_SCALE_NONE;
        d.rotation = 175;
        assert(!lv_draw_aic_ge2d_accepts_image(&task));
        d.rotation = 900;
        assert(lv_draw_aic_ge2d_accepts_image(&task));
        d.rotation = 0;
        assert(lv_draw_buf_init(&src,32,32,LV_COLOR_FORMAT_RGB565,128,pixels,sizeof(pixels)) == LV_RESULT_OK);
        int before = submits;
        assert(!lv_draw_aic_ge2d_blit(&task,&d,&decoder,&origin,&clip));
        assert(submits == before);
        assert(lv_draw_buf_init(&src,32,32,LV_COLOR_FORMAT_ARGB8888,128,pixels,sizeof(pixels)) == LV_RESULT_OK);
        d.antialias = 1;
        assert(!lv_draw_aic_ge2d_blit(&task,&d,&decoder,&origin,&clip));
        assert(submits == before);
        d.colorkey = NULL;
        assert(lv_draw_aic_ge2d_blit(&task,&d,&decoder,&origin,&clip));
        assert(captured.ctrl.ck_en == 0);
    }
    assert(!lv_aic_ge2d_scale_axis(32, -1, 8, 0, 384, &a));
    assert(!lv_aic_ge2d_scale_axis(32, 60, 8, 0, 512, &a));
    assert(!lv_aic_ge2d_scale_axis(32, INT_MAX, INT_MAX, INT_MIN, 16, &a));
    assert(lv_aic_ge2d_scale_split_risk(16777216 / 264, 64));
    assert(!lv_aic_ge2d_scale_split_risk(16777216 / 384, 64));
    assert(!lv_aic_ge2d_scale_split_risk(65536, 64));
    assert(!lv_aic_ge2d_scale_split_risk(63488, 31));
    {
        lv_area_t dst = {-23, 0, 0, 31}, crop;
        lv_point_t pivot = {0, 0};
        unsigned flags = 0;
        assert(lv_aic_ge2d_rotation_crop(32, 24, &dst, &pivot, 900, &crop, &flags));
        assert(flags == MPP_ROTATION_90);
        assert(crop.x1 == 0 && crop.y1 == 0 && crop.x2 == 31 && crop.y2 == 23);
        dst = (lv_area_t){-12, -14, -2, -3};
        assert(lv_aic_ge2d_rotation_crop(32, 24, &dst, &pivot, 1800, &crop, &flags));
        assert(flags == MPP_ROTATION_180 && crop.x1 == 2 && crop.y1 == 3 && crop.x2 == 12 && crop.y2 == 14);
        assert(!lv_aic_ge2d_rotation_crop(32, 24, &dst, &pivot, 450, &crop, &flags));
        {
            int32_t phase_x, phase_y;
            dst = (lv_area_t){-15, 0, 0, 15};
            assert(lv_aic_ge2d_rotation_scale_crop(32, 32, &dst, &pivot, 900,
                                                   128, 128, &crop, &flags,
                                                   &phase_x, &phase_y));
            assert(flags == MPP_ROTATION_90 && crop.x1 == 0 && crop.y1 == 0);
            assert(phase_x == 0 && phase_y == 0);
        }
    }
    {
        assert(lv_draw_buf_init(&src, 32, 32, LV_COLOR_FORMAT_ARGB8888, 128,
                                pixels, sizeof(pixels)) == LV_RESULT_OK);
        decoder.decoded = &src;
        lv_draw_image_dsc_init(&d);
        d.src = &src; d.header = src.header; d.tile = 1; d.opa = 128;
        d.image_area = (lv_area_t){90, 190, 121, 221};
        task.type = LV_DRAW_TASK_TYPE_IMAGE;
        task.draw_dsc = &d;
        task.area = (lv_area_t){90, 190, 153, 253};
        task.clip_area = (lv_area_t){95, 195, 148, 248};
        task.target_layer = &layer;
        allowed_src = pixels; allowed_dst = output;
        fail_at = 0;
        int before = submits;
        assert(lv_draw_aic_ge2d_tiles(&task, &d, &decoder) == 1);
        assert(submits == before + 4);
        assert(captured.src_buf.crop.width == 27 && captured.src_buf.crop.height == 27);
        assert(captured.ctrl.src_global_alpha == 128);
        before = submits; fail_at = 1;
        assert(lv_draw_aic_ge2d_tiles(&task, &d, &decoder) == -1);
        assert(submits == before + 1 && !s_blit_validate_only);
        fail_at = 0; before = submits; allowed_src = NULL;
        assert(lv_draw_aic_ge2d_tiles(&task, &d, &decoder) == 0);
        assert(submits == before && !s_blit_validate_only);
        /* Public executor with the real LVGL bin decoder, including failures
         * after earlier alpha tiles have already reached the engine. */
        allowed_src = pixels;
        lv_draw_aic_ge2d_outcome_t outcome;
        before = submits;
        assert(lv_draw_aic_ge2d_image(&task, &outcome) == LV_RESULT_OK);
        assert(outcome == LV_DRAW_AIC_GE2D_OUTCOME_ENGINE && submits == before + 4);
        for (int tile = 1; tile <= 4; tile++) {
            before = submits;
            fail_submission = before + tile;
            assert(lv_draw_aic_ge2d_image(&task, &outcome) == LV_RESULT_INVALID);
            assert(outcome == LV_DRAW_AIC_GE2D_OUTCOME_NOTHING);
            assert(submits == before + tile && !s_blit_validate_only);
        }
        fail_submission = 0;
        before = submits;
        task.clip_area = (lv_area_t){200, 300, 210, 310};
        assert(lv_draw_aic_ge2d_image(&task, &outcome) == LV_RESULT_OK);
        assert(outcome == LV_DRAW_AIC_GE2D_OUTCOME_NOTHING && submits == before);
        /* A huge task with a tiny visible clip must skip invisible tiles. */
        task.area = (lv_area_t){-1000000000, -1000000000, 1000000000, 1000000000};
        task.clip_area = (lv_area_t){95, 195, 100, 200};
        assert(lv_draw_aic_ge2d_image(&task, &outcome) == LV_RESULT_OK);
        assert(outcome == LV_DRAW_AIC_GE2D_OUTCOME_ENGINE && submits == before + 1);
        lv_image_cache_drop(&src);
    }
    lv_deinit();
    return 0;
}
