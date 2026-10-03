/* SPDX-License-Identifier: Apache-2.0 */
#include <assert.h>
#include <stdint.h>
#include <string.h>
#define ulong uintptr_t
#include "../../draw/ge2d/lv_draw_aic_ge2d.c"
#include "../../draw/ge2d/lv_draw_aic_ge2d_fill.c"

static struct ge_fillrect captured;
static lv_area_t cache_area;
static int submits, emits, syncs, caches, fail_at;
static const void *allowed_dst;
static bool yuv_fault, rgb_fault;
static int image_calls;
bool lv_draw_aic_ge2d_image_faulted(void) { return rgb_fault; }
bool lv_draw_aic_ge2d_yuv_faulted(void) { return yuv_fault; }
struct mpp_ge *mpp_ge_open(void) { return (struct mpp_ge *)(uintptr_t)1; }
void mpp_ge_close(struct mpp_ge *ge) { (void)ge; }
int mpp_ge_fillrect(struct mpp_ge *ge, struct ge_fillrect *f)
{ (void)ge; captured = *f; submits++; return fail_at == 1 ? -1 : 0; }
int mpp_ge_emit(struct mpp_ge *ge)
{ (void)ge; emits++; return fail_at == 2 ? -1 : 0; }
int mpp_ge_sync(struct mpp_ge *ge)
{ (void)ge; syncs++; return fail_at == 3 ? -1 : 0; }
bool lv_draw_aic_ge2d_buf_address_valid(const lv_draw_buf_t *b)
{ return b && b->data && b->data == allowed_dst; }
bool lv_draw_aic_ge2d_dst_format_supported(lv_color_format_t cf)
{ return lv_aic_pixel_format_is_ge2d_dst(cf); }
void lv_draw_aic_ge2d_prepare_dst_cache(const lv_draw_buf_t *b, const lv_area_t *a)
{ (void)b; cache_area = *a; caches++; }
lv_result_t lv_draw_aic_ge2d_image(lv_draw_task_t *t, lv_draw_aic_ge2d_outcome_t *o)
{ (void)t; (void)o; image_calls++; return LV_RESULT_INVALID; }
static void dispatcher_failure_contract(lv_layer_t *layer)
{
    for (unsigned fatal = 0; fatal < 3; fatal++) {
        lv_draw_aic_ge2d_unit_t unit = {0};
        lv_draw_task_t *task = lv_draw_add_task(layer, &layer->buf_area,
                                               LV_DRAW_TASK_TYPE_IMAGE);
        assert(task != NULL);
        task->preferred_draw_unit_id = AIC_GE2D_DRAW_UNIT_ID;
        yuv_fault = fatal == 1; rgb_fault = fatal == 2;
        image_calls = 0;
        lv_draw_aic_ge2d_stats_reset();
        assert(lv_draw_aic_ge2d_dispatch(&unit.base_unit, layer) == 1);
        assert(image_calls == 1 && g_ge2d_stats.errors == 1);
        assert(g_ge2d_stats.image_completed == 0);
        assert(task->draw_unit == &unit.base_unit);
        assert(task->state == (fatal ? LV_DRAW_TASK_STATE_IN_PROGRESS :
                                      LV_DRAW_TASK_STATE_FAILED));
        assert(unit.task_act == (fatal ? task : NULL));
        lv_draw_task_t *queued = NULL;
        if (fatal) {
            queued = lv_draw_add_task(layer, &layer->buf_area, LV_DRAW_TASK_TYPE_IMAGE);
            assert(queued != NULL);
            queued->preferred_draw_unit_id = AIC_GE2D_DRAW_UNIT_ID;
        }
        for (unsigned retry = 0; retry < 3; retry++) {
            assert(lv_draw_aic_ge2d_dispatch(&unit.base_unit, layer) == LV_DRAW_UNIT_IDLE);
            assert(image_calls == 1 && g_ge2d_stats.errors == 1);
            assert(layer->draw_buf->data == allowed_dst);
            if (queued) assert(queued->state == LV_DRAW_TASK_STATE_WAITING);
        }
        /* Only this synchronous mock can prove no DMA is pending. Never
         * release a quarantined production task this way. */
        layer->draw_task_head = NULL;
        lv_free(queued);
        lv_free(task);
    }
    yuv_fault = rgb_fault = false;
}
static void reset_calls(void) { submits = emits = syncs = caches = 0; }
static void verify_latched(lv_draw_task_t *task)
{
    int saved_failure = fail_at;
    assert(lv_draw_aic_ge2d_fill_faulted());
    fail_at = 0; reset_calls();
    assert(lv_draw_aic_ge2d_fill(task) == LV_RESULT_INVALID);
    assert(lv_draw_aic_ge2d_fill_replace(task, 0) == LV_RESULT_INVALID);
    assert(submits == 0 && emits == 0 && syncs == 0 && caches == 0);
    /* Test-only reset: synchronous mocks never start DMA. */
    fill_dma_faulted = false; fail_at = saved_failure;
}
static void fill_dispatch_failure(lv_layer_t *layer, lv_draw_fill_dsc_t *dsc)
{
    for (fail_at = 1; fail_at <= 3; fail_at++) {
        lv_draw_aic_ge2d_unit_t unit = {0};
        lv_draw_task_t *task = lv_draw_add_task(layer, &layer->buf_area,
                                               LV_DRAW_TASK_TYPE_FILL);
        task->draw_dsc = dsc;
        task->clip_area = layer->buf_area;
        task->preferred_draw_unit_id = AIC_GE2D_DRAW_UNIT_ID;
        reset_calls(); lv_draw_aic_ge2d_stats_reset();
        assert(lv_draw_aic_ge2d_dispatch(&unit.base_unit, layer) == 1);
        assert(task->state == LV_DRAW_TASK_STATE_IN_PROGRESS && unit.task_act == task);
        assert(g_ge2d_stats.errors == 1 && g_ge2d_stats.fill_completed == 0);
        assert(lv_draw_aic_ge2d_dispatch(&unit.base_unit, layer) == LV_DRAW_UNIT_IDLE);
        assert(submits == 1 && g_ge2d_stats.errors == 1);
        verify_latched(task);
        /* Mock-only cleanup, never valid for real uncertain DMA. */
        layer->draw_task_head = NULL; lv_free(task);
    }
    fail_at = 0;
}
static void rejected(lv_draw_task_t *t)
{
    reset_calls();
    assert(lv_draw_aic_ge2d_fill(t) == LV_RESULT_INVALID);
    assert(submits == 0 && emits == 0 && syncs == 0 && caches == 0);
}
int main(void)
{
    static uint8_t output[32 * 160];
    lv_draw_buf_t dst;
    lv_draw_fill_dsc_t d;
    lv_layer_t layer = {0};
    lv_draw_task_t task = {0};
    const lv_color_format_t formats[] = {LV_COLOR_FORMAT_RGB565,
        LV_COLOR_FORMAT_RGB888, LV_COLOR_FORMAT_XRGB8888, LV_COLOR_FORMAT_ARGB8888};
    const uint8_t opacities[] = {LV_OPA_MIN + 1, 64, 128, 192, LV_OPA_MAX - 1, LV_OPA_MAX, 255};
    lv_init();
    allowed_dst = output;
    g_ge2d_dev = mpp_ge_open(); g_ge2d_ready = true;
    lv_draw_fill_dsc_init(&d); d.color = lv_color_make(200,40,80);
    layer.draw_buf = &dst; layer.buf_area = (lv_area_t){100,200,131,231};
    task.target_layer = &layer; task.draw_dsc = &d; task.type = LV_DRAW_TASK_TYPE_FILL;
    task.area = (lv_area_t){95,195,120,220}; task.clip_area = (lv_area_t){90,198,110,210};
    for (unsigned f = 0; f < sizeof(formats)/sizeof(formats[0]); f++) {
        assert(lv_draw_buf_init(&dst,32,32,formats[f],160,output,sizeof(output)) == LV_RESULT_OK);
        for (unsigned i = 0; i < sizeof(opacities)/sizeof(opacities[0]); i++) {
            d.opa = opacities[i];
            if (formats[f] == LV_COLOR_FORMAT_ARGB8888 && d.opa < LV_OPA_MAX) {
                assert(!lv_draw_aic_ge2d_accepts_fill(&task)); rejected(&task); continue;
            }
            assert(lv_draw_aic_ge2d_accepts_fill(&task)); reset_calls();
            assert(lv_draw_aic_ge2d_fill(&task) == LV_RESULT_OK);
            assert(submits == 1 && emits == 1 && syncs == 1 && caches == 1);
            enum mpp_pixel_format fmt;
            assert(lv_aic_pixel_format_to_mpp(formats[f], &fmt));
            assert(captured.dst_buf.format == fmt);
            assert(captured.dst_buf.phy_addr[0] == (uint32_t)(uintptr_t)output);
            assert(captured.dst_buf.stride[0] == 160);
            assert(captured.dst_buf.crop.x == 0 && captured.dst_buf.crop.y == 0);
            assert(captured.dst_buf.crop.width == 11 && captured.dst_buf.crop.height == 11);
            assert(cache_area.x1 == 0 && cache_area.y1 == 0 && cache_area.x2 == 10 && cache_area.y2 == 10);
            assert(captured.ctrl.alpha_en == (d.opa < LV_OPA_MAX));
            assert(captured.ctrl.alpha_rules == GE_PD_NONE && captured.ctrl.src_alpha_mode == 0);
            assert((captured.start_color & 0xffffffU) == 0xc82850U);
            if(d.opa < LV_OPA_MAX) assert((captured.start_color >> 24) == d.opa);
        }
    }
    assert(lv_draw_buf_init(&dst,32,32,LV_COLOR_FORMAT_RGB888,160,output,sizeof(output)) == LV_RESULT_OK);
    d.opa = 128;
    task.preference_score = 100;
    assert(lv_draw_aic_ge2d_evaluate(NULL,&task) == 1);
    assert(task.preferred_draw_unit_id == AIC_GE2D_DRAW_UNIT_ID && task.preference_score == 70);
    d.radius = 2; assert(!lv_draw_aic_ge2d_accepts_fill(&task)); rejected(&task); d.radius = 0;
    d.grad.dir = LV_GRAD_DIR_HOR; assert(!lv_draw_aic_ge2d_accepts_fill(&task)); rejected(&task); d.grad.dir = LV_GRAD_DIR_NONE;
    allowed_dst = NULL; assert(!lv_draw_aic_ge2d_accepts_fill(&task)); rejected(&task); allowed_dst = output;
    dst.header.cf = LV_COLOR_FORMAT_A8; rejected(&task); dst.header.cf = LV_COLOR_FORMAT_RGB888;
    dst.header.stride = 95; rejected(&task); dst.header.stride = 160;
    dst.data_size--; rejected(&task); dst.data_size++;
    dst.header.w = 31; rejected(&task); dst.header.w = 32;
    dst.header.h = 31; rejected(&task); dst.header.h = 32;
    g_ge2d_dev = NULL; rejected(&task); g_ge2d_dev = mpp_ge_open();
    for (int opacity = 0; opacity <= LV_OPA_MIN; opacity++) {
        d.opa = opacity; reset_calls(); assert(!lv_draw_aic_ge2d_accepts_fill(&task));
        assert(lv_draw_aic_ge2d_fill(&task) == LV_RESULT_OK && submits == 0 && caches == 0);
    }
    d.opa = 128; task.clip_area = (lv_area_t){0,0,10,10}; reset_calls();
    assert(lv_draw_aic_ge2d_fill(&task) == LV_RESULT_OK && submits == 0 && caches == 0);
    task.area = task.clip_area; /* task/clip overlap, but entirely outside the layer */
    assert(lv_draw_aic_ge2d_fill(&task) == LV_RESULT_OK && submits == 0 && caches == 0);
    task.area = (lv_area_t){110,210,150,250}; task.clip_area = task.area;
    reset_calls(); assert(lv_draw_aic_ge2d_fill(&task) == LV_RESULT_OK);
    assert(captured.dst_buf.crop.x == 10 && captured.dst_buf.crop.y == 10);
    assert(captured.dst_buf.crop.width == 22 && captured.dst_buf.crop.height == 22);
    assert(cache_area.x2 == 31 && cache_area.y2 == 31);
    task.area = task.clip_area = layer.buf_area;
    for (fail_at = 1; fail_at <= 3; fail_at++) {
        reset_calls(); assert(lv_draw_aic_ge2d_fill(&task) == LV_RESULT_INVALID);
        assert(submits == 1 && emits == (fail_at >= 2) && syncs == (fail_at >= 3));
        verify_latched(&task);
    }
    fail_at = 0; task.draw_dsc = NULL; rejected(&task); task.draw_dsc = &d;
    /* SDK blend=0 must write alpha exactly, including zero; it is not a
     * source-over fill and must not disappear at LV_OPA_MIN/MAX thresholds. */
    for (unsigned f = 0; f < sizeof(formats)/sizeof(formats[0]); f++) {
        assert(lv_draw_buf_init(&dst,32,32,formats[f],160,output,sizeof(output)) == LV_RESULT_OK);
        for (unsigned alpha = 0; alpha <= 255; alpha++) {
            uint32_t argb = (alpha << 24) | 0x123456;
            d.opa = alpha;
            reset_calls();
            assert(lv_draw_aic_ge2d_fill_replace(&task, argb) == LV_RESULT_OK);
            assert(submits == 1 && emits == 1 && syncs == 1 && caches == 1);
            assert(captured.start_color == argb && !captured.ctrl.alpha_en);
        }
    }
    for (fail_at = 1; fail_at <= 3; fail_at++) {
        reset_calls();
        assert(lv_draw_aic_ge2d_fill_replace(&task, 0x00123456) == LV_RESULT_INVALID);
        assert(submits == 1 && emits == (fail_at >= 2) && syncs == (fail_at >= 3));
        verify_latched(&task);
    }
    fail_at = 0;
    allowed_dst = NULL;
    reset_calls();
    assert(lv_draw_aic_ge2d_fill_replace(&task, 0) == LV_RESULT_INVALID);
    assert(submits == 0 && caches == 0);
    allowed_dst = output;
    task.type = LV_DRAW_TASK_TYPE_IMAGE; rejected(&task); rejected(NULL);
    dispatcher_failure_contract(&layer);
    d.opa = 255; fill_dispatch_failure(&layer, &d);
    lv_deinit(); return 0;
}
