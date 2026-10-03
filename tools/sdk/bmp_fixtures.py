#!/usr/bin/env python3
# SPDX-License-Identifier: Apache-2.0
"""Deterministic first-party BMP probes: two rows of primary-color pixels."""
import struct


def fixtures():
    result = {}
    for bpp, top_down, masked in ((16, False, False), (16, True, True),
                                   (24, False, False), (32, True, False)):
        width, height, offset = 3, 2, 70
        stride = ((width * bpp + 31) // 32) * 4
        data = bytearray(offset + stride * height)
        data[:2] = b"BM"
        struct.pack_into("<I", data, 2, len(data))
        struct.pack_into("<I", data, 10, offset)
        struct.pack_into("<IiiHHI", data, 14, 40, width,
                         -height if top_down else height, 1, bpp, 3 if masked else 0)
        if masked:
            struct.pack_into("<III", data, 54, 0xf800, 0x7e0, 0x1f)
        for y in range(height):
            for x in range(width):
                p = offset + (y if top_down else height - 1 - y) * stride + x * (bpp // 8)
                if bpp == 16:
                    values = (0xf800, 0x7e0, 31) if masked else (0x7c00, 0x3e0, 31)
                    struct.pack_into("<H", data, p, values[(x + y) % 3])
                else:
                    rgb = [(0, 0, 255), (0, 255, 0), (255, 0, 0)][(x + y) % 3]
                    data[p:p+3] = bytes(rgb)
                    if bpp == 32:
                        data[p+3] = 255 if y == 0 else 128
        name = "probe-rgb565.bmp" if masked else "probe-%d.bmp" % bpp
        result[name] = bytes(data)
    return result
