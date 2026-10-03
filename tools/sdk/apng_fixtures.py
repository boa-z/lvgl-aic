#!/usr/bin/env python3
# SPDX-License-Identifier: Apache-2.0
"""Original, deterministic PNG/APNG acceptance assets; stdlib only."""
import struct
import zlib


def chunk(kind, data):
    return struct.pack(">I", len(data)) + kind + data + struct.pack(">I", zlib.crc32(kind + data) & 0xffffffff)


def pixels(w, h, rgba):
    return zlib.compress((b"\0" + bytes(rgba) * w) * h)


def image(animated, plays=0):
    data = b"\x89PNG\r\n\x1a\n" + chunk(b"IHDR", struct.pack(">IIBBBBB", 128, 96, 8, 6, 0, 0, 0))
    if not animated:
        return data + chunk(b"IDAT", pixels(128, 96, (32, 160, 240, 255))) + chunk(b"IEND", b"")
    frames = ((128, 96, 0, 0, 0, 0, (224, 64, 48, 255)),
              (64, 48, 16, 16, 2, 1, (32, 224, 96, 128)),
              (48, 48, 64, 32, 1, 0, (48, 96, 240, 255)),
              (32, 32, 8, 56, 0, 0, (240, 208, 32, 255)))
    data += chunk(b"acTL", struct.pack(">II", len(frames), plays))
    seq = 0
    for i, (w, h, x, y, dispose, blend, color) in enumerate(frames):
        data += chunk(b"fcTL", struct.pack(">IIIIIHHBB", seq, w, h, x, y, 1, 2, dispose, blend))
        seq += 1
        encoded = pixels(w, h, color)
        if i == 0:
            data += chunk(b"IDAT", encoded)
        else:
            data += chunk(b"fdAT", struct.pack(">I", seq) + encoded)
            seq += 1
    return data + chunk(b"IEND", b"")


def fixtures():
    return {"apng-static.png": image(False), "apng-loop.png": image(True),
            "apng-disposal.png": image(True, 2)}
