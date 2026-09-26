#pragma once
#include <stddef.h>
#define MEM_CMA 1
void *aicos_malloc_align(unsigned int type, size_t size, size_t align);
void aicos_free_align(unsigned int type, void *ptr);
void aicos_dcache_clean_invalid_range(unsigned long *addr, unsigned long size);
void aicos_dcache_invalid_range(unsigned long *addr, unsigned long size);
