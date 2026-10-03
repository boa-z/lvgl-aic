/* SPDX-License-Identifier: Apache-2.0 */
#include "lv_aic_spi_worker.h"
#include "lvgl.h"
#include <aic_osal.h>
#include <pthread.h>
#include <assert.h>
#include <stdlib.h>
#include <time.h>
static pthread_t thread;
static void (*entry_fn)(void *);
static void *entry_context;
static bool fail_sem,fail_thread,fail_wake;
static unsigned allocated,freed;
static void pause_ms(void) { struct timespec t={0,1000000};nanosleep(&t,NULL); }
aicos_sem_t aicos_sem_create(uint32_t count)
{ if(fail_sem) return NULL;unsigned *s=malloc(sizeof(*s));assert(s);*s=count;allocated++;return s; }
void aicos_sem_delete(aicos_sem_t s) { free(s);freed++; }
int aicos_sem_give(aicos_sem_t s)
{ if(fail_wake) return -1;__atomic_add_fetch((unsigned *)s,1,__ATOMIC_RELEASE);return 0; }
int aicos_sem_take(aicos_sem_t s,uint32_t ms)
{
    for(unsigned i=0;i<ms;i++) {
        unsigned n=__atomic_load_n((unsigned *)s,__ATOMIC_ACQUIRE);
        if(n && __atomic_compare_exchange_n((unsigned *)s,&n,n-1,false,__ATOMIC_ACQUIRE,__ATOMIC_RELAXED)) return 0;
        pause_ms();
    }
    return -1;
}
static void *entry(void *unused) { (void)unused;entry_fn(entry_context);return NULL; }
aicos_thread_t aicos_thread_create(const char *name,uint32_t stack,uint32_t priority,
    void (*fn)(void *),void *context)
{
    assert(name && stack==4096 && priority==20);
    if(fail_thread) return NULL;
    entry_fn=fn;entry_context=context;assert(!pthread_create(&thread,NULL,entry,NULL));return &thread;
}

