/* SPDX-License-Identifier: Apache-2.0 */
#pragma once
#include <stddef.h>
#include <stdint.h>
#define MEM_CMA 1
#define AICOS_WAIT_FOREVER UINT32_MAX
typedef void *aicos_mutex_t;
aicos_mutex_t aicos_mutex_create(void);
void aicos_mutex_delete(aicos_mutex_t mutex);
int aicos_mutex_take(aicos_mutex_t mutex,uint32_t timeout);
int aicos_mutex_give(aicos_mutex_t mutex);
void *aicos_malloc_align(unsigned int type,size_t size,size_t align);
void aicos_free_align(unsigned int type,void *ptr);
void aicos_dcache_clean_invalid_range(unsigned long *addr,unsigned long size);
void aicos_dcache_invalid_range(unsigned long *addr,unsigned long size);
