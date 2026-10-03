/* SPDX-License-Identifier: Apache-2.0 */
#include <assert.h>
#include <stdatomic.h>
#include <pthread.h>
#include <time.h>
#include <aic_osal.h>
atomic_int inside,completed;
static atomic_int denied,calls,retries,unavailable;
static pthread_mutex_t device=PTHREAD_MUTEX_INITIALIZER;
extern void test_ve_decode(void);
void aicos_msleep(uint32_t ms)
{
    if(ms==5) atomic_fetch_add(&retries,1);
    struct timespec t={ms/1000,(long)(ms%1000)*1000000};nanosleep(&t,NULL);
}
int ve_get_client(void)
{
    atomic_fetch_add(&calls,1);
    if(atomic_load(&unavailable) || pthread_mutex_trylock(&device)) {
        atomic_fetch_add(&denied,1);return -1;
    }
    return 0;
}
void test_ve_put_client(void) { assert(!pthread_mutex_unlock(&device)); }
static void *decode(void *unused)
{ (void)unused;for(unsigned i=0;i<20;i++) test_ve_decode();return NULL; }
int main(void)
{
    atomic_store(&unavailable,1);
    pthread_t first,second;
    assert(!pthread_create(&first,NULL,decode,NULL));
    for(unsigned i=0;i<2000 && atomic_load(&retries)<3;i++) aicos_msleep(1);
    assert(atomic_load(&retries)>=3 && !atomic_load(&inside) && !atomic_load(&completed));
    assert(!pthread_create(&second,NULL,decode,NULL));
    atomic_store(&unavailable,0);
    assert(!pthread_join(first,NULL) && !pthread_join(second,NULL));
    assert(atomic_load(&completed)==40 && !atomic_load(&inside));
    assert(atomic_load(&denied)==atomic_load(&retries));
    assert(atomic_load(&calls)==atomic_load(&denied)+40);
    assert(!pthread_mutex_destroy(&device));
    return 0;
}
