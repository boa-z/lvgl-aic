/* SPDX-License-Identifier: Apache-2.0 */
#include "lvgl.h"
#include "lvgl_private.h"
#include "src/image/svg/lv_svg_render.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../common/lv_aic_svg_document_cases.h"
/* Count LVGL heap bytes at its allocation boundary, including real array
 * copies. ThorVG's separate C++ heap is outside this particular leak oracle. */
static struct {void *p;size_t bytes;} live[8192];
static size_t bytes_live;
void *__real_lv_malloc_core(size_t);
void *__real_lv_realloc_core(void *,size_t);
void __real_lv_free_core(void *);
static void remember(void *p,size_t bytes)
{
    if(!p) return;
    for(unsigned i=0;i<8192;i++) if(!live[i].p) {live[i].p=p;live[i].bytes=bytes;bytes_live+=bytes;return;}
    assert(false);
}
static void forget(void *p)
{
    if(!p) return;
    for(unsigned i=0;i<8192;i++) if(live[i].p==p) {bytes_live-=live[i].bytes;live[i].p=NULL;return;}
    assert(false);
}
void *__wrap_lv_malloc_core(size_t size) {void *p=__real_lv_malloc_core(size);remember(p,size);return p;}
void *__wrap_lv_realloc_core(void *p,size_t size)
{
    void *next=__real_lv_realloc_core(p,size);
    if(next || !size) {forget(p);remember(next,size);}return next;
}
void __wrap_lv_free_core(void *p) {forget(p);__real_lv_free_core(p);}
/* Inject only at opacity-scope boundaries, after parser/render-list setup. */
static unsigned fail_kind, fail_at, fault_calls, layer_creates;
static bool drawing;
lv_layer_t *__real_lv_draw_layer_create(lv_layer_t *,lv_color_format_t,const lv_area_t *);
lv_layer_t *__wrap_lv_draw_layer_create(lv_layer_t *p,lv_color_format_t cf,const lv_area_t *a)
{
    if(drawing) {
        layer_creates++;
        if(fail_kind==1 && ++fault_calls==fail_at) return NULL;
    }
    return __real_lv_draw_layer_create(p,cf,a);
}
void *__real_lv_draw_layer_alloc_buf(lv_layer_t *);
void *__wrap_lv_draw_layer_alloc_buf(lv_layer_t *p)
{
    if(drawing && fail_kind==2 && ++fault_calls==fail_at) return NULL;
    return __real_lv_draw_layer_alloc_buf(p);
}
lv_draw_vector_dsc_t *__real_lv_draw_vector_dsc_create(lv_layer_t *);
lv_draw_vector_dsc_t *__wrap_lv_draw_vector_dsc_create(lv_layer_t *p)
{
    if(drawing && fail_kind==3 && ++fault_calls==fail_at) return NULL;
    return __real_lv_draw_vector_dsc_create(p);
}
static lv_obj_t *canvas;
static lv_display_t *display;
static void flush(lv_display_t *d,const lv_area_t *a,uint8_t *p)
{(void)a;(void)p;lv_display_flush_ready(d);}
#if LV_USE_FREETYPE
static const char *font_path(const char *family) {(void)family;return AIC_SVG_TEST_FONT;}
#endif
static unsigned image_mode;
static lv_opa_t background_opa=255,image_opa=255;
static void render(const char *body)
{
    char document[65536];
    int n=snprintf(document,sizeof document,"<svg width='64' height='64' xmlns='http://www.w3.org/2000/svg' xmlns:xlink='http://www.w3.org/1999/xlink'>%s</svg>",body);
    if(!strncmp(body,"<svg ",5)) n=snprintf(document,sizeof document,"%s",body);
    assert(n>0 && (unsigned)n<sizeof document);
    lv_canvas_fill_bg(canvas,lv_color_black(),background_opa);
    lv_layer_t *head=display->layer_head;
    lv_layer_t *next=head->next;
    uint32_t memory=LV_GLOBAL_DEFAULT()->draw_info.used_memory_for_layers;
    lv_layer_t layer;lv_canvas_init_layer(canvas,&layer);
    lv_svg_node_t *doc=lv_svg_load_data(document,n);assert(doc);
    /* Traverse native reference bounds too: paint servers can query them
     * independently of rendering, including invalid/cyclic definitions. */
    lv_svg_render_obj_t *list=lv_svg_render_create(doc);assert(list);
    for(lv_svg_render_obj_t *obj=list;obj;obj=obj->next) if(obj->tag==LV_SVG_TAG_USE) {
        lv_area_t bounds={0};obj->clz->get_bounds(obj,&bounds);
    }
    lv_svg_render_delete(list);
    drawing=true;fault_calls=0;layer_creates=0;
    if(image_mode) {
        lv_image_dsc_t src={.header={.magic=LV_IMAGE_HEADER_MAGIC,.cf=LV_COLOR_FORMAT_RAW,.w=64,.h=64},
            .data=(const uint8_t *)document,.data_size=n};
        lv_draw_image_dsc_t image;lv_draw_image_dsc_init(&image);image.src=&src;image.opa=image_opa;
        lv_area_t area={0,0,63,63};lv_draw_image(&layer,&image,&area);lv_canvas_finish_layer(canvas,&layer);
        lv_image_cache_drop(&src);lv_image_header_cache_drop(&src);
    }
    else {lv_draw_svg(&layer,doc);lv_canvas_finish_layer(canvas,&layer);}
    drawing=false;
    lv_svg_node_delete(doc);
    assert(display->layer_head==head && head->next==next);
    assert(LV_GLOBAL_DEFAULT()->draw_info.used_memory_for_layers==memory);
}
static void pixel(int x,int y,uint32_t rgb)
{
    lv_color32_t p=lv_canvas_get_px(canvas,x,y);
    int r=(rgb>>16)&255,g=(rgb>>8)&255,b=rgb&255;
    if(abs(p.red-r)>2 || abs(p.green-g)>2 || abs(p.blue-b)>2)
        fprintf(stderr,"pixel (%d,%d) got=%u,%u,%u expected=%d,%d,%d\n",x,y,p.red,p.green,p.blue,r,g,b);
    assert(abs(p.red-r)<=2 && abs(p.green-g)<=2 && abs(p.blue-b)<=2);
}

