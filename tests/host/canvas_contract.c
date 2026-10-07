/* SPDX-License-Identifier: Apache-2.0 */
#include <assert.h>
#define AIC_LVGL_USE_PRIVATE_API 1
#include "lvgl_aic_private.h"
static bool fail_alloc;
static unsigned allocations, releases;
static void *canvas_alloc(size_t size)
{
    if(fail_alloc) return NULL;
    void *p=lv_malloc(size); if(p) allocations++; return p;
}
static void canvas_free(void *p) { if(p) releases++; lv_free(p); }
#define lv_malloc canvas_alloc
#define lv_free canvas_free
#include "../../widgets/lv_aic_canvas.c"
#undef lv_malloc
#undef lv_free
int main(void)
{
    lv_init();
    lv_display_t *display=lv_display_create(320,240); assert(display);
    for(unsigned cycle=0;cycle<20;cycle++) {
        lv_obj_t *obj=lv_aic_canvas_create(lv_screen_active()); assert(obj);
        aic_canvas_t *c=(aic_canvas_t *)obj;
        assert(lv_aic_canvas_set_budget(obj,16384));
        assert(lv_aic_canvas_alloc_buffer(obj,64,32)==LV_RESULT_OK);
        assert(c->bytes==8192 && c->buffer.header.stride==256);
        for(unsigned i=0;i<c->bytes;i++) assert(((uint8_t *)c->pixels)[i]==0);
        lv_draw_label_dsc_t label; lv_draw_label_dsc_init(&label);
        label.color=lv_color_white();
        lv_aic_canvas_draw_text_to_center(obj,&label,"Canvas");
        unsigned visible=0;
        for(unsigned i=3;i<c->bytes;i+=4) visible+=((uint8_t *)c->pixels)[i]!=0;
        assert(visible>10 && label.text==NULL);
        lv_aic_canvas_draw_text_to_center(obj,&label,"");
        for(unsigned i=0;i<c->bytes;i++) assert(((uint8_t *)c->pixels)[i]==0);
        void *old=c->pixels;
        fail_alloc=true;
        assert(lv_aic_canvas_alloc_buffer(obj,17,32)==LV_RESULT_INVALID && c->pixels==old);
        fail_alloc=false;
        assert(lv_aic_canvas_alloc_buffer(obj,4096,4096)==LV_RESULT_INVALID);
        assert(lv_aic_canvas_alloc_buffer(obj,-1,32)==LV_RESULT_INVALID);
        assert(!lv_aic_canvas_set_budget(obj,8191) && c->pixels==old);
        assert(lv_aic_canvas_alloc_buffer(obj,17,32)==LV_RESULT_OK);
        assert(c->pixels!=old && c->bytes==4096 && c->buffer.header.stride==128);
        lv_aic_canvas_draw_text(obj,-4,2,17,&label,"wrap text");
        lv_obj_delete(obj);
        assert(allocations==releases);
    }
    lv_display_delete(display); lv_deinit(); return 0;
}
