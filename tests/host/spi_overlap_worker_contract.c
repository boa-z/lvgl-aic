/* SPDX-License-Identifier: Apache-2.0 */
#include "spi_overlap_test_support.h"
static lv_aic_spi_pipeline_t *create(void)
{ setup_transfer();return lv_aic_spi_pipeline_create((void *)transfer,4096,20); }
static lv_aic_spi_event_t next(lv_aic_spi_pipeline_t *p)
{
    lv_aic_spi_event_t event;
    for(uint64_t start=now_ms();now_ms()-start<4000;) {
        if(lv_aic_spi_pipeline_take(p,&event)) return event;
        pause_ms();
    }
    assert(!"event timeout");return event;
}
static void finish(lv_aic_spi_pipeline_t *p,bool fault)
{
    lv_aic_spi_pipeline_stop(p);
    assert(!pthread_join(thread,NULL));
    lv_aic_spi_event_t event;assert(!lv_aic_spi_pipeline_take(p,&event));
    assert(lv_aic_spi_pipeline_close(p) && allocated==freed);
    assert(lv_aic_spi_transfer_close(transfer)==(fault?LV_AIC_SPI_FAULT:LV_AIC_SPI_OK));
}
static void pair_contract(unsigned scenario)
{
    lv_aic_spi_pipeline_t *p=create();assert(p);
    uint8_t pixels[2][12]={{42,0},{43,0}};
    lv_aic_spi_rgb565_frame_t f={pixels[0],12,6,3,2};
    if(scenario==1) pixels[1][0]=0;       /* new conversion rejected */
    if(scenario==2) fail_start_at=2;     /* prior OK, new FAULT */
    if(scenario==3) fail_wait_at=1;      /* prior uncertain */
    if(scenario==4) pixels[1][0]=254;   /* transform faults during DMA */
    if(scenario==5) fail_start_at=1;     /* queued second must not submit */
    assert(!lv_aic_spi_pipeline_close(p));
    assert(lv_aic_spi_pipeline_submit(p,&f,0,pixels[0])==LV_AIC_SPI_OK);
    f.data=pixels[1];
    assert(lv_aic_spi_pipeline_submit(p,&f,180,pixels[1])==LV_AIC_SPI_OK);
    assert(lv_aic_spi_pipeline_submit(p,&f,0,NULL)==LV_AIC_SPI_BUSY);
    if(scenario) {
        __atomic_store_n(&allow_wait,1,__ATOMIC_RELEASE);
        lv_aic_spi_pipeline_stop(p); /* queued work must survive stop */
        assert(lv_aic_spi_pipeline_submit(p,&f,0,NULL)==LV_AIC_SPI_INVALID);
    }
    __atomic_store_n(&worker_allowed,1,__ATOMIC_RELEASE);
    lv_aic_spi_event_t event=next(p);
    assert(event.kind==LV_AIC_SPI_SOURCE_RELEASED && event.cookie==pixels[0]);
    assert(event.result==(scenario==5?LV_AIC_SPI_FAULT:LV_AIC_SPI_OK));
    memset(pixels[0],99,12); /* Source released before final DMA wait. */
    if(!scenario) {
        while(!__atomic_load_n(&wait_entered,__ATOMIC_ACQUIRE)) pause_ms();
        assert(!lv_aic_spi_pipeline_take(p,&event));
        __atomic_store_n(&allow_wait,1,__ATOMIC_RELEASE);
    }
    unsigned released=1,completed=0;
    while(completed<2) {
        event=next(p);
        if(event.kind==LV_AIC_SPI_SOURCE_RELEASED) {
            assert(released==1 && event.cookie==pixels[1]);released++;
            assert(event.result==(scenario==1?LV_AIC_SPI_INVALID:
                scenario>=2?LV_AIC_SPI_FAULT:LV_AIC_SPI_OK));
            memset(pixels[1],100,12);
        } else {
            assert(event.cookie==pixels[completed]);
            lv_aic_spi_result_t expected=LV_AIC_SPI_OK;
            if(scenario>=3 || (scenario==2 && completed==1)) expected=LV_AIC_SPI_FAULT;
            if(scenario==1 && completed==1) expected=LV_AIC_SPI_INVALID;
            assert(event.result==expected);completed++;
        }
    }
    assert(released==2);
    finish(p,scenario>=2);
    assert(starts==(scenario==0 || scenario==2?2:1));
    if(scenario!=5) assert(concurrent==1);
    else assert(transforms==1);
}
static void stress_contract(void)
{
    lv_aic_spi_pipeline_t *p=create();assert(p);
    fail_wake=true; /* Timeout must rescue jobs and final completion. */
    __atomic_store_n(&worker_allowed,1,__ATOMIC_RELEASE);
    __atomic_store_n(&allow_wait,1,__ATOMIC_RELEASE);
    uint8_t pixels[2][12]={{1},{2}};
    unsigned sent=0,released=0,completed=0;
    bool available[2]={true,true};
    uint64_t deadline=now_ms()+15000;
    while(completed<256) {
        assert(now_ms()<deadline);
        if(sent<256 && available[sent%2]) {
            lv_aic_spi_rgb565_frame_t f={pixels[sent%2],12,6,3,2};
            lv_aic_spi_result_t r=lv_aic_spi_pipeline_submit(p,&f,0,(void *)(uintptr_t)(sent+1));
            assert(r==LV_AIC_SPI_OK || r==LV_AIC_SPI_BUSY);
            if(r==LV_AIC_SPI_OK) { available[sent%2]=false;sent++; }
        }
        lv_aic_spi_event_t event;
        if(lv_aic_spi_pipeline_take(p,&event)) {
            assert(event.result==LV_AIC_SPI_OK);
            if(event.kind==LV_AIC_SPI_SOURCE_RELEASED) {
                assert((uintptr_t)event.cookie==released+1);
                available[released%2]=true;pixels[released%2][0]=(uint8_t)(1+released%200);released++;
            } else { assert((uintptr_t)event.cookie==completed+1);completed++; }
        } else pause_ms();
    }
    finish(p,false);fail_wake=false;
    assert(sent==256 && released==256 && starts==256 && waits==256);
}
static void single_contract(bool busy)
{
    lv_aic_spi_pipeline_t *p=create();assert(p);
    force_busy=busy;if(!busy) fail_wait_at=1;
    uint8_t pixels[12]={42};lv_aic_spi_rgb565_frame_t f={pixels,12,6,3,2};
    assert(lv_aic_spi_pipeline_submit(p,&f,0,pixels)==LV_AIC_SPI_OK);
    __atomic_store_n(&worker_allowed,1,__ATOMIC_RELEASE);
    __atomic_store_n(&allow_wait,1,__ATOMIC_RELEASE);
    lv_aic_spi_event_t event=next(p);
    assert(event.kind==LV_AIC_SPI_SOURCE_RELEASED && event.result==(busy?LV_AIC_SPI_FAULT:LV_AIC_SPI_OK));
    event=next(p);assert(event.kind==LV_AIC_SPI_COMPLETED && event.result==LV_AIC_SPI_FAULT);
    assert(lv_aic_spi_pipeline_submit(p,&f,0,pixels)==LV_AIC_SPI_FAULT);
    finish(p,!busy); /* ownership violation faults pipeline, not our idle fake session */
}
int main(void)
{
    setbuf(stdout,NULL);
    lv_init();lv_aic_spi_session_t *s=(void *)(uintptr_t)1;
    assert(!lv_aic_spi_pipeline_create(NULL,4096,20));
    assert(!lv_aic_spi_pipeline_create(s,512,20));
    assert(!lv_aic_spi_pipeline_create(s,4096,32));
    fail_sem=true;assert(!lv_aic_spi_pipeline_create(s,4096,20));fail_sem=false;
    fail_thread=true;assert(!lv_aic_spi_pipeline_create(s,4096,20));fail_thread=false;
    assert(allocated==freed);
    for(unsigned i=0;i<6;i++) { printf("pair %u\n",i);pair_contract(i); }
    puts("stress");stress_contract();puts("final wait fault");single_contract(false);puts("ownership fault");single_contract(true);
    return 0;
}
