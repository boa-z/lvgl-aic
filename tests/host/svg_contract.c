/* SPDX-License-Identifier: Apache-2.0 */
#include "lvgl.h"
#include "lvgl_private.h"
#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
static const char svg[]="<svg width='32' height='32' viewBox='0 0 32 32' xmlns='http://www.w3.org/2000/svg'>"
    "<rect width='32' height='32' fill='#ff0000'/>"
    "<circle cx='16' cy='16' r='8' fill='#0000ff'/></svg>";
static uint32_t pixels[64*64],reference[64*64];
static unsigned opens,closes,flushes;
static void *open_file(lv_fs_drv_t *drv,const char *path,lv_fs_mode_t mode)
{
    (void)drv;if(strcmp(path,"/shapes.svg") || mode!=LV_FS_MODE_RD) return NULL;
    uint32_t *position=malloc(sizeof *position);assert(position);*position=0;opens++;return position;
}
static lv_fs_res_t close_file(lv_fs_drv_t *drv,void *file)
{ (void)drv;free(file);closes++;return LV_FS_RES_OK; }
static lv_fs_res_t read_file(lv_fs_drv_t *drv,void *file,void *buf,uint32_t bytes,uint32_t *read)
{
    (void)drv;uint32_t *pos=file;
    *read=sizeof(svg)-1-*pos;if(*read>bytes) *read=bytes;
    memcpy(buf,svg+*pos,*read);*pos+=*read;return LV_FS_RES_OK;
}
static lv_fs_res_t seek_file(lv_fs_drv_t *drv,void *file,uint32_t pos,lv_fs_whence_t whence)
{
    (void)drv;uint32_t *offset=file;
    uint64_t base=whence==LV_FS_SEEK_SET?0:whence==LV_FS_SEEK_CUR?*offset:sizeof(svg)-1;
    if(base+pos>sizeof(svg)-1) return LV_FS_RES_INV_PARAM;
    *offset=(uint32_t)(base+pos);return LV_FS_RES_OK;
}
static lv_fs_res_t tell_file(lv_fs_drv_t *drv,void *file,uint32_t *pos)
{ (void)drv;*pos=*(uint32_t *)file;return LV_FS_RES_OK; }
static void flush(lv_display_t *d,const lv_area_t *a,uint8_t *data)
{ (void)a;(void)data;flushes++;lv_display_flush_ready(d); }
static void check_pixels(void)
{
    uint32_t red=pixels[12*64+12],blue=pixels[24*64+24],black=pixels[52*64+52];
    /* Native opacity products leave at most one channel level at full cover. */
    assert(((red>>16)&255)>=254 && (red&0xffff)==0);
    assert((blue&255)>=254 && ((blue>>16)&255)<=1 && (blue&0xff00)==0);
    assert((black&0xffffff)==0);
}
int main(void)
{
    lv_init();lv_fs_drv_t fs;lv_fs_drv_init(&fs);fs.letter='S';
    fs.open_cb=open_file;fs.close_cb=close_file;fs.read_cb=read_file;
    fs.seek_cb=seek_file;fs.tell_cb=tell_file;lv_fs_drv_register(&fs);
    lv_display_t *d=lv_display_create(64,64);assert(d);
    lv_display_set_color_format(d,LV_COLOR_FORMAT_ARGB8888);
    lv_display_set_buffers(d,pixels,NULL,sizeof pixels,LV_DISPLAY_RENDER_MODE_DIRECT);
    lv_display_set_flush_cb(d,flush);
    lv_obj_set_style_bg_color(lv_screen_active(),lv_color_black(),0);
    lv_obj_set_style_bg_opa(lv_screen_active(),LV_OPA_COVER,0);
    lv_image_dsc_t image={.header={.magic=LV_IMAGE_HEADER_MAGIC,.cf=LV_COLOR_FORMAT_RAW,.w=32,.h=32},
        .data_size=sizeof(svg)-1,.data=(const uint8_t *)svg};
    for(unsigned n=0;n<20;n++) {
        const void *src=n%2?(const void *)"S:/shapes.svg":(const void *)&image;
        lv_image_header_t header;
        assert(lv_image_decoder_get_info(src,&header)==LV_RESULT_OK);
        assert(header.w==32 && header.h==32 && (header.flags&LV_IMAGE_FLAGS_CUSTOM_DRAW));
        lv_obj_t *obj=lv_image_create(lv_screen_active());lv_image_set_src(obj,src);lv_obj_set_pos(obj,8,8);
        lv_refr_now(d);check_pixels();
        if(!n) memcpy(reference,pixels,sizeof pixels);
        else assert(!memcmp(reference,pixels,sizeof pixels));
        lv_obj_delete(obj);lv_image_cache_drop(src);lv_refr_now(d);
        assert((pixels[24*64+24]&0xffffff)==0);
    }
    lv_image_dsc_t invalid=image;invalid.data=(const uint8_t *)"xxxxx";invalid.data_size=5;
    /* The generic BIN decoder also accepts VARIABLE buffers; verify that the
     * SVG parser declines this signature, without claiming global rejection. */
    lv_image_decoder_dsc_t decoded;
    lv_image_decoder_args_t args={.no_cache=true};
    assert(lv_image_decoder_open(&decoded,&invalid,&args)==LV_RESULT_OK);
    assert(decoded.decoder && strcmp(decoded.decoder->name,"SVG") &&
           !(decoded.header.flags&LV_IMAGE_FLAGS_CUSTOM_DRAW));
    lv_image_decoder_close(&decoded);
    lv_display_delete(d);lv_deinit();
    assert(opens>0 && opens==closes && flushes>=40);
    puts("PASS native SVG FILE/VARIABLE pixel parity, dimensions, invalid signature and lifecycle");
    return 0;
}
