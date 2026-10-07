/* SPDX-License-Identifier: Apache-2.0 */
#include <assert.h>
#include <windows.h>
#define AIC_LVGL_GE_DRAW_BUF_BUDGET 8000U
#include "../../draw/ge2d/lv_aic_ge2d_draw_buf.c"
static unsigned allocs,frees;
static bool fault,fail_alloc;
bool lv_draw_aic_ge2d_faulted(void) { return fault; }
void *aicos_malloc_align(unsigned int type,size_t size,size_t alignment)
{
    assert(type==MEM_CMA && alignment>=64);
    if(fail_alloc) return NULL;
    void *p=VirtualAlloc((void *)(uintptr_t)(0x48000000+allocs*0x10000),size,
                         MEM_RESERVE|MEM_COMMIT,PAGE_READWRITE);
    assert(p);allocs++;return p;
}
void aicos_free_align(unsigned int type,void *p)
{ assert(type==MEM_CMA);assert(VirtualFree(p,0,MEM_RELEASE));frees++; }
int main(void)
{
    lv_init();
    lv_draw_buf_handlers_t saved=*lv_draw_buf_get_handlers();
    const lv_color_format_t formats[]={LV_COLOR_FORMAT_RGB565,LV_COLOR_FORMAT_RGB888,
        LV_COLOR_FORMAT_XRGB8888,LV_COLOR_FORMAT_ARGB8888};
    for(unsigned i=0;i<4;i++) {
        lv_draw_buf_t *b=__wrap_lv_draw_buf_create(17,11,formats[i],0);assert(b);
        assert((uintptr_t)b->data>=0x48000000 && (uintptr_t)b->data<=UINT32_MAX);
        assert(used==b->data_size && b->handlers==&handlers);
        lv_draw_buf_clear(b,NULL);
        for(unsigned y=0;y<11;y++) for(unsigned x=0;x<17*lv_color_format_get_size(formats[i]);x++)
            assert(b->data[y*b->header.stride+x]==0);
        lv_draw_buf_destroy(b);assert(!used && !allocations && allocs==frees);
    }
    lv_draw_buf_t *a=__wrap_lv_draw_buf_create(32,32,LV_COLOR_FORMAT_ARGB8888,0);assert(a);
    lv_draw_buf_t *b=__wrap_lv_draw_buf_create(32,32,LV_COLOR_FORMAT_ARGB8888,0);assert(b);
    assert(b->handlers!=&handlers && used==a->data_size);
    lv_draw_buf_destroy(b);lv_draw_buf_destroy(a);assert(!used);
    fail_alloc=true;
    b=__wrap_lv_draw_buf_create(8,8,LV_COLOR_FORMAT_RGB888,0);assert(b && b->handlers!=&handlers);
    lv_draw_buf_destroy(b);fail_alloc=false;
    b=__wrap_lv_draw_buf_create(8,8,LV_COLOR_FORMAT_A8,0);assert(b && b->handlers!=&handlers);
    lv_draw_buf_destroy(b);
    b=__wrap_lv_draw_buf_create(8,8,LV_COLOR_FORMAT_ARGB8888,0);assert(b);
    void *pixels=b->data;size_t bytes=used;unsigned released=frees;
    fault=true;lv_draw_buf_destroy(b);assert(frees==released && used==bytes && allocations);
    assert(!__wrap_lv_draw_buf_create(8,8,LV_COLOR_FORMAT_RGB888,0));
    fault=false;release(pixels); /* Mock-only proof of quiescence. */
    assert(!used && allocs==frees);
    assert(!memcmp(&saved,lv_draw_buf_get_handlers(),sizeof(saved)));
    lv_layer_t layer={0};layer.buf_area=(lv_area_t){0,0,16,10};
    layer.color_format=LV_COLOR_FORMAT_ARGB8888;
    assert(lv_draw_layer_alloc_buf(&layer));
    assert(layer.draw_buf->handlers==&handlers && layer.buffer_owned);
    for(unsigned i=0;i<17*4;i++) assert(layer.draw_buf->data[i]==0);
    lv_draw_layer_dealloc_buf(&layer);assert(!used && allocs==frees);
    lv_deinit();lv_init();
    b=__wrap_lv_draw_buf_create(8,8,LV_COLOR_FORMAT_RGB565,0);assert(b);
    lv_draw_buf_destroy(b);assert(!used && allocs==frees);lv_deinit();
    return 0;
}
