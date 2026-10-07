/* SPDX-License-Identifier: Apache-2.0 */
#include "lv_aic_apng_test.h"
#if defined(AIC_LVGL_USE_APNG_WIDGET) && AIC_LVGL_USE_APNG_WIDGET
#include "lv_aic_apng_widget.h"
static lv_obj_t *panel,*animation,*mirror,*status_label;
static unsigned source_index;
static const char *const paths[]={"/data/mpp_test/apng-disposal.png","/data/mpp_test/apng-loop.png",
                                 "/data/mpp_test/apng-static.png"};
static const char *const names[]={"disposal / 2 plays","infinite loop","static PNG"};
enum command { NONE,SHOW,PAUSE,RESUME,SLOW,NORMAL,FAST,REPLAY,NEXT,STATUS,CLOSE };
void lv_aic_apng_test_delete(void)
{
    if(panel) lv_obj_delete(panel);
    panel=animation=mirror=status_label=NULL;
}
static void refresh(void)
{
    if(!animation || !status_label) return;
    lv_aic_apng_playback_status_t s=lv_aic_apng_get_status(animation);
    static const char *const states[]={"opening","ready","playing","paused","ended","closing","closed","fault"};
    lv_label_set_text_fmt(status_label,"%s | %s | rate %u/%u\nframe %u | composed %u | published %u",
        names[source_index],states[s.state],(unsigned)s.rate_num,(unsigned)s.rate_den,
        (unsigned)s.frame_index,(unsigned)s.composed,(unsigned)s.published);
    lv_obj_align(animation,LV_ALIGN_CENTER,-96,0);
    if(mirror) lv_obj_align(mirror,LV_ALIGN_CENTER,96,0);
}
static void changed(lv_event_t *event) { (void)event;refresh(); }
static lv_result_t act(enum command cmd)
{
    if(cmd==CLOSE) { lv_aic_apng_test_delete();return LV_RESULT_OK; }
    if(!animation) return LV_RESULT_INVALID;
    switch(cmd) {
    case PAUSE:return lv_aic_apng_pause(animation,true);
    case RESUME:return lv_aic_apng_pause(animation,false);
    case SLOW:return lv_aic_apng_set_rate(animation,1,2);
    case NORMAL:return lv_aic_apng_set_rate(animation,1,1);
    case FAST:return lv_aic_apng_set_rate(animation,2,1);
    case REPLAY:return lv_aic_apng_restart(animation);
    case NEXT:
        source_index=(source_index+1)%3;
        if(lv_aic_apng_set_src(animation,paths[source_index])!=LV_RESULT_OK) return LV_RESULT_INVALID;
        return lv_aic_apng_start(animation);
    case STATUS:refresh();return LV_RESULT_OK;
    default:return LV_RESULT_INVALID;
    }
}
static void button_event(lv_event_t *event)
{ (void)act((enum command)(uintptr_t)lv_event_get_user_data(event)); }
int lv_aic_apng_test_show(void)
{
    if(panel || !lv_display_get_default() || lv_aic_apng_pending_cleanup()) return LV_AIC_ERR_INVALID_STATE;
    if(!lv_aic_rgb_image_decoder_is_initialized() && !lv_aic_rgb_image_decoder_init()) return LV_AIC_ERR_INVALID_STATE;
    panel=lv_obj_create(lv_layer_top());if(!panel) return LV_AIC_ERR_NO_MEMORY;
    lv_obj_remove_style_all(panel);lv_obj_set_size(panel,lv_pct(100),lv_pct(100));
    lv_obj_set_style_bg_color(panel,lv_color_hex(0x182430),0);lv_obj_set_style_bg_opa(panel,LV_OPA_COVER,0);
    lv_obj_set_scrollable(panel,false);lv_obj_set_clickable(panel,true);
    status_label=lv_label_create(panel);if(!status_label) goto fail;
    lv_obj_set_pos(status_label,12,8);lv_obj_set_style_text_color(status_label,lv_color_white(),0);
    lv_obj_set_width(status_label,lv_display_get_horizontal_resolution(NULL)-24);
    lv_obj_t *viewport=lv_obj_create(panel);if(!viewport) goto fail;
    lv_obj_set_pos(viewport,12,64);
    lv_obj_set_size(viewport,lv_display_get_horizontal_resolution(NULL)-24,
                    lv_display_get_vertical_resolution(NULL)-180);
    lv_obj_set_scrollable(viewport,false);lv_obj_set_style_bg_color(viewport,lv_color_hex(0x8090a0),0);
    animation=lv_aic_apng_create(viewport);if(!animation) goto fail;
    lv_aic_apng_playback_options_t options={.limits={1024*1024,256*1024,128*96,16},
        .stream_budget=1024*1024,.snapshot_budget=512*1024,.cma_budget=256*1024,
        .packet_limit=256*1024,.snapshots=3,.minimum_delay_us=1000};
    if(lv_aic_apng_configure(animation,&options)!=LV_RESULT_OK) goto fail;
    mirror=lv_aic_apng_slave_create(viewport);if(!mirror) goto fail;
    if(lv_aic_apng_slave_set_master(mirror,animation)!=LV_RESULT_OK) goto fail;
    lv_obj_add_event_cb(animation,changed,LV_EVENT_VALUE_CHANGED,NULL);
    static const char *const labels[]={"Pause","Resume","0.5x","1x","2x","Replay","Next source","Close"};
    static const enum command commands[]={PAUSE,RESUME,SLOW,NORMAL,FAST,REPLAY,NEXT,CLOSE};
    int width=(lv_display_get_horizontal_resolution(NULL)-40)/4;
    int top=lv_display_get_vertical_resolution(NULL)-104;
    for(unsigned i=0;i<8;i++) {
        lv_obj_t *button=lv_button_create(panel);if(!button) goto fail;
        lv_obj_set_size(button,width,42);lv_obj_set_pos(button,8+(i%4)*(width+8),top+(i/4)*50);
        lv_obj_add_event_cb(button,button_event,LV_EVENT_CLICKED,(void *)(uintptr_t)commands[i]);
        lv_obj_t *label=lv_label_create(button);if(!label) goto fail;
        lv_label_set_text(label,labels[i]);lv_obj_center(label);
    }
    source_index=0;
    if(lv_aic_apng_set_src(animation,paths[0])!=LV_RESULT_OK || lv_aic_apng_start(animation)!=LV_RESULT_OK) goto fail;
    refresh();return LV_AIC_OK;
fail:
    lv_aic_apng_test_delete();return LV_AIC_ERR_NO_MEMORY;
}
#if AIC_LVGL_BSP_RTTHREAD
#include <rtthread.h>
#include <rthw.h>
#include <finsh.h>
#include <string.h>
static volatile enum command pending;
static volatile bool ready;
void lv_aic_apng_test_poll(void)
{
    rt_base_t level=rt_hw_interrupt_disable();ready=true;enum command cmd=pending;pending=NONE;rt_hw_interrupt_enable(level);
    if(cmd!=NONE) {
        int result=cmd==SHOW?lv_aic_apng_test_show():(int)act(cmd);
        rt_kprintf("APNG command=%d result=%d cleanup=%u\n",cmd,result,lv_aic_apng_pending_cleanup());
        if(cmd==STATUS && animation) {
            lv_aic_apng_playback_status_t s=lv_aic_apng_get_status(animation);
            rt_kprintf("APNG state=%d frame=%u composed=%u published=%u plays=%u rate=%u/%u\n",
                s.state,(unsigned)s.frame_index,(unsigned)s.composed,(unsigned)s.published,
                (unsigned)s.completed_plays,(unsigned)s.rate_num,(unsigned)s.rate_den);
        }
    }
    refresh();
}
void lv_aic_apng_test_deinit(void)
{
    rt_base_t level=rt_hw_interrupt_disable();ready=false;pending=NONE;rt_hw_interrupt_enable(level);
    lv_aic_apng_test_delete();
}
static void lv_aic_apng_test(int argc,char **argv)
{
    static const char *const words[]={"show","pause","resume","slow","normal","fast","replay","next","status","close"};
    enum command cmd=NONE;
    if(argc==2) for(unsigned i=0;i<10;i++) if(!strcmp(argv[1],words[i])) cmd=(enum command)(i+1);
    if(cmd==NONE) { rt_kprintf("lv_aic_apng_test show|pause|resume|slow|normal|fast|replay|next|status|close\n");return; }
    rt_base_t level=rt_hw_interrupt_disable();bool ok=ready && pending==NONE;if(ok) pending=cmd;rt_hw_interrupt_enable(level);
    rt_kprintf(ok?"APNG request queued\n":"APNG busy or UI not ready; retry\n");
}
MSH_CMD_EXPORT(lv_aic_apng_test, APNG acceptance controls on UI thread);
#else
void lv_aic_apng_test_poll(void) { refresh(); }
void lv_aic_apng_test_deinit(void) { lv_aic_apng_test_delete(); }
#endif
#endif
