#pragma once
#include <stdio.h>
#define LOG_LVL_INFO 1
#define LOG_I(...) ((void)0)
#define LOG_E(...) do { fprintf(stderr,__VA_ARGS__); fputc(10,stderr); } while(0)
#define LOG_W(...) ((void)0)
static inline void ulog_flush(void) {}
