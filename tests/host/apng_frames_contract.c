/* SPDX-License-Identifier: Apache-2.0 */
#include "lv_aic_apng_frames.h"
#include "lv_aic_rgb_image_private.h"
#include <aic_osal.h>
#include <pthread.h>
#include <assert.h>
#include <stdlib.h>
#include <string.h>
aicos_mutex_t aicos_mutex_create(void)
{ pthread_mutex_t *m=malloc(sizeof(*m));assert(m && !pthread_mutex_init(m,NULL));return m; }
void aicos_mutex_delete(aicos_mutex_t m) { assert(!pthread_mutex_destroy(m));free(m); }
int aicos_mutex_take(aicos_mutex_t m,uint32_t timeout)
{ assert(timeout==AICOS_WAIT_FOREVER);return pthread_mutex_lock(m); }
int aicos_mutex_give(aicos_mutex_t m) { return pthread_mutex_unlock(m); }
static void *producer(void *arg)
{
    uint8_t rgba[16]={17,29,43,128,55,66,77,255,0xa5,0xa5,0xa5,0xa5};
    for(uint64_t i=10;i<1010;i++) lv_aic_apng_frames_publish(arg,rgba,16,sizeof(rgba),i);
    return NULL;
}
int main(void)
{
    lv_init();
    assert(!lv_aic_apng_frames_create(0,1,2,4096));
    assert(!lv_aic_apng_frames_create(2,1,1,4096));
    assert(!lv_aic_apng_frames_create(2,1,2,1));
    lv_aic_apng_frames_t *p=lv_aic_apng_frames_create(2,1,2,4096);assert(p);
    uint8_t rgba[16]={17,29,43,128,55,66,77,255};
    lv_aic_rgb_image_t *a=NULL,*b=NULL;uint64_t seq=99;
    assert(!lv_aic_apng_frames_publish(p,rgba,7,sizeof(rgba),1));
    assert(!lv_aic_apng_frames_publish(p,rgba,16,7,1));
    assert(lv_aic_apng_frames_publish(p,rgba,16,sizeof(rgba),1));
    assert(!lv_aic_apng_frames_poll(p,&a,&seq) && !a && seq==99); /* No decoder. */
    assert(lv_aic_rgb_image_decoder_init());
    assert(lv_aic_apng_frames_publish(p,rgba,16,sizeof(rgba),2));
    assert(lv_aic_apng_frames_publish(p,rgba,16,sizeof(rgba),3));
    assert(!lv_aic_apng_frames_publish(p,rgba,16,sizeof(rgba),2));
    assert(lv_aic_apng_frames_poll(p,&a,&seq) && seq==3);
    const lv_aic_rgb_frame_t *frame;
    lv_aic_rgb_image_t *reader=lv_aic_rgb_image_acquire(lv_aic_rgb_image_source(a),&frame);assert(reader);
    assert(frame->capacity==8 && frame->stride==8 && ((const uint32_t *)frame->data)[0]==0x80111d2b);
    assert(!lv_aic_apng_frames_publish(p,frame->data,8,8,4)); /* Pool alias. */
    memset(rgba,0,sizeof(rgba));assert(((const uint32_t *)frame->data)[0]==0x80111d2b);
    assert(lv_aic_apng_frames_publish(p,rgba,16,sizeof(rgba),4));
    assert(lv_aic_apng_frames_poll(p,&b,&seq) && seq==4);
    lv_aic_rgb_image_destroy(a); /* Reader, including quarantined GE, keeps slot. */
    assert(!lv_aic_apng_frames_publish(p,rgba,16,sizeof(rgba),5));
    assert(((const uint32_t *)frame->data)[1]==0xff37424d);
    lv_aic_rgb_image_destroy(b);
    assert(lv_aic_apng_frames_publish(p,rgba,16,sizeof(rgba),5));
    lv_aic_apng_frames_close(p);
    assert(!lv_aic_apng_frames_poll(p,&b,&seq));
    assert(!lv_aic_apng_frames_publish(p,rgba,16,sizeof(rgba),6));
    assert(!lv_aic_apng_frames_destroy(p));
    lv_aic_rgb_image_release_lease(reader);assert(lv_aic_apng_frames_destroy(p));
    /* Lossless mode preserves the oldest ready frame, even if polling fails
     * before the RGB decoder exists. Disabling restores latest-wins behavior. */
    assert(lv_aic_rgb_image_decoder_deinit());
    p=lv_aic_apng_frames_create(2,1,2,4096);assert(p);
    lv_aic_apng_frames_preserve(p,true);
    assert(lv_aic_apng_frames_publish(p,rgba,16,sizeof(rgba),1));
    assert(lv_aic_apng_frames_pending(p) && lv_aic_apng_frames_blocked(p));
    a=NULL;seq=99;
    assert(!lv_aic_apng_frames_poll(p,&a,&seq) && seq==99 && !a);
    assert(!lv_aic_apng_frames_publish(p,rgba,16,sizeof(rgba),2));
    assert(lv_aic_rgb_image_decoder_init());
    assert(lv_aic_apng_frames_poll(p,&a,&seq) && seq==1);
    assert(!lv_aic_apng_frames_pending(p));
    assert(lv_aic_apng_frames_publish(p,rgba,16,sizeof(rgba),2));
    lv_aic_apng_frames_preserve(p,false);
    assert(!lv_aic_apng_frames_blocked(p) && lv_aic_apng_frames_pending(p));
    assert(lv_aic_apng_frames_publish(p,rgba,16,sizeof(rgba),3));
    assert(lv_aic_apng_frames_poll(p,&b,&seq) && seq==3);
    lv_aic_rgb_image_destroy(a);lv_aic_rgb_image_destroy(b);
    assert(lv_aic_apng_frames_destroy(p));
    p=lv_aic_apng_frames_create(2,1,3,4096);assert(p);
    pthread_t thread;assert(!pthread_create(&thread,NULL,producer,p));
    uint64_t last=0;
    for(unsigned i=0;i<2000;i++) if(lv_aic_apng_frames_poll(p,&a,&seq)) {
        assert(seq>last);last=seq;
        reader=lv_aic_rgb_image_acquire(lv_aic_rgb_image_source(a),&frame);assert(reader);
        assert(((const uint32_t *)frame->data)[0]==0x80111d2b);
        lv_aic_rgb_image_destroy(a);lv_aic_rgb_image_release_lease(reader);
    }
    assert(!pthread_join(thread,NULL));
    if(lv_aic_apng_frames_poll(p,&a,&seq)) { assert(seq>last);lv_aic_rgb_image_destroy(a); }
    assert(lv_aic_apng_frames_destroy(p));
    assert(lv_aic_rgb_image_decoder_deinit());lv_deinit();return 0;
}
