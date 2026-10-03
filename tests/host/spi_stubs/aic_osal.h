#pragma once
void aicos_dcache_clean_range(unsigned long *address,unsigned long bytes);
#include <stddef.h>
#define MEM_CMA 1
void *aicos_malloc_align(unsigned int type,size_t bytes,size_t alignment);
void aicos_free_align(unsigned int type,void *pointer);
