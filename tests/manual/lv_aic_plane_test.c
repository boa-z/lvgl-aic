/* SPDX-License-Identifier: Apache-2.0 */
#include "lv_aic_plane_test.h"
#if LV_AIC_PLANE_TEST_ENABLED
#include "lv_aic_player.h"
#include "lv_aic_rgb_image.h"
#include <rtthread.h>
#include <rthw.h>
#include <finsh.h>
#include <string.h>
static lv_obj_t *panel,*player;
static volatile unsigned pending;
static volatile bool ready;
enum { NONE,SHOW,PAUSE,RESUME,ROTATE,PIVOT,HIDE,STATUS,CLOSE };
static void close_panel(void)
{
    if(panel) lv_obj_delete(panel);
    panel=player=NULL;
}
static lv_result_t show_panel(void)
{
    if(panel || lv_aic_player_pending_cleanup() || !lv_display_get_default() ||
       lv_display_get_color_format(NULL)!=LV_COLOR_FORMAT_ARGB8888 ||
       lv_display_get_horizontal_resolution(NULL)<240 || lv_display_get_vertical_resolution(NULL)<240)
        return LV_RESULT_INVALID;
    if(!lv_aic_rgb_image_decoder_is_initialized() && !lv_aic_rgb_image_decoder_init()) return LV_RESULT_INVALID;
    panel=lv_obj_create(lv_layer_top());if(!panel) return LV_RESULT_INVALID;
    lv_obj_remove_style_all(panel);lv_obj_set_size(panel,lv_pct(100),lv_pct(100));
    lv_obj_set_style_bg_color(panel,lv_color_hex(0x405060),0);lv_obj_set_style_bg_opa(panel,LV_OPA_COVER,0);
    lv_obj_set_scrollable(panel,false);
    lv_obj_t *label=lv_label_create(panel);if(!label) goto fail;
    lv_label_set_text(label,"Video plane: APNG fixture\nUART: pause/resume rotate pivot hide status close");
    lv_obj_set_pos(label,8,8);lv_obj_set_style_text_color(label,lv_color_white(),0);
    player=lv_aic_player_create(panel);if(!player) goto fail;
    lv_obj_remove_style_all(player);lv_obj_set_size(player,96,64);lv_obj_center(player);
    lv_aic_apng_playback_options_t options={.limits={1024*1024,256*1024,128*96,16},
        .stream_budget=1024*1024,.snapshot_budget=512*1024,.cma_budget=256*1024,
        .packet_limit=256*1024,.snapshots=3,.minimum_delay_us=1000};
    if(lv_aic_player_configure_apng(player,&options)!=LV_RESULT_OK ||
       lv_aic_player_set_video_plane(player,true)!=LV_RESULT_OK ||
       lv_aic_player_set_video_plane_rotation_budget(player,256*1024)!=LV_RESULT_OK ||
       lv_aic_player_set_src(player,"/data/mpp_test/apng-loop.png")!=LV_RESULT_OK ||
       lv_aic_player_start(player)!=LV_RESULT_OK) goto fail;
    return LV_RESULT_OK;
fail:
    close_panel();return LV_RESULT_INVALID;
}
void lv_aic_plane_test_poll(void)
{
    rt_base_t level=rt_hw_interrupt_disable();ready=true;unsigned cmd=pending;pending=NONE;rt_hw_interrupt_enable(level);
    if(!cmd) return;
    lv_result_t result=LV_RESULT_INVALID;
    if(cmd==SHOW) result=show_panel();
    else if(cmd==CLOSE) { close_panel();result=LV_RESULT_OK; }
    else if(player) {
        switch(cmd) {
        case PAUSE:result=lv_aic_player_pause(player);break;
        case RESUME:result=lv_aic_player_start(player);break;
        case ROTATE:lv_image_set_rotation(player,(lv_image_get_rotation(player)+900)%3600);result=LV_RESULT_OK;break;
        case PIVOT:lv_image_set_pivot(player,LV_PCT(25),LV_PCT(25));result=LV_RESULT_OK;break;
        case HIDE:lv_obj_set_hidden(player,!lv_obj_is_hidden(player));result=LV_RESULT_OK;break;
        case STATUS:result=LV_RESULT_OK;break;
        default:break;
        }
    }
    rt_kprintf("PLANE command=%u result=%d state=%d angle=%d cleanup=%u\n",cmd,result,
        player?(int)lv_aic_player_get_state(player):-1,player?(int)lv_image_get_rotation(player):0,
        (unsigned)lv_aic_player_pending_cleanup());
}
void lv_aic_plane_test_deinit(void)
{
    rt_base_t level=rt_hw_interrupt_disable();ready=false;pending=NONE;rt_hw_interrupt_enable(level);
    close_panel();
}
static void lv_aic_plane_test(int argc,char **argv)
{
    static const char *const words[]={"show","pause","resume","rotate","pivot","hide","status","close"};
    unsigned cmd=NONE;
    if(argc==2) for(unsigned i=0;i<8;i++) if(!strcmp(argv[1],words[i])) cmd=i+1;
    if(!cmd) { rt_kprintf("lv_aic_plane_test show|pause|resume|rotate|pivot|hide|status|close\n");return; }
    rt_base_t level=rt_hw_interrupt_disable();bool ok=ready && pending==NONE;if(ok) pending=cmd;rt_hw_interrupt_enable(level);
    rt_kprintf(ok?"PLANE request queued\n":"PLANE busy or UI not ready; retry\n");
}
MSH_CMD_EXPORT(lv_aic_plane_test, Video plane acceptance controls on UI thread);
#endif
