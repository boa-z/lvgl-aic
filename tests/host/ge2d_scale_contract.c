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
#include "../../draw/ge2d/lv_draw_aic_ge2d_fill.c"

static struct ge_bitblt captured, tile_history[16];
static int rgb_retained,rgb_released;
static bool rgb_retain(void *p) { (void)p; rgb_retained++; return true; }
static void rgb_release(void *p) { (void)p; rgb_released++; }
static bool record_tiles;
static unsigned tile_count, oracle_count;
static lv_area_t oracle_cells[16], oracle_clips[16];
/* Run the upstream tiled helper as an independent grid/clip oracle. */
static void tile_oracle(lv_draw_task_t *t,const lv_draw_image_dsc_t *d,
    const lv_image_decoder_dsc_t *decoder,lv_draw_image_sup_t *sup,
    const lv_area_t *cell,const lv_area_t *clip)
{
    (void)d; (void)decoder; (void)sup;
    lv_area_t visible;
    if(!lv_area_intersect(&visible,clip,&t->clip_area) ||
       !lv_area_intersect(&visible,&visible,&t->target_layer->buf_area)) return;
    assert(oracle_count<16); oracle_cells[oracle_count]=*cell;
    oracle_clips[oracle_count++]=visible;
}
static struct ge_rotation captured_rotation;
static struct ge_fillrect captured_fill;
static int fills;
static int submits, rotate_submits, fail_at, rotate_fail;
static int fail_submission;
static const void *allowed_src, *allowed_dst;
static void combined_transform_contract(void)
{
    const uint32_t scales[][2]={{128,384},{384,512},{512,128}};
    const lv_point_t pivot={8,12};
    for (unsigned s=0;s<3;s++) {
        for (int angle=0;angle<3600;angle+=900) {
            lv_point_t points[4]={{10,14},{22,14},{10,26},{22,26}};
            lv_point_array_transform(points,4,angle,scales[s][0],scales[s][1],&pivot,true);
            lv_area_t visible={points[0].x,points[0].y,points[0].x,points[0].y}, crop;
            for (unsigned i=1;i<4;i++) {
                if(points[i].x<visible.x1) visible.x1=points[i].x;
                if(points[i].x>visible.x2) visible.x2=points[i].x;
                if(points[i].y<visible.y1) visible.y1=points[i].y;
                if(points[i].y>visible.y2) visible.y2=points[i].y;
            }
            unsigned flags;
            int32_t px,py;
            assert(lv_aic_ge2d_rotation_scale_crop(32,32,&visible,&pivot,angle,
                       scales[s][0],scales[s][1],&crop,&flags,&px,&py));
            assert(crop.x1==10 && crop.y1==14 && crop.x2==22 && crop.y2==26);
            assert(px==0 && py==0);
            /* A one-pixel inset creates independent fractional source phases.
             * Use a floating inverse matrix as the oracle, not the Q16 helper. */
            visible.x1++; visible.y1++; visible.x2--; visible.y2--;
            double radians=angle*3.141592653589793/1800.0;
            double minx=1e6,miny=1e6;
            for(unsigned i=0;i<4;i++) {
                double dx=((i&1)?visible.x2:visible.x1)-pivot.x;
                double dy=((i&2)?visible.y2:visible.y1)-pivot.y;
                double x=pivot.x+(cos(radians)*dx+sin(radians)*dy)*256/scales[s][0];
                double y=pivot.y+(-sin(radians)*dx+cos(radians)*dy)*256/scales[s][1];
                if(x<minx) minx=x;
                if(y<miny) miny=y;
            }
            assert(lv_aic_ge2d_rotation_scale_crop(32,32,&visible,&pivot,angle,
                       scales[s][0],scales[s][1],&crop,&flags,&px,&py));
            assert(fabs(crop.x1+px/65536.0-minx)<0.00004);
            assert(fabs(crop.y1+py/65536.0-miny)<0.00004);
        }
    }
}
/* A large image-relative translation must not wrap during Q8 conversion.
 * Use a double forward matrix, independent of the inverse fixed-point helper. */
