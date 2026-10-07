/* SPDX-License-Identifier: Apache-2.0 */
#define ulong uintptr_t
#include "mpp_engine_fixture.h"
#include "../../draw/ge2d/lv_draw_aic_ge2d.c"
#include "../../draw/ge2d/lv_draw_aic_ge2d_image.c"
#include "../../draw/ge2d/lv_draw_aic_ge2d_fill.c"
#include "lv_aic_video_window.h"
#include "../common/lv_aic_video_window_cases.h"
/* The manual runner uses only LVGL and stub logging, not RT-Thread APIs. */
#undef AIC_LVGL_BSP_RTTHREAD
#define AIC_LVGL_BSP_RTTHREAD 1
#include "../manual/lv_aic_video_window_test.c"
#undef AIC_LVGL_BSP_RTTHREAD
#define AIC_LVGL_BSP_RTTHREAD 0

static _Alignas(64) struct {uint32_t before[16],pixels[64*64],after[16];} storage;
#define framebuffer storage.pixels
static unsigned engine_mode,fills,frames;
static const lv_draw_buf_t *cached_dst;
struct mpp_ge *mpp_ge_open(void) {return engine_mode?(struct mpp_ge *)(uintptr_t)1:NULL;}
void mpp_ge_close(struct mpp_ge *ge) {(void)ge;}
int mpp_ge_bitblt(struct mpp_ge *ge,struct ge_bitblt *b) {(void)ge;(void)b;assert(false);return -1;}
int mpp_ge_rotate(struct mpp_ge *ge,struct ge_rotation *r) {(void)ge;(void)r;assert(false);return -1;}
int mpp_ge_fillrect(struct mpp_ge *ge,struct ge_fillrect *f)
{
    (void)ge;assert(engine_mode==2 && !f->ctrl.alpha_en && f->dst_buf.format==MPP_FMT_ARGB_8888);
    assert(cached_dst && f->dst_buf.phy_addr[0]==(uint32_t)(uintptr_t)cached_dst->data && f->dst_buf.stride[0]==256);
    int x=f->dst_buf.crop.x,y=f->dst_buf.crop.y,w=f->dst_buf.crop.width,h=f->dst_buf.crop.height;
    assert(x>=0 && y>=0 && w>0 && h>0 && x+w<=64 && y+h<=64);
    for(int row=y;row<y+h;row++) for(int col=x;col<x+w;col++) ((uint32_t *)cached_dst->data)[row*64+col]=f->start_color;
    fills++;return 0;
}
int mpp_ge_emit(struct mpp_ge *ge) {(void)ge;return 0;}
int mpp_ge_sync(struct mpp_ge *ge) {(void)ge;return 0;}
bool lv_draw_aic_ge2d_buf_address_valid(const lv_draw_buf_t *b) {return engine_mode==2 && b && b->header.w==64 && b->header.h==64 && b->header.stride==256;}
bool lv_draw_aic_ge2d_dst_format_supported(lv_color_format_t cf) {return lv_aic_pixel_format_is_ge2d_dst(cf);}
void lv_draw_aic_ge2d_prepare_src_cache(const lv_draw_buf_t *b,const lv_area_t *a) {(void)b;(void)a;}
void lv_draw_aic_ge2d_prepare_dst_cache(const lv_draw_buf_t *b,const lv_area_t *a) {cached_dst=b;(void)a;}
int lv_draw_aic_ge2d_yuv(lv_draw_task_t *t) {(void)t;return 0;}
bool lv_draw_aic_ge2d_yuv_faulted(void) {return false;}
static void flush(lv_display_t *d,const lv_area_t *a,uint8_t *p) {(void)a;(void)p;lv_display_flush_ready(d);}
static void pixel(unsigned x,unsigned y,uint32_t expected)
{
    uint32_t actual=framebuffer[y*64+x];
    if(actual!=expected) fprintf(stderr,"pixel %u,%u got=%08x expected=%08x\n",x,y,(unsigned)actual,(unsigned)expected);
    assert(actual==expected);
}
static void refresh(lv_display_t *display)
{
    lv_refr_now(display);frames++;
    for(unsigned i=0;i<16;i++) assert(storage.before[i]==0xdeadbeef && storage.after[i]==0xdeadbeef);
    assert(display->layer_head && !display->layer_head->next);
    assert(!LV_GLOBAL_DEFAULT()->draw_info.used_memory_for_layers);
}
static void scene(lv_display_t *display,const lv_area_t *hole,uint32_t inside,uint32_t outside)
{
    refresh(display);
    for(int y=0;y<64;y++) for(int x=0;x<64;x++)
        pixel(x,y,hole && x>=hole->x1 && x<=hole->x2 && y>=hole->y1 && y<=hole->y2?inside:outside);
}
static void coverage(lv_obj_t *obj,bool opaque)
{
    lv_obj_update_layout(obj);lv_area_t area;lv_obj_get_coords(obj,&area);
    area.x1++;area.y1++;area.x2--;area.y2--;
    lv_cover_check_info_t info={.area=&area,.res=LV_COVER_RES_COVER};
    lv_obj_send_event(obj,LV_EVENT_COVER_CHECK,&info);
    assert(info.res==(opaque?LV_COVER_RES_COVER:LV_COVER_RES_NOT_COVER));
}
static void run(void)
{
    lv_init();
    for(unsigned i=0;i<16;i++) storage.before[i]=storage.after[i]=0xdeadbeef;
    memset(framebuffer,0,sizeof framebuffer);fills=0;
    lv_image_decoder_t *decoder=NULL;assert(lv_aic_mpp_decoder_init(&decoder)==LV_AIC_OK);
    lv_draw_aic_ge2d_init();
    lv_display_t *display=lv_display_create(64,64);assert(display);
    lv_display_set_color_format(display,LV_COLOR_FORMAT_ARGB8888);
    lv_display_set_buffers(display,framebuffer,NULL,sizeof framebuffer,LV_DISPLAY_RENDER_MODE_DIRECT);
    lv_display_set_flush_cb(display,flush);
    lv_obj_t *screen=lv_screen_active();lv_obj_remove_style_all(screen);
    lv_obj_set_style_bg_color(screen,lv_color_hex(0x204060),0);lv_obj_set_style_bg_opa(screen,255,0);
    lv_obj_t *image=lv_image_create(screen);lv_obj_set_pos(image,0,0);
    lv_image_set_src(image,"L:/64x64_1_80ff0000.fake");
    refresh(display);pixel(8,8,0xff8f1f2f);
    /* Repeated redraws must blend against a freshly repainted background. */
    for(unsigned i=0;i<3;i++) {lv_obj_invalidate(image);refresh(display);pixel(8,8,0xff8f1f2f);}
    lv_obj_set_style_bg_opa(image,255,0);
    coverage(image,false);
    lv_image_set_src(image,"L:/64x64_0_00123456.fake");coverage(image,false);
    scene(display,&(lv_area_t){0,0,63,63},0x00123456,0);
    lv_image_set_src(image,"L:/64x64_0_ff123456.fake");coverage(image,true);
    scene(display,&(lv_area_t){0,0,63,63},0xff123456,0);
    lv_obj_delete(image);scene(display,NULL,0,0xff204060);
    for(unsigned cycle=0;cycle<5;cycle++) {
        lv_obj_t *window=lv_aic_video_window_create(screen);assert(window);
        lv_aic_video_window_set_size(window,20,12);lv_aic_video_window_set_color(window,lv_color_hex(0x123456));
        lv_obj_set_pos(window,12,16);
        scene(display,&(lv_area_t){12,16,31,27},0x00123456,0xff204060);
        lv_obj_set_pos(window,20,24);lv_aic_video_window_set_size(window,16,8);
        scene(display,&(lv_area_t){20,24,35,31},0x00123456,0xff204060);
        lv_obj_set_hidden(window,true);scene(display,NULL,0,0xff204060);
        lv_obj_set_hidden(window,false);scene(display,&(lv_area_t){20,24,35,31},0x00123456,0xff204060);
        lv_obj_set_pos(window,-4,-2);scene(display,&(lv_area_t){0,0,11,5},0x00123456,0xff204060);
        lv_obj_set_pos(window,32,32);lv_image_set_pivot(window,0,0);
        /* Analytic right-angle bounds use pixel centers 0..15 and 0..7. */
        for(unsigned angle=0;angle<4;angle++) {
            lv_image_set_rotation(window,angle*900);scene(display,&video_window_rotated_bounds[angle],0x00123456,0xff204060);
        }
        lv_image_set_rotation(window,0);lv_obj_delete(window);scene(display,NULL,0,0xff204060);
    }
    /* The same inclusive-extent correction applies to ordinary native images,
     * independently of the AIC pseudo-fill executor and metadata decoder. */
    uint32_t native_pixels[16*8];for(unsigned i=0;i<16*8;i++) native_pixels[i]=0xff123456;
    for(unsigned portrait=0;portrait<2;portrait++) {
        unsigned w=portrait?8:16,h=portrait?16:8;
        lv_image_dsc_t src={.header={.magic=LV_IMAGE_HEADER_MAGIC,.cf=LV_COLOR_FORMAT_ARGB8888,
            .w=w,.h=h,.stride=w*4},.data=(const uint8_t *)native_pixels,.data_size=sizeof native_pixels};
        image=lv_image_create(screen);lv_image_set_src(image,&src);lv_image_set_pivot(image,0,0);
        lv_image_set_antialias(image,false);lv_obj_set_pos(image,32,32);
        for(unsigned angle=0;angle<4;angle++) {
            lv_image_set_rotation(image,angle*900);
            scene(display,portrait?&video_window_portrait_bounds[angle]:&video_window_rotated_bounds[angle],0xff123456,0xff204060);
        }
        lv_obj_delete(image);scene(display,NULL,0,0xff204060);
        lv_image_cache_drop(&src);lv_image_header_cache_drop(&src);
    }
    const lv_draw_aic_ge2d_stats_t *stats=lv_draw_aic_ge2d_stats();
    assert(stats->image_completed && !stats->errors);
    if(engine_mode==2) assert(fills && stats->image_completed>stats->image_sw_fallback);
    else assert(!fills && stats->image_completed==stats->image_sw_fallback);
    if(engine_mode==2) for(unsigned cycle=0;cycle<10;cycle++) {
        lv_display_t *original=lv_display_get_default();lv_layer_t *head=original->layer_head;
        assert(lv_aic_video_window_test_run()==0);
        assert(lv_display_get_default()==original && original->layer_head==head && !head->next);
        assert(!LV_GLOBAL_DEFAULT()->draw_info.used_memory_for_layers);
    }
    lv_display_delete(display);lv_aic_mpp_decoder_deinit(decoder);lv_deinit();
    assert(!live_cma && !decodes && lv_aic_fake_fs_idle());
}
int main(void)
{
    for(engine_mode=0;engine_mode<3;engine_mode++) run();
    printf("PASS %u real widget/pseudo-image refreshes: GE unavailable, heap fallback and descriptor fill model\n",frames);
    return 0;
}
