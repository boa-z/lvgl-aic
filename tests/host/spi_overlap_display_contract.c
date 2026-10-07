/* SPDX-License-Identifier: Apache-2.0 */
#include "spi_overlap_test_support.h"
#include "lv_aic_spi_display.h"
void aicos_msleep(unsigned int ms) { while(ms--) pause_ms(); }
lv_aic_spi_result_t lv_aic_spi_session_submit(lv_aic_spi_session_t *s,
    const lv_aic_spi_rgb565_frame_t *f,unsigned degrees)
{ return lv_aic_spi_session_submit_ex(s,f,degrees,NULL); }
static lv_aic_spi_display_t *create_display(unsigned count)
{
    setup_transfer();render_pixels=true;
    lv_aic_spi_display_t *d=lv_aic_spi_display_create_pipelined((void *)transfer,8,4,0,1024,count,4096,20);
    assert(d);
    lv_obj_t *screen=lv_display_get_screen_active(lv_aic_spi_display_get(d));
    lv_obj_set_style_bg_color(screen,lv_color_white(),0);
    lv_obj_set_style_bg_opa(screen,LV_OPA_COVER,0);
    __atomic_store_n(&worker_allowed,1,__ATOMIC_RELEASE);
    return d;
}
static void render(lv_aic_spi_display_t *d,bool red)
{
    lv_display_t *display=lv_aic_spi_display_get(d);
    lv_obj_t *screen=lv_display_get_screen_active(display);
    lv_obj_set_style_bg_color(screen,red?lv_color_hex(0xff0000):lv_color_white(),0);
    lv_obj_invalidate(screen);lv_refr_now(display);
}
static void poll_done(lv_aic_spi_display_t *d)
{
    uint64_t deadline=now_ms()+4000;
    while(!lv_aic_spi_display_poll(d)) { assert(now_ms()<deadline);pause_ms(); }
}
static void close_display(lv_aic_spi_display_t *d,bool fault)
{
    uint64_t deadline=now_ms()+4000;
    while(!lv_aic_spi_display_close(d)) { assert(now_ms()<deadline);pause_ms(); }
    assert(!pthread_join(thread,NULL));
    assert(allocated==freed);
    assert(lv_aic_spi_transfer_close(transfer)==(fault?LV_AIC_SPI_FAULT:LV_AIC_SPI_OK));
}
int main(void)
{
    lv_init();
    lv_aic_spi_display_t *d=create_display(2);
    render(d,false);render(d,true);
    lv_aic_spi_display_stats_t stats;
    assert(lv_aic_spi_display_stats(d,&stats) && stats.accepted==2 && stats.completed==0 && stats.pending);
    /* Second refresh returned while the first transport wait is held. The old
     * display worker would block here waiting for DMA completion before ready. */
    assert(!lv_aic_spi_display_poll(d));
    __atomic_store_n(&allow_wait,1,__ATOMIC_RELEASE);poll_done(d);
    assert(lv_aic_spi_display_stats(d,&stats) && stats.completed==2 && !stats.failed && !stats.pending);
    for(unsigned i=0;i<12;i++) render(d,i%2);
    poll_done(d);assert(lv_aic_spi_display_stats(d,&stats) && stats.completed==14 && stats.accepted==14);
    assert(lv_aic_spi_display_claim_blit(d)==LV_AIC_SPI_OK);
    uint8_t external[12]={42};lv_aic_spi_rgb565_frame_t f={external,12,6,3,2};
    assert(lv_aic_spi_display_blit(d,&f,0)==LV_AIC_SPI_OK);
    assert(!lv_aic_spi_display_poll(d)); /* Must not steal direct blit result. */
    lv_aic_spi_result_t result;
    uint64_t deadline=now_ms()+4000;
    while(!lv_aic_spi_display_blit_take(d,&result)) { assert(now_ms()<deadline);pause_ms(); }
    assert(result==LV_AIC_SPI_OK && lv_aic_spi_display_poll(d));
    close_display(d,false);

    /* One draw buffer is also safe: reuse after source release while previous
     * transfer remains live; close must wait for checked completion. */
    d=create_display(1);render(d,false);render(d,true);
    assert(!lv_aic_spi_display_close(d));
    __atomic_store_n(&allow_wait,1,__ATOMIC_RELEASE);close_display(d,false);

    /* Direct LVGL deletion must retain borrowed draw pixels until worker exit. */
    d=create_display(2);render(d,false);render(d,true);
    lv_display_delete(lv_aic_spi_display_get(d));
    assert(!lv_aic_spi_display_get(d) && !lv_aic_spi_display_close(d));
    __atomic_store_n(&allow_wait,1,__ATOMIC_RELEASE);close_display(d,false);

    /* A fault starting frame 2 must keep frame 1 counted as completed. */
    d=create_display(2);fail_start_at=2;render(d,false);render(d,true);
    __atomic_store_n(&allow_wait,1,__ATOMIC_RELEASE);poll_done(d);
    assert(lv_aic_spi_display_stats(d,&stats));
    assert(stats.accepted==2 && stats.completed==1 && stats.failed==1 && !stats.pending);
    assert(lv_aic_spi_display_result(d)==LV_AIC_SPI_FAULT);
    assert(lv_aic_spi_display_claim_blit(d)==LV_AIC_SPI_FAULT);
    close_display(d,true);
    return 0;
}