static void wide_rotation_contract(void)
{
    const lv_point_t pivots[]={{16777216,-16777216},{-16777216,16777216}};
    for(unsigned p=0;p<2;p++) for(int angle=0;angle<3600;angle+=900) {
        double radians=angle*3.14159265358979323846/1800.0;
        double minx=1e30,miny=1e30,maxx=-1e30,maxy=-1e30;
        for(unsigned i=0;i<4;i++) {
            double dx=(((i&1)?12:4)-pivots[p].x)*2.0;
            double dy=(((i&2)?18:6)-pivots[p].y)*3.0;
            double x=pivots[p].x+cos(radians)*dx-sin(radians)*dy;
            double y=pivots[p].y+sin(radians)*dx+cos(radians)*dy;
            if(x<minx) minx=x;
            if(x>maxx) maxx=x;
            if(y<miny) miny=y;
            if(y>maxy) maxy=y;
        }
        lv_area_t visible={(int32_t)llround(minx),(int32_t)llround(miny),
                           (int32_t)llround(maxx),(int32_t)llround(maxy)},crop;
        unsigned flags;int32_t px,py;
        assert(lv_aic_ge2d_rotation_scale_crop(32,32,&visible,&pivots[p],angle,
                                              512,768,&crop,&flags,&px,&py));
        assert(crop.x1==4 && crop.x2==12 && crop.y1==6 && crop.y2==18);
        assert(px==0 && py==0);
    }
    lv_point_t zero={0,0};lv_area_t crop;unsigned flags;int32_t px,py;
    lv_area_t visible={16777216,0,16777232,16};
    assert(!lv_aic_ge2d_rotation_scale_crop(32,32,&visible,&zero,0,
                                           256,256,&crop,&flags,&px,&py));
    visible=(lv_area_t){12,0,4,16};
    assert(!lv_aic_ge2d_rotation_crop(32,32,&visible,&zero,0,&crop,&flags));
    assert(!lv_aic_ge2d_rotation_scale_crop(32,32,&visible,&zero,0,
                                           256,256,&crop,&flags,&px,&py));
}
/* Native YUV executor has its own real-ABI lease/submission contract. */
int lv_draw_aic_ge2d_yuv(lv_draw_task_t *task) { (void)task; return 0; }
bool lv_draw_aic_ge2d_yuv_faulted(void) { return false; }
struct mpp_ge *mpp_ge_open(void) { return (struct mpp_ge *)(uintptr_t)1; }
void mpp_ge_close(struct mpp_ge *ge) { (void)ge; }
int mpp_ge_bitblt(struct mpp_ge *ge, struct ge_bitblt *b)
{ (void)ge; captured = *b; if(record_tiles) { assert(tile_count<16); tile_history[tile_count++]=*b; } submits++; return fail_at == 1 || submits == fail_submission ? -1 : 0; }
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
int mpp_ge_fillrect(struct mpp_ge *ge, struct ge_fillrect *fill)
{ (void)ge; captured_fill = *fill; fills++; return fail_at == 1 ? -1 : 0; }

