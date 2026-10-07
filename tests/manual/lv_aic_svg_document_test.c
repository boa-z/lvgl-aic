/* SPDX-License-Identifier: Apache-2.0 */
#define AIC_LVGL_USE_PRIVATE_API 1
#define LOG_TAG "lvgl.svg.document"
#define LOG_LVL LOG_LVL_INFO
#include "lv_aic_manual_test.h"
#if LV_USE_SVG && AIC_LVGL_BSP_RTTHREAD
#include "lvgl_aic_private.h"
#include "src/image/svg/lv_svg_render.h"
#include "lv_aic_test_log.h"
#include "../common/lv_aic_svg_document_cases.h"
#include <string.h>

int lv_aic_svg_document_test_run(void)
{
    lv_obj_t *canvas=lv_canvas_create(lv_screen_active());
    if(!canvas) return -1;
    lv_obj_set_hidden(canvas,true);
    lv_draw_buf_t *buffer=lv_draw_buf_create(64,64,LV_COLOR_FORMAT_ARGB8888,0);
    if(!buffer) {lv_obj_delete(canvas);return -1;}
    lv_canvas_set_draw_buf(canvas,buffer);
    int result=-1;
    char document[1024];
    for(unsigned image=0;image<2;image++) {
        for(unsigned i=0;i<sizeof svg_document_cases/sizeof svg_document_cases[0];i++) {
            int n=lv_snprintf(document,sizeof document,
                "<svg width='64' height='64' xmlns='http://www.w3.org/2000/svg' xmlns:xlink='http://www.w3.org/1999/xlink'>%s</svg>",
                svg_document_cases[i].body);
            if(!strncmp(svg_document_cases[i].body,"<svg ",5))
                n=lv_snprintf(document,sizeof document,"%s",svg_document_cases[i].body);
            if(n<=0 || (unsigned)n>=sizeof document) goto done;
            lv_canvas_fill_bg(canvas,lv_color_black(),255);
            lv_layer_t layer;lv_canvas_init_layer(canvas,&layer);
            if(image) {
                lv_image_dsc_t source={.header={.magic=LV_IMAGE_HEADER_MAGIC,.cf=LV_COLOR_FORMAT_RAW,.w=64,.h=64},
                    .data=(const uint8_t *)document,.data_size=(uint32_t)n};
                lv_draw_image_dsc_t d;lv_draw_image_dsc_init(&d);d.src=&source;
                lv_area_t area={0,0,63,63};lv_draw_image(&layer,&d,&area);lv_canvas_finish_layer(canvas,&layer);
                lv_image_cache_drop(&source);lv_image_header_cache_drop(&source);
            }
            else {
                lv_svg_node_t *doc=lv_svg_load_data(document,n);
                if(!doc) goto done;
                lv_draw_svg(&layer,doc);lv_canvas_finish_layer(canvas,&layer);lv_svg_node_delete(doc);
            }
            for(unsigned sample=0;sample<3;sample++) {
                lv_color32_t p=lv_canvas_get_px(canvas,svg_document_cases[i].sample[sample].x,
                                               svg_document_cases[i].sample[sample].y);
                uint32_t expected=svg_document_cases[i].sample[sample].rgb;
                const int errors[]={ (int)p.red-(int)((expected>>16)&255),
                    (int)p.green-(int)((expected>>8)&255),(int)p.blue-(int)(expected&255)};
                for(unsigned c=0;c<3;c++) if(errors[c]<-2 || errors[c]>2) {
                    AIC_TEST_E("FAIL SVG doc image=%u case=%s sample=%u channel=%u error=%d",
                               image,svg_document_cases[i].name,sample,c,errors[c]);goto done;
                }
            }
            AIC_TEST_I("PASS SVG doc image=%u case=%s samples=3",image,svg_document_cases[i].name);
        }
    }
    AIC_TEST_I("PASS %u SVG document probes; heap stress and timing acceptance remain separate",
               (unsigned)(2*sizeof svg_document_cases/sizeof svg_document_cases[0]));
    result=0;
done:
    lv_obj_delete(canvas);lv_draw_buf_destroy(buffer);
    if(result) AIC_TEST_E("FAIL SVG document probes");
    return result;
}
#endif
