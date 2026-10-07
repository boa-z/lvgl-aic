/* SPDX-License-Identifier: Apache-2.0 */
#include <assert.h>
#include <string.h>
#include "lv_aic_bmp_header.h"
static void put32(uint8_t *p, uint32_t v)
{ for (unsigned i = 0; i < 4; i++) p[i] = (uint8_t)(v >> (8 * i)); }
int main(void)
{
    uint8_t b[70] = {'B','M'};
    lv_aic_bmp_header_t h;
    put32(b + 10, 70); put32(b + 14, 40);
    put32(b + 18, 3); put32(b + 22, 2);
    b[26] = 1; b[28] = 24;
    assert(lv_aic_bmp_parse_header(b, 54, 94, &h));
    assert(h.width == 3 && h.height == 2 && h.stride == 12 && h.offset == 70 && !h.top_down);
    put32(b + 22, (uint32_t)-2);
    assert(lv_aic_bmp_parse_header(b, 54, 94, &h) && h.top_down);
    for (uint32_t n = 0; n < 54; n++) assert(!lv_aic_bmp_parse_header(b, n, 94, &h));
    assert(!lv_aic_bmp_parse_header(b, 54, 93, &h));
    put32(b + 10, 53); assert(!lv_aic_bmp_parse_header(b, 54, 94, &h));
    put32(b + 10, 70); put32(b + 18, 0xffffffff);
    assert(!lv_aic_bmp_parse_header(b, 54, 94, &h));
    put32(b + 18, 3); put32(b + 22, 0x80000000U);
    assert(!lv_aic_bmp_parse_header(b, 54, 94, &h));
    put32(b + 22, 2); b[28] = 16;
    assert(lv_aic_bmp_parse_header(b, 54, 94, &h) && h.rgb555);
    b[28] = 32;
    assert(lv_aic_bmp_parse_header(b, 54, 94, &h));
    put32(b + 30, 1); assert(!lv_aic_bmp_parse_header(b, 54, 94, &h));
    put32(b + 30, 0); put32(b + 2, 93);
    assert(!lv_aic_bmp_parse_header(b, 54, 94, &h));
    put32(b + 2, 0); b[28] = 16; put32(b + 30, 3);
    put32(b + 54, 0xf800); put32(b + 58, 0x7e0); put32(b + 62, 31);
    assert(lv_aic_bmp_parse_header(b, 70, 94, &h) && !h.rgb555);
    assert(!lv_aic_bmp_parse_header(b, 65, 94, &h));
    put32(b + 62, 0x7e0);
    assert(!lv_aic_bmp_parse_header(b, 70, 94, &h));
    return 0;
}