/* Synchronous mocks alone may drain and reset a faulted decoder. */
static void reset_decoder_fault(void)
{
    assert(decoder_quarantined);
    lv_image_decoder_close(&image_decoder);
    decoder_quarantined = false;
}
static const char lifetime_file[] = "Z:/dma-lifetime.test";
static const uint8_t lifetime_encoded[] = {0x42};
static const lv_image_dsc_t lifetime_raw = {
    .header = {.magic=LV_IMAGE_HEADER_MAGIC, .cf=LV_COLOR_FORMAT_RAW, .w=32, .h=32},
    .data=lifetime_encoded, .data_size=sizeof(lifetime_encoded)
};
static unsigned lifetime_opens, lifetime_closes;
static lv_draw_buf_t *lifetime_pixels;
static lv_result_t lifetime_info(lv_image_decoder_t *dec, lv_image_decoder_dsc_t *dsc,
                                 lv_image_header_t *header)
{
    (void)dec;
    if (dsc->src != &lifetime_raw &&
        !(dsc->src_type == LV_IMAGE_SRC_FILE && !strcmp(dsc->src, lifetime_file)))
        return LV_RESULT_INVALID;
    *header = (lv_image_header_t){.magic=LV_IMAGE_HEADER_MAGIC,
        .cf=LV_COLOR_FORMAT_ARGB8888, .w=32, .h=32, .stride=128};
    return LV_RESULT_OK;
}
static lv_result_t lifetime_open(lv_image_decoder_t *dec, lv_image_decoder_dsc_t *dsc)
{
    (void)dec;
    assert(!lifetime_pixels);
    lifetime_pixels = lv_draw_buf_create(32,32,LV_COLOR_FORMAT_ARGB8888,128);
    assert(lifetime_pixels);
    memset(lifetime_pixels->data,0x3c,lifetime_pixels->data_size);
    allowed_src = lifetime_pixels->data;
    dsc->decoded = lifetime_pixels;
    dsc->user_data = dsc; /* Detect moving a decoder descriptor after open. */
    lifetime_opens++;
    return LV_RESULT_OK;
}
static void lifetime_close(lv_image_decoder_t *dec, lv_image_decoder_dsc_t *dsc)
{
    (void)dec;
    assert(dsc->user_data == dsc && dsc->decoded == lifetime_pixels);
    lv_draw_buf_destroy(lifetime_pixels); lifetime_pixels = NULL;
    dsc->decoded = NULL; lifetime_closes++;
}
static void *lifetime_fs_open(lv_fs_drv_t *drv,const char *path,lv_fs_mode_t mode)
{ (void)drv; (void)path; (void)mode; return (void *)(uintptr_t)1; }
static lv_fs_res_t lifetime_fs_close(lv_fs_drv_t *drv,void *file)
{ (void)drv; (void)file; return LV_FS_RES_OK; }
static lv_fs_res_t lifetime_fs_seek(lv_fs_drv_t *drv,void *file,uint32_t pos,lv_fs_whence_t whence)
{ (void)drv; (void)file; (void)pos; (void)whence; return LV_FS_RES_OK; }
static void generic_decoder_lifetime(lv_layer_t *layer)
{
    static lv_fs_drv_t fs;
    lv_fs_drv_init(&fs); fs.letter='Z'; fs.open_cb=lifetime_fs_open;
    fs.close_cb=lifetime_fs_close; fs.seek_cb=lifetime_fs_seek;
    lv_fs_drv_register(&fs);
    lv_image_decoder_t *dec = lv_image_decoder_create();
    lv_image_decoder_set_info_cb(dec,lifetime_info);
    lv_image_decoder_set_open_cb(dec,lifetime_open);
    lv_image_decoder_set_close_cb(dec,lifetime_close);
    lv_draw_image_dsc_t d;
    lv_draw_task_t task = {0};
    lv_draw_image_dsc_init(&d);
    d.header = (lv_image_header_t){.magic=LV_IMAGE_HEADER_MAGIC,
        .cf=LV_COLOR_FORMAT_ARGB8888,.w=32,.h=32,.stride=128};
    d.image_area = (lv_area_t){0,0,31,31};
    task.type = LV_DRAW_TASK_TYPE_IMAGE; task.draw_dsc = &d;
    task.target_layer = layer; task.area = task.clip_area = d.image_area;
    for (unsigned raw=0;raw<2;raw++) for (unsigned tile=0;tile<2;tile++) {
        d.src = raw ? (const void *)&lifetime_raw : lifetime_file; d.tile = tile;
        for (fail_at=1;fail_at<=3;fail_at++) {
            unsigned old_closes=lifetime_closes;
            lv_draw_aic_ge2d_outcome_t outcome;
            assert(lv_draw_aic_ge2d_image(&task,&outcome)==LV_RESULT_INVALID);
            assert(lv_draw_aic_ge2d_image_faulted() && lifetime_pixels);
            assert(lifetime_closes==old_closes && lifetime_opens==old_closes+1);
            lv_image_cache_drop(d.src);
            assert(lifetime_pixels->data[0]==0x3c);
            int before=submits;
            assert(lv_draw_aic_ge2d_image(&task,&outcome)==LV_RESULT_INVALID);
            assert(submits==before && lifetime_closes==old_closes);
            lv_draw_aic_ge2d_deinit(); lv_draw_aic_ge2d_init();
            assert(lifetime_pixels->data[0]==0x3c && lifetime_closes==old_closes);
            reset_decoder_fault();
            assert(lifetime_closes==lifetime_opens && !lifetime_pixels);
        }
        fail_at=0;
        lv_draw_aic_ge2d_outcome_t outcome;
        assert(lv_draw_aic_ge2d_image(&task,&outcome)==LV_RESULT_OK);
        assert(outcome==LV_DRAW_AIC_GE2D_OUTCOME_ENGINE && lifetime_opens==lifetime_closes);
        lv_image_cache_drop(d.src);
    }
    lv_image_decoder_delete(dec);
}
int main(void)
{
    combined_transform_contract();
    wide_rotation_contract();
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
    d.scale_x=384; d.scale_y=512; d.pivot=(lv_point_t){16,16}; d.rotation=900;
    clip=(lv_area_t){101,220,111,230};
    assert(lv_draw_aic_ge2d_blit(&task,&d,&decoder,&origin,&clip));
    assert(captured.ctrl.flags==MPP_ROTATION_90);
    assert(captured.scale_phase.dx_16[0]==43690 && captured.scale_phase.dy_16[0]==32768);
    assert(captured.src_buf.crop.x==18 && captured.src_buf.crop.y==18);
    assert(captured.scale_phase.h_phase_16[0]==43690 && captured.scale_phase.v_phase_16[0]==32768);
    d.rotation=0;
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
        reset_decoder_fault();
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
        reset_decoder_fault();
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
        d.rotation = 900; assert(lv_draw_aic_ge2d_accepts_image(&task));
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
        reset_decoder_fault();
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
        /* Even RGB channel endpoints cannot establish whether GE compares
         * packed RGB565 or expanded RGB888; keep all corners on software. */
        for(unsigned corner=0;corner<8;corner++) {
            key.low=key.high=lv_color_make(corner&4?255:0,corner&2?255:0,corner&1?255:0);
            int count=submits;
            assert(!lv_draw_aic_ge2d_blit(&task,&d,&decoder,&origin,&clip));
            assert(submits==count);
        }
        key.low=key.high=lv_color_make(128,0,255);
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
            reset_decoder_fault();
        }
        fail_submission = 0;
        before = submits;
        task.clip_area = (lv_area_t){200, 300, 210, 310};
        assert(lv_draw_aic_ge2d_image(&task, &outcome) == LV_RESULT_OK);
        assert(outcome == LV_DRAW_AIC_GE2D_OUTCOME_NOTHING && submits == before);
        /* Transform each native grid cell, matching upstream helper coordinates.
         * Independent floating inverse checks phases with nonzero pivot and
         * anisotropic scales for all four orthogonal rotations. */
        task.clip_area=(lv_area_t){95,195,148,248};
        d.scale_x=384; d.scale_y=512; d.pivot=(lv_point_t){16,16};
        for(int angle=0;angle<3600;angle+=900) {
            d.rotation=angle; oracle_count=tile_count=0;
            lv_draw_image_tiled_helper(&task,&d,&task.area,tile_oracle,NULL);
            before=submits; record_tiles=true;
            assert(lv_draw_aic_ge2d_image(&task,&outcome)==LV_RESULT_OK);
            record_tiles=false;
            assert(outcome==LV_DRAW_AIC_GE2D_OUTCOME_ENGINE && tile_count==oracle_count && tile_count==4);
            for(unsigned i=0;i<tile_count;i++) {
                struct ge_bitblt *b=&tile_history[i]; lv_area_t *c=&oracle_clips[i];
                assert(b->dst_buf.crop.x==c->x1-layer.buf_area.x1);
                assert(b->dst_buf.crop.y==c->y1-layer.buf_area.y1);
                assert(b->dst_buf.crop.width==lv_area_get_width(c) && b->dst_buf.crop.height==lv_area_get_height(c));
                double minx=1e6,miny=1e6,r=angle*3.141592653589793/1800;
                for(unsigned corner=0;corner<4;corner++) {
                    double x=((corner&1)?c->x2:c->x1)-oracle_cells[i].x1-d.pivot.x;
                    double y=((corner&2)?c->y2:c->y1)-oracle_cells[i].y1-d.pivot.y;
                    double sx=d.pivot.x+(cos(r)*x+sin(r)*y)*256/d.scale_x;
                    double sy=d.pivot.y+(-sin(r)*x+cos(r)*y)*256/d.scale_y;
                    if(sx<minx) minx=sx;
                    if(sy<miny) miny=sy;
                }
                assert(fabs(b->src_buf.crop.x+b->scale_phase.h_phase_16[0]/65536.0-minx)<0.00025);
                assert(fabs(b->src_buf.crop.y+b->scale_phase.v_phase_16[0]/65536.0-miny)<0.00025);
                assert(b->scale_phase.dx_16[0]==43690 && b->scale_phase.dy_16[0]==32768);
            }
        }
        d.scale_x=d.scale_y=256; d.rotation=450;
        before=rotate_submits;
        assert(lv_draw_aic_ge2d_image(&task,&outcome)==LV_RESULT_OK);
        assert(outcome==LV_DRAW_AIC_GE2D_OUTCOME_ENGINE && rotate_submits==before+4);
        d.rotation=0; d.scale_x=d.scale_y=512; d.pivot=(lv_point_t){0,0};
        for(int tile=1;tile<=4;tile++) {
            before=submits; fail_submission=before+tile;
            assert(lv_draw_aic_ge2d_image(&task,&outcome)==LV_RESULT_INVALID);
            assert(outcome!=LV_DRAW_AIC_GE2D_OUTCOME_SOFTWARE && submits==before+tile);
            reset_decoder_fault();
        }
        fail_submission=0;
        /* Earlier cells are supported, but the last is narrower than scaler
         * limits. Preflight must decline before writing those earlier cells. */
        task.area.x2=154; task.clip_area.x2=154; before=submits;
        assert(lv_draw_aic_ge2d_tiles(&task,&d,&decoder)==0 && submits==before);
        task.area.x2=153; task.clip_area.x2=148;
        d.scale_x=d.scale_y=256;
        /* A huge task with a tiny visible clip must skip invisible tiles. */
        task.area = (lv_area_t){-1000000000, -1000000000, 1000000000, 1000000000};
        task.clip_area = (lv_area_t){95, 195, 100, 200};
        assert(lv_draw_aic_ge2d_image(&task, &outcome) == LV_RESULT_OK);
        assert(outcome == LV_DRAW_AIC_GE2D_OUTCOME_ENGINE && submits == before + 1);
        lv_image_cache_drop(&src);
    }
    {
        lv_draw_aic_ge2d_outcome_t outcome;
        lv_draw_image_dsc_init(&d);
        d.src = "L:/8x8_0_00123456.fake";
        layer.color_format = LV_COLOR_FORMAT_ARGB8888;
        d.opa = 0; /* SDK uses encoded alpha, not image opacity. */
        task.area = (lv_area_t){110,210,117,217};
        task.clip_area = (lv_area_t){112,212,115,215};
        task.draw_dsc = &d; task.type = LV_DRAW_TASK_TYPE_IMAGE;
        task.target_layer = &layer;
        assert(lv_draw_buf_init(&dst,128,128,LV_COLOR_FORMAT_ARGB8888,512,
                                output,sizeof(output)) == LV_RESULT_OK);
        allowed_dst = output;
        int before = fills;
        assert(lv_draw_aic_ge2d_image(&task, &outcome) == LV_RESULT_OK);
        assert(outcome == LV_DRAW_AIC_GE2D_OUTCOME_ENGINE && fills == before + 1);
        assert(captured_fill.start_color == 0x00123456 && captured_fill.ctrl.alpha_en == 0);
        assert(captured_fill.dst_buf.crop.x == 22 && captured_fill.dst_buf.crop.y == 22);
        assert(captured_fill.dst_buf.crop.width == 4 && captured_fill.dst_buf.crop.height == 4);
        memset(output, 0xa5, sizeof(output));
        for (fail_at = 1; fail_at <= 3; fail_at++) {
            assert(lv_draw_aic_ge2d_image(&task, &outcome) == LV_RESULT_INVALID);
            assert(outcome == LV_DRAW_AIC_GE2D_OUTCOME_NOTHING);
            for (unsigned i = 0; i < sizeof(output); i++) assert(output[i] == 0xa5);
            assert(lv_draw_aic_ge2d_fill_faulted());
            /* Synchronous mock only: a real DMA fault requires reboot. */
            fill_dma_faulted = false;
        }
        fail_at = 0;
        d.rotation = 900; task.clip_area = layer.buf_area;
        assert(lv_draw_aic_ge2d_image(&task, &outcome) == LV_RESULT_OK);
        assert(captured_fill.dst_buf.crop.x == 13 && captured_fill.dst_buf.crop.y == 20);
        assert(captured_fill.dst_buf.crop.width == 8 && captured_fill.dst_buf.crop.height == 8);
        d.rotation = 0;
        task.clip_area = (lv_area_t){112,212,115,215};
        g_ge2d_dev = NULL; g_ge2d_ready = false;
        task.preference_score = 100;
        assert(lv_draw_aic_ge2d_evaluate(NULL, &task) == 1);
        assert(task.preferred_draw_unit_id == AIC_GE2D_DRAW_UNIT_ID);
        for (unsigned f = 0; f < sizeof(formats)/sizeof(formats[0]); f++) {
            unsigned bpp = lv_color_format_get_size(formats[f]);
            layer.color_format = formats[f];
            assert(lv_draw_buf_init(&dst,128,128,formats[f],512,output,sizeof(output)) == LV_RESULT_OK);
            memset(output, 0xa5, sizeof(output));
            assert(lv_draw_aic_ge2d_image(&task, &outcome) == LV_RESULT_OK);
            assert(outcome == LV_DRAW_AIC_GE2D_OUTCOME_SOFTWARE);
            for (unsigned y = 0; y < 128; y++) {
                for (unsigned byte = 0; byte < 512; byte++) {
                    unsigned x = byte / bpp, c = byte % bpp;
                    uint8_t expected = 0xa5;
                    if (y >= 22 && y <= 25 && x >= 22 && x <= 25) {
                        const uint8_t argb[] = {0x56,0x34,0x12,0x00};
                        const uint8_t rgb565[] = {0xaa,0x11};
                        expected = bpp == 2 ? rgb565[c] : argb[c];
                    }
                    assert(output[y*512 + byte] == expected);
                }
            }
        }
        assert(lv_draw_buf_init(&dst,128,128,LV_COLOR_FORMAT_ARGB8888,512,
                                output,sizeof(output)) == LV_RESULT_OK);
        memset(output, 0, sizeof(output));
        d.src = "L:/8x8_1_80123456.fake";
        layer.color_format = LV_COLOR_FORMAT_ARGB8888;
        assert(lv_draw_aic_ge2d_image(&task, &outcome) == LV_RESULT_OK);
        assert(outcome == LV_DRAW_AIC_GE2D_OUTCOME_SOFTWARE);
        const uint8_t *p = output + 22*512 + 22*4;
        assert(p[0] == 0x56 && p[1] == 0x34 && p[2] == 0x12 && p[3] == 0x80);
        task.clip_area = (lv_area_t){0,0,1,1};
        assert(lv_draw_aic_ge2d_image(&task, &outcome) == LV_RESULT_OK);
        assert(outcome == LV_DRAW_AIC_GE2D_OUTCOME_NOTHING);
    }
    /* A quarantined client must not turn a fake image into a CPU fallback
     * that could race the outstanding DMA. Exercise the real image executor. */
    for (unsigned kind = 0; kind < 2; kind++) {
        lv_draw_aic_ge2d_outcome_t outcome;
        fill_dma_faulted = kind == 0;
        if (kind == 1) lv_draw_aic_ge2d_quarantine();
        task.clip_area = task.area;
        memset(output, 0xa5, sizeof(output));
        int old_fills = fills, old_submits = submits;
        assert(lv_draw_aic_ge2d_image(&task, &outcome) == LV_RESULT_INVALID);
        assert(outcome == LV_DRAW_AIC_GE2D_OUTCOME_NOTHING);
        assert(fills == old_fills && submits == old_submits);
        for (unsigned i = 0; i < sizeof(output); i++) assert(output[i] == 0xa5);
        /* Only synchronous mocks can safely reset fault state. */
        fill_dma_faulted = g_ge2d_external_fault = false;
    }
    g_ge2d_dev=mpp_ge_open(); g_ge2d_ready=true;
    assert(lv_aic_rgb_image_decoder_init());
    for(unsigned copy=0;copy<2;copy++) for(unsigned tiled=0;tiled<2;tiled++) for(int failure=1;failure<=3;failure++) {
        lv_aic_rgb_frame_t rgb={LV_COLOR_FORMAT_ARGB8888,copy?31:32,32,128,pixels,copy?4092:4096};
        lv_aic_rgb_image_t *image=lv_aic_rgb_image_create(&rgb,rgb_retain,rgb_release,NULL); assert(image);
        const lv_image_dsc_t *source=lv_aic_rgb_image_source(image);
        lv_image_decoder_args_t args={.stride_align=false,.premultiply=false};
        lv_image_decoder_dsc_t probe;
        assert(lv_image_decoder_open(&probe,source,&args)==LV_RESULT_OK);
        allowed_src=probe.decoded->data;
        assert((allowed_src!=pixels)==(copy!=0));
        const uint8_t *snapshot=allowed_src; uint8_t first=snapshot[0];
        lv_image_decoder_close(&probe);
        assert(lv_draw_buf_init(&dst,128,128,LV_COLOR_FORMAT_ARGB8888,512,output,sizeof(output))==LV_RESULT_OK);
        layer.draw_buf=&dst; layer.buf_area=(lv_area_t){0,0,127,127}; layer.color_format=LV_COLOR_FORMAT_ARGB8888;
        allowed_dst=output; fail_at=0; fail_submission=0; rotate_fail=0; record_tiles=false;
        lv_draw_image_dsc_init(&d); d.src=source; d.tile=tiled;
        d.header.w=rgb.width; d.header.h=rgb.height; d.header.cf=rgb.format;
        d.image_area=(lv_area_t){0,0,(int32_t)rgb.width-1,31};
        task.type=LV_DRAW_TASK_TYPE_IMAGE; task.draw_dsc=&d; task.target_layer=&layer;
        task.area=d.image_area; task.clip_area=d.image_area;
        lv_draw_aic_ge2d_outcome_t outcome;
        assert(lv_draw_aic_ge2d_image(&task,&outcome)==LV_RESULT_OK && outcome==LV_DRAW_AIC_GE2D_OUTCOME_ENGINE);
        fail_at=failure;
        assert(lv_draw_aic_ge2d_image(&task,&outcome)==LV_RESULT_INVALID && lv_draw_aic_ge2d_image_faulted());
        lv_aic_rgb_image_destroy(image);
        assert(rgb_retained==rgb_released+1 && !lv_aic_rgb_image_decoder_deinit());
        assert(snapshot[0]==first); /* Copied decode storage survived decoder close too. */
        int before=submits;
        assert(lv_draw_aic_ge2d_image(&task,&outcome)==LV_RESULT_INVALID && submits==before);
        /* Mock engine has no DMA. Only the test may release/reset quarantine. */
        reset_decoder_fault();
        lv_aic_rgb_image_release_lease(rgb_quarantined); rgb_quarantined=NULL;
        assert(rgb_retained==rgb_released);
    }
    assert(lv_aic_rgb_image_decoder_deinit());
    fail_at = 0; generic_decoder_lifetime(&layer);
    lv_deinit();
    return 0;
}
