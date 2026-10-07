/* SPDX-License-Identifier: Apache-2.0 */
#include <assert.h>
#include <stdio.h>
#include "../manual/lv_aic_svg_document_test.c"
static void flush(lv_display_t *d,const lv_area_t *a,uint8_t *p) {(void)a;(void)p;lv_display_flush_ready(d);}
int main(void)
{
    lv_init();lv_display_t *display=lv_display_create(64,64);assert(display);
    static uint32_t pixels[64*64];
    lv_display_set_color_format(display,LV_COLOR_FORMAT_ARGB8888);
    lv_display_set_buffers(display,pixels,NULL,sizeof pixels,LV_DISPLAY_RENDER_MODE_DIRECT);
    lv_display_set_flush_cb(display,flush);
    for(unsigned cycle=0;cycle<10;cycle++) {
        lv_layer_t *head=display->layer_head;
        unsigned memory=LV_GLOBAL_DEFAULT()->draw_info.used_memory_for_layers;
        assert(lv_aic_svg_document_test_run()==0);
        assert(display->layer_head==head && !head->next && LV_GLOBAL_DEFAULT()->draw_info.used_memory_for_layers==memory);
    }
    lv_display_delete(display);lv_deinit();
    puts("PASS 440 actual board SVG document probe scenes through native canvas dispatch");return 0;
}