#include "lv_aic_spi_display.h"
#include "lv_aic_spi_panel.h"
#include <rtdevice.h>
#include <string.h>
#ifdef _WIN32
#include <windows.h>
#else
#include <sys/mman.h>
#endif
static uint8_t *dma;
static unsigned dma_allocs,dma_frees,commands,frames,cleans,status;
static bool data_mode,fail_pixel_wait;
static const uint8_t *inflight;
static size_t inflight_bytes;
static uint8_t snapshot[12];
void aicos_msleep(unsigned int ms) { while(ms--) pause_ms(); }
void *aicos_malloc_align(unsigned int type,size_t bytes,size_t alignment)
{ assert(type==MEM_CMA && bytes==64 && alignment==64);dma_allocs++;return dma; }
void aicos_free_align(unsigned int type,void *p)
{ assert(type==MEM_CMA && p==dma && !status);dma_frees++; }
void aicos_dcache_clean_range(unsigned long *p,unsigned long bytes)
{ assert(((void *)p==dma || (void *)p==dma+4096) && bytes==64 && !status);cleans++; }
static void present(void) {}
static bool dc(void *context,bool mode)
{ assert(context==dma && !status);data_mode=mode;return true; }
rt_uint32_t rt_spi_get_transfer_status(struct rt_spi_device *d) { assert(d);return status; }
int rt_spi_nonblock_set(struct rt_spi_device *d,unsigned mode) { assert(d && mode==1);return 0; }
size_t rt_qspi_transfer_message(struct rt_qspi_device *d,struct rt_qspi_message *m)
{
    assert(d && !status && m->qspi_data_lines==1 && m->parent.cs_take && m->parent.cs_release);
    const uint8_t *p=m->parent.send_buf;
    if(m->parent.length==1) {
        assert(!data_mode && p==dma+4096 && p[0]==0x2c && commands==frames);commands++;
    } else {
        assert(data_mode && p==dma && m->parent.length==12 && commands==frames+1);
        for(unsigned i=0;i<12;i+=2) {
            if(frames<6) assert(p[i]==((frames%2)?0:255) && p[i+1]==p[i]);
            else assert(p[i]==0xf8 && p[i+1]==0);
        }
        frames++;
    }
    inflight=p;inflight_bytes=m->parent.length;memcpy(snapshot,p,inflight_bytes);status=1;
    return m->parent.length;
}
int rt_spi_wait_completion(struct rt_spi_device *d)
{
    assert(d && status==1);pause_ms();
    assert(!memcmp(inflight,snapshot,inflight_bytes));
    if(fail_pixel_wait && inflight_bytes==12) return -1;
    status=0;return 0;
}
int main(void)
{
    lv_init();
#ifdef _WIN32
    dma=VirtualAlloc((void *)(uintptr_t)0x48000000,8192,MEM_RESERVE|MEM_COMMIT,PAGE_READWRITE);
#else
    dma=mmap((void *)(uintptr_t)0x48000000,8192,PROT_READ|PROT_WRITE,MAP_PRIVATE|MAP_ANONYMOUS|MAP_FIXED_NOREPLACE,-1,0);
#endif
    assert(dma==(void *)(uintptr_t)0x48000000);memset(dma,0xa5,8192);dma[4096]=0x2c;
    struct rt_spi_ops ops={present,present,present,present,present};
    struct rt_spi_bus bus={&ops};struct rt_qspi_device device={{&bus}};
    lv_aic_spi_panel_step_t step={.data=dma+4096,.bytes=1,.capacity=64,.data_lines=1};
    lv_aic_spi_panel_t *panel=lv_aic_spi_panel_create(&device,&step,1,3,2,dc,dma,true);assert(panel);
    lv_aic_spi_session_config_t config={.device=&device,.width=3,.height=2,.data_lines=1,.swap_bytes=true,
        .prepare=lv_aic_spi_panel_prepare,.prepare_context=panel};
    lv_aic_spi_session_t *session=lv_aic_spi_session_open_owned(&config,64);assert(session);
    lv_aic_spi_display_t *display=lv_aic_spi_display_create_buffered(session,8,4,90,1024,2,4096,20);assert(display);
    lv_display_t *lv=lv_aic_spi_display_get(display);lv_obj_t *screen=lv_display_get_screen_active(lv);
    lv_obj_set_style_bg_opa(screen,LV_OPA_COVER,0);
    for(unsigned i=0;i<6;i++) {
        lv_obj_set_style_bg_color(screen,(i%2)?lv_color_black():lv_color_white(),0);
        lv_obj_invalidate(screen);lv_refr_now(lv);
    }
    lv_aic_spi_result_t claim;
    while((claim=lv_aic_spi_display_claim_blit(display))==LV_AIC_SPI_BUSY) pause_ms();
    assert(claim==LV_AIC_SPI_OK);
    uint8_t red[64];for(unsigned i=0;i<64;i+=2) { red[i]=0;red[i+1]=0xf8; }
    lv_aic_spi_rgb565_frame_t source={red,64,16,8,4};
    assert(lv_aic_spi_display_blit(display,&source,90)==LV_AIC_SPI_OK);
    lv_aic_spi_result_t result;
    while(!lv_aic_spi_display_blit_take(display,&result)) pause_ms();
    assert(result==LV_AIC_SPI_OK);
    lv_aic_spi_display_stats_t stats;assert(lv_aic_spi_display_stats(display,&stats));
    assert(stats.accepted==7 && stats.completed==7 && !stats.failed && !stats.pending);
    while(!lv_aic_spi_display_close(display)) pause_ms();
    assert(!pthread_join(thread,NULL));
    assert(lv_aic_spi_session_close(session)==LV_AIC_SPI_OK && lv_aic_spi_panel_close(panel));
    assert(frames==7 && commands==7 && cleans==14 && allocated==freed && dma_allocs==dma_frees);
    for(unsigned i=12;i<64;i++) assert(dma[i]==0xa5);
    /* Repeat the entire chain with uncertain pixel DMA completion. */
    frames=commands=cleans=0;fail_pixel_wait=true;
    panel=lv_aic_spi_panel_create(&device,&step,1,3,2,dc,dma,true);assert(panel);
    config.prepare_context=panel;session=lv_aic_spi_session_open_owned(&config,64);assert(session);
    display=lv_aic_spi_display_create_buffered(session,8,4,90,1024,2,4096,20);assert(display);
    lv=lv_aic_spi_display_get(display);screen=lv_display_get_screen_active(lv);
    lv_obj_set_style_bg_color(screen,lv_color_white(),0);lv_obj_set_style_bg_opa(screen,LV_OPA_COVER,0);
    lv_refr_now(lv);
    while((claim=lv_aic_spi_display_claim_blit(display))==LV_AIC_SPI_BUSY) pause_ms();
    assert(claim==LV_AIC_SPI_FAULT);
    assert(lv_aic_spi_display_stats(display,&stats) && stats.accepted==1 && stats.failed==1 && !stats.completed);
    while(!lv_aic_spi_display_close(display)) pause_ms();
    assert(!pthread_join(thread,NULL));
    assert(lv_aic_spi_session_close(session)==LV_AIC_SPI_FAULT);
    assert(allocated==freed && dma_allocs==dma_frees+1 && status==1);
    assert(frames==1 && commands==1 && !memcmp(dma,snapshot,12));
    /* Faulted session, panel context and DMA mapping intentionally stay alive. */
    return 0;
}
