/* SPDX-License-Identifier: Apache-2.0 */
#include <stdint.h>
#include <assert.h>
typedef uintptr_t ulong; /* SDK ulong is pointer-sized on the 32-bit target. */
typedef uint32_t u32;
#define ALIGN_DOWN(x,a) ((x)&~((a)-1))
#define ALIGN_UP(x,a) (((x)+(a)-1)&~((a)-1))
void aicos_dcache_clean_range(unsigned long *p,unsigned long n);
#include "../../draw/ge2d/lv_draw_aic_ge2d_utils.c"
void aicos_dcache_clean_range(unsigned long *p,unsigned long n) { (void)p;(void)n;assert(0); }
void aicos_dcache_clean_invalid_range(unsigned long *p,unsigned long n) { (void)p;(void)n;assert(0); }
bool lv_aic_pixel_format_is_ge2d_dst(lv_color_format_t cf) { (void)cf;return false; }
bool lv_aic_yuv_layout(lv_aic_yuv_format_t f,uint32_t w,uint32_t h,lv_aic_yuv_layout_t *l)
{ (void)f;(void)w;(void)h;(void)l;return false; }
int main(void)
{
    lv_draw_buf_t b={0};assert(!lv_draw_aic_ge2d_buf_address_valid(NULL));
    assert(!lv_draw_aic_ge2d_buf_address_valid(&b));
    b.data=(void *)(uintptr_t)0x40000000;b.data_size=0;
    assert(!lv_draw_aic_ge2d_buf_address_valid(&b));
    b.data_size=64;assert(lv_draw_aic_ge2d_buf_address_valid(&b));
    b.data=(void *)(uintptr_t)0x3fffffff;assert(!lv_draw_aic_ge2d_buf_address_valid(&b));
    b.data=(void *)(uintptr_t)0xffffffc0;assert(lv_draw_aic_ge2d_buf_address_valid(&b));
    b.data_size=65;assert(!lv_draw_aic_ge2d_buf_address_valid(&b));
    b.data=(void *)(uintptr_t)UINT32_MAX;b.data_size=1;assert(lv_draw_aic_ge2d_buf_address_valid(&b));
    b.data_size=2;assert(!lv_draw_aic_ge2d_buf_address_valid(&b));
#if UINTPTR_MAX > UINT32_MAX
    b.data=(void *)(uintptr_t)UINT64_C(0x140000000);b.data_size=64;
    assert(!lv_draw_aic_ge2d_buf_address_valid(&b));
#endif
    return 0;
}
