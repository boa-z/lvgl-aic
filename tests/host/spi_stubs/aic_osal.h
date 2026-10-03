#pragma once
void aicos_dcache_clean_range(unsigned long *address,unsigned long bytes);
#include <stddef.h>
#define MEM_CMA 1
void *aicos_malloc_align(unsigned int type,size_t bytes,size_t alignment);
void aicos_free_align(unsigned int type,void *pointer);

#include <stdint.h>
typedef void *aicos_sem_t;
typedef void *aicos_thread_t;
aicos_sem_t aicos_sem_create(uint32_t count);
void aicos_sem_delete(aicos_sem_t sem);
int aicos_sem_take(aicos_sem_t sem,uint32_t milliseconds);
int aicos_sem_give(aicos_sem_t sem);
aicos_thread_t aicos_thread_create(const char *name,uint32_t stack,uint32_t priority,
    void (*entry)(void *),void *context);

#define RT_THREAD_PRIORITY_MAX 32

void aicos_msleep(unsigned int milliseconds);

void aicos_dcache_clean_invalid_range(unsigned long *address,unsigned long bytes);
void aicos_dcache_invalid_range(unsigned long *address,unsigned long bytes);
