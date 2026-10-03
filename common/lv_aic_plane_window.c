/* SPDX-License-Identifier: Apache-2.0 */
#define AIC_LVGL_USE_PRIVATE_API 1
#include "lv_aic_plane_window.h"
#include "lvgl_aic_private.h"
#if defined(AIC_LVGL_USE_VIDEO_PLANE) && AIC_LVGL_USE_VIDEO_PLANE
#include <stdio.h>
#include <string.h>
/* Strict rectangle profile: reject transforms that a single DE rectangle
 * cannot reproduce. Hidden objects keep their last frame but stop scanout. */
bool lv_aic_plane_window_present(lv_aic_plane_window_t *b,lv_obj_t *obj,const void *source)
{
    if(!b || !obj || !source) return false;
    lv_display_t *display=lv_obj_get_display(obj);
    lv_obj_t *root=lv_obj_get_screen(obj);
    bool hidden=root!=lv_display_get_screen_active(display) &&
        root!=lv_display_get_layer_top(display) && root!=lv_display_get_layer_sys(display) &&
        root!=lv_display_get_layer_bottom(display);
    for(lv_obj_t *a=obj;a;a=lv_obj_get_parent(a))
        hidden=hidden || lv_obj_is_hidden(a);
    if(hidden) {
        if(b->plane && !lv_aic_video_plane_hide(b->plane)) return false;
        b->source=NULL;return true;
    }
    /* LVGL 9.6 offset getters mirror around the physical extent at 180/270.
     * Normalize their origin before rejecting sub-display offsets. */
    lv_display_rotation_t rotation=lv_display_get_rotation(display);
    int32_t ox=lv_display_get_offset_x(display),oy=lv_display_get_offset_y(display);
    if(rotation==LV_DISPLAY_ROTATION_180 || rotation==LV_DISPLAY_ROTATION_270) {
        ox=lv_display_get_physical_horizontal_resolution(display)-ox;
        oy=lv_display_get_physical_vertical_resolution(display)-oy;
    }
    int32_t image_angle=lv_image_get_rotation(obj);
    if(image_angle%900U) return false;
    unsigned degrees=(image_angle/10U+360U-(unsigned)rotation*90U)%360U;
    if(degrees && !b->budget) return false;
    if(display!=lv_display_get_default() || ox || oy ||
       lv_display_get_color_format(display)!=LV_COLOR_FORMAT_ARGB8888 ||
       lv_image_get_scale_x(obj)!=LV_SCALE_NONE ||
       lv_image_get_scale_y(obj)!=LV_SCALE_NONE ||
       lv_obj_get_style_image_opa(obj,LV_PART_MAIN)!=LV_OPA_COVER ||
       lv_obj_get_style_image_recolor_opa(obj,LV_PART_MAIN)>LV_OPA_MIN ||
       lv_image_get_blend_mode(obj)!=LV_BLEND_MODE_NORMAL || lv_image_get_bitmap_map_src(obj)) return false;
    lv_obj_update_layout(obj);lv_area_t area;lv_obj_get_coords(obj,&area);
    int32_t width=lv_area_get_width(&area),height=lv_area_get_height(&area);
    if(width<1 || height<1 || width>4096 || height>4096 || (uint64_t)width*height>8U*1024U*1024U) return false;
    char window[64];snprintf(window,sizeof(window),"L:/%ux%u_0_00000000.fake",(unsigned)width,(unsigned)height);
    const void *current=lv_image_get_src(obj);
    if(!current || lv_image_src_get_type(current)!=LV_IMAGE_SRC_FILE || strcmp(current,window)) lv_image_set_src(obj,window);
    current=lv_image_get_src(obj);
    if(!current || lv_image_src_get_type(current)!=LV_IMAGE_SRC_FILE || strcmp(current,window)) return false;
    if(lv_image_get_rotation(obj)!=image_angle || lv_image_get_scale_x(obj)!=LV_SCALE_NONE ||
       lv_image_get_scale_y(obj)!=LV_SCALE_NONE) return false;
    lv_point_t pivot;lv_image_get_pivot(obj,&pivot);
    if(pivot.x < -4096 || pivot.x > 4096 || pivot.y < -4096 || pivot.y > 4096) return false;
    lv_area_t transformed;
    lv_image_buf_get_transformed_area(&transformed,width,height,image_angle,
        LV_SCALE_NONE,LV_SCALE_NONE,&pivot);
    int32_t image_x=lv_image_get_offset_x(obj),image_y=lv_image_get_offset_y(obj);
    /* Match native image placement for a same-sized fake source. Tile/auto
     * alignment has different clipping/transform semantics; do not approximate. */
    if((image_x || image_y) && lv_image_get_inner_align(obj)>=_LV_IMAGE_ALIGN_AUTO_TRANSFORM) return false;
    if(image_x < -4096 || image_x > 4096 || image_y < -4096 || image_y > 4096) return false;
    lv_area_move(&transformed,area.x1+image_x,area.y1+image_y);area=transformed;
    for(lv_obj_t *a=obj;a;a=lv_obj_get_parent(a)) {
        if(lv_obj_get_style_transform_width(a,LV_PART_MAIN) || lv_obj_get_style_transform_height(a,LV_PART_MAIN) ||
           lv_obj_get_style_blend_mode(a,LV_PART_MAIN)!=LV_BLEND_MODE_NORMAL ||
           lv_obj_get_style_transform_rotation(a,LV_PART_MAIN) ||
           lv_obj_get_style_transform_scale_x(a,LV_PART_MAIN)!=LV_SCALE_NONE ||
           lv_obj_get_style_transform_scale_y(a,LV_PART_MAIN)!=LV_SCALE_NONE ||
           lv_obj_get_style_transform_skew_x(a,LV_PART_MAIN) || lv_obj_get_style_transform_skew_y(a,LV_PART_MAIN) ||
           lv_obj_get_style_opa(a,LV_PART_MAIN)!=LV_OPA_COVER ||
           lv_obj_get_style_opa_layered(a,LV_PART_MAIN)!=LV_OPA_COVER || lv_obj_get_style_radius(a,LV_PART_MAIN)) return false;
        if(a!=obj) {
            lv_area_t clip;lv_obj_get_content_coords(a,&clip);
            if(area.x1<clip.x1 || area.y1<clip.y1 || area.x2>clip.x2 || area.y2>clip.y2) return false;
        }
    }
    if(!b->plane) b->plane=lv_aic_video_plane_open();
    if(!b->plane || !lv_aic_video_plane_enable_ui_alpha(b->plane)) return false;
    lv_display_rotate_area(display,&area);
    if(source==b->source && degrees==b->degrees && !memcmp(&area,&b->area,sizeof(area))) return true;
    if(!lv_aic_video_plane_present_rotated(b->plane,source,area.x1,area.y1,
        lv_area_get_width(&area),lv_area_get_height(&area),degrees,b->budget)) return false;
    b->source=source;b->area=area;b->degrees=degrees;return true;
}
bool lv_aic_plane_window_close(lv_aic_plane_window_t *window)
{
    if(!window) return false;
    if(window->plane && !lv_aic_video_plane_close(window->plane)) return false;
    window->plane=NULL;window->source=NULL;
    return true;
}
#endif
