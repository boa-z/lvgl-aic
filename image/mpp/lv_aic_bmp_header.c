/* SPDX-License-Identifier: Apache-2.0 */
#include "lv_aic_bmp_header.h"
#include <string.h>
static uint16_t le16(const uint8_t *p)
{ return (uint16_t)p[0] | (uint16_t)((uint16_t)p[1] << 8); }
static uint32_t le32(const uint8_t *p)
{ return (uint32_t)p[0] | (uint32_t)p[1] << 8 | (uint32_t)p[2] << 16 | (uint32_t)p[3] << 24; }
bool lv_aic_bmp_parse_header(const uint8_t *data, uint32_t available,
                             uint32_t file_size, lv_aic_bmp_header_t *out)
{
    if (!out) return false;
    memset(out, 0, sizeof(*out));
    if (!data || available < 54 || file_size < 54 || data[0] != 'B' || data[1] != 'M') return false;
    uint32_t declared = le32(data + 2), offset = le32(data + 10), dib = le32(data + 14);
    uint32_t width = le32(data + 18), raw_height = le32(data + 22);
    uint16_t bpp = le16(data + 28);
    if (dib < 40 || (uint64_t)dib + 14 > offset || offset > file_size ||
        le16(data + 26) != 1 || le32(data + 30) != 0 ||
        (bpp != 24 && bpp != 32) || !width || width > 4096 ||
        !raw_height || raw_height == 0x80000000U) return false;
    bool top_down = (raw_height & 0x80000000U) != 0;
    uint32_t height = top_down ? 0U - raw_height : raw_height;
    if (height > 4096 || (uint64_t)width * height > 4096U * 4096U) return false;
    uint32_t stride = ((width * bpp + 31U) / 32U) * 4U;
    uint64_t end = (uint64_t)offset + (uint64_t)stride * height;
    if (end > file_size || (declared && (declared < end || declared > file_size))) return false;
    out->width = width; out->height = height; out->offset = offset;
    out->stride = stride; out->bpp = bpp; out->top_down = top_down;
    return true;
}