static void image_provider(const char *url,lv_draw_image_dsc_t *dsc)
{
    static uint32_t pixels[64];
    static lv_image_dsc_t source={.header={.magic=LV_IMAGE_HEADER_MAGIC,
        .cf=LV_COLOR_FORMAT_ARGB8888,.w=8,.h=8,.stride=32},
        .data=(const uint8_t *)pixels,.data_size=sizeof pixels};
    if(strcmp(url,"test-red")) return;
    for(unsigned i=0;i<64;i++) pixels[i]=0xffff0000;
    dsc->src=&source;dsc->header=source.header;
}
static void image_element_opacity(void)
{
    const lv_svg_render_hal_t hal={.load_image=image_provider};lv_svg_render_init(&hal);
    for(image_mode=0;image_mode<2;image_mode++) {
        render("<g opacity='0.5'><image width='8' height='8' opacity='0.5' xlink:href='test-red'/></g>");
        pixel(3,3,0x400000);
        render("<g opacity='0.5'><image width='8' height='8' xlink:href='test-red' opacity='0.5'/></g>");
        pixel(3,3,0x400000);
    }
    const lv_svg_render_hal_t empty={0};lv_svg_render_init(&empty);image_mode=0;
    puts("PASS SVG image element opacity is applied once in either attribute order");
}
static void opacity_boundaries(void)
{
    const char *shape="<rect x='4' y='4' width='24' height='24' fill='#ff0000' opacity='0.5'/>";
    for(image_mode=0;image_mode<2;image_mode++) for(unsigned bg=0;bg<3;bg++) {
        background_opa=bg==0?0:bg==1?64:255;
        image_opa=image_mode?128:255;
        render(shape);
        lv_color32_t p=lv_canvas_get_px(canvas,8,8);
        double sa=128.0/255.0*(image_mode?128.0/255.0:1.0);
        double alpha=sa+(background_opa/255.0)*(1-sa);
        assert(abs((int)p.alpha-(int)(alpha*255+0.5))<=2);
        assert(abs((int)p.red-(int)(255*sa/alpha+0.5))<=3);
        assert(p.green==0 && p.blue==0);
    }
    background_opa=image_opa=255;image_mode=0;
    const char *siblings="<g opacity='0.5'><rect width='16' height='16' fill='#ff0000'/></g>"
        "<g opacity='0.5'><rect x='24' width='16' height='16' fill='#0000ff'/></g>"
        "<rect x='48' width='12' height='12' fill='#00ff00'/>";
    render(siblings);size_t before=bytes_live;
    for(unsigned cycle=0;cycle<10;cycle++) for(fail_kind=1;fail_kind<=3;fail_kind++) {
        /* The first vector descriptor is the root; fail its opacity child. */
        fail_at=fail_kind==3?2:1;
        render(siblings);assert(fault_calls>=fail_at);
        pixel(8,8,0);pixel(28,8,0x000080);pixel(52,8,0x00ff00);
        assert(bytes_live==before);
    }
    fail_kind=0;
    /* 256 full-canvas 16 KiB sibling layers fit exactly in the 4 MiB
     * default. The next opacity object must be skipped, even if earlier
     * siblings were already rendered/freed by the native dispatcher. */
    char body[65536];unsigned used=0;
    for(unsigned i=0;i<256;i++) used+=snprintf(body+used,sizeof body-used,
        "<rect width='8' height='8' fill='#ff0000' opacity='0.5'/>");
    used+=snprintf(body+used,sizeof body-used,
        "<rect x='24' width='16' height='16' fill='#0000ff' opacity='0.5'/>"
        "<rect x='48' width='12' height='12' fill='#00ff00'/>");
    assert(used<sizeof body);render(body);assert(layer_creates==256);
    pixel(4,4,0xff0000);pixel(28,8,0);pixel(52,8,0x00ff00);
    render(shape);pixel(8,8,0x800000);assert(layer_creates==1);
    /* Bound group recursion too; a skipped branch must not hide siblings. */
    used=0;for(unsigned i=0;i<50;i++) used+=snprintf(body+used,sizeof body-used,"<g>");
    used+=snprintf(body+used,sizeof body-used,"<rect width='16' height='16' fill='#ff0000'/>");
    for(unsigned i=0;i<50;i++) used+=snprintf(body+used,sizeof body-used,"</g>");
    used+=snprintf(body+used,sizeof body-used,"<rect x='48' width='12' height='12' fill='#00ff00'/>");
    render(body);pixel(8,8,0);pixel(52,8,0x00ff00);
    render(siblings);assert(bytes_live==before);
    puts("PASS SVG alpha targets, image opacity, 30 allocation failures, cumulative budget and group depth");
}
#if LV_USE_FREETYPE
static void span_opacity(void)
{
    const lv_svg_render_hal_t hal={.get_font_path=font_path};lv_svg_render_init(&hal);
    uint32_t reference[4096];char body[1024];
    for(image_mode=0;image_mode<2;image_mode++) for(unsigned opa=0;opa<3;opa++) {
        snprintf(body,sizeof body,"<text x='4' y='36' font-family='test' font-size='24' fill='#00ff00'>"
            "<tspan font-family='test' font-size='24' fill='#ff0000' opacity='%s'>A</tspan>B</text>",
            opa==0?"1":opa==1?"0.5":"0");
        render(body);unsigned red=0,green=0;
        for(unsigned y=0;y<64;y++) for(unsigned x=0;x<64;x++) {
            lv_color32_t p=lv_canvas_get_px(canvas,x,y);
            if(!opa) reference[y*64+x]=(p.red<<16)|(p.green<<8)|p.blue;
            else {
                uint32_t r=reference[y*64+x];
                assert(abs((int)p.red-(int)(((r>>16)&255)*(opa==1?128:0)/255))<=2);
                assert(abs((int)p.green-(int)((r>>8)&255))<=2);
            }
            if(p.red>20) red++;
            if(p.green>20) green++;
        }
        assert(green>20);if(opa<2) assert(red>20);else assert(!red);
    }
    image_mode=0;puts("PASS SVG span opacity preserves following text positions in direct/image modes");
}
#endif
int main(int argc,char **argv)
{
    lv_init();display=lv_display_create(64,64);assert(display);
    static uint32_t framebuffer[4096];
    lv_display_set_color_format(display,LV_COLOR_FORMAT_ARGB8888);
    lv_display_set_buffers(display,framebuffer,NULL,sizeof framebuffer,LV_DISPLAY_RENDER_MODE_DIRECT);
    lv_display_set_flush_cb(display,flush);
    canvas=lv_canvas_create(lv_screen_active());
    lv_draw_buf_t *buffer=lv_draw_buf_create(64,64,LV_COLOR_FORMAT_ARGB8888,0);assert(buffer);
    lv_canvas_set_draw_buf(canvas,buffer);
    const char *mode=argc>1?argv[1]:"all";
    unsigned scenes=0;
    for(image_mode=0;image_mode<2;image_mode++) for(unsigned cycle=0;cycle<10;cycle++)
    for(unsigned i=0;i<sizeof svg_document_cases/sizeof svg_document_cases[0];i++) {
        if(strcmp(mode,"all") && strcmp(mode,svg_document_cases[i].name)) continue;
        render(svg_document_cases[i].body);
        for(unsigned p=0;p<3;p++) pixel(svg_document_cases[i].sample[p].x,svg_document_cases[i].sample[p].y,
                                       svg_document_cases[i].sample[p].rgb);
        scenes++;
    }
    image_mode=0;
    if(!strcmp(mode,"all") || !strcmp(mode,"opacity")) {opacity_boundaries();image_element_opacity();}
#if LV_USE_FREETYPE
    if(!strcmp(mode,"all") || !strcmp(mode,"span-opacity")) span_opacity();
#endif
    if(!strcmp(mode,"all") || !strcmp(mode,"depth")) {
        const unsigned lengths[]={30,31,32,40};
        for(unsigned n=0;n<4;n++) {
            char body[4096];unsigned used=snprintf(body,sizeof body,"<defs><rect id='u0' width='8' height='8' fill='#ff0000'/>");
            for(unsigned i=1;i<=lengths[n];i++) used+=snprintf(body+used,sizeof body-used,"<use id='u%u' xlink:href='#u%u'/>",i,i-1);
            used+=snprintf(body+used,sizeof body-used,"</defs><use xlink:href='#u%u'/>",lengths[n]);
            assert(used<sizeof body);render(body);pixel(3,3,lengths[n]<32?0xff0000:0);
        }
        /* A declined chain must not poison the next independent render. */
        render("<rect x='8' y='8' width='16' height='16' fill='#ff0000'/>");pixel(12,12,0xff0000);
    }
    if(!strcmp(mode,"all") || !strcmp(mode,"heap")) {
        const char *body="<g fill='none' stroke='#ff0000' stroke-width='2' stroke-dasharray='4 4'><g><path d='M 4 8 L 60 8'/></g></g>";
        render(body);size_t before=bytes_live;
        for(unsigned i=0;i<100;i++) render(body);
        fprintf(stderr,"SVG grouped dash live-byte growth: %lld\n",(long long)bytes_live-(long long)before);
        assert(bytes_live==before);pixel(5,8,0xff0000);pixel(10,8,0);
    }
#if LV_USE_FREETYPE
    if(!strcmp(mode,"all") || !strcmp(mode,"text-heap")) {
        const lv_svg_render_hal_t hal={.get_font_path=font_path};lv_svg_render_init(&hal);
        const char *body="<text x='4' y='36' font-family='test' font-size='24' fill='none' stroke='#ff0000' stroke-width='1' stroke-dasharray='2 2'><tspan font-family='test' font-size='24'>A</tspan></text>";
        render(body);size_t before=bytes_live;
        for(unsigned i=0;i<20;i++) render(body);
        assert(bytes_live==before);
        unsigned nonblack=0;
        for(unsigned y=0;y<64;y++) for(unsigned x=0;x<64;x++) {
            lv_color32_t p=lv_canvas_get_px(canvas,x,y);if(p.red>20) nonblack++;
        }
        assert(nonblack>20);fprintf(stderr,"PASS SVG outline span render and 20 balanced dash lifetimes\n");
    }
#endif
    lv_obj_delete(canvas);lv_draw_buf_destroy(buffer);lv_display_delete(display);lv_deinit();
    assert(bytes_live==0);
    printf("PASS %u direct/decoded image SVG scenes and reference-depth boundaries\n",scenes);
    puts("PASS SVG document transform/style/reference and grouped stroke lifetime");return 0;
}
