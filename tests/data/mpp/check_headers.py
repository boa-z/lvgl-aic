#!/usr/bin/env python3
"""Host-side cross-check for lv_aic_mpp_decoder header rules.

Mirrors the accept/reject logic in image/mpp/lv_aic_mpp_decoder.c
(JPEG SOF0/SOF1 + 8-bit + 1/3 components; PNG IHDR color types 2/6/3)
against the vendored assets in this directory. No MPP hardware needed.

This script audits fixture headers and CRCs using Python's zlib. Production
C boundary checks are exercised by tests/host/mpp_contract.c. Neither host
test proves SDK hardware decoding or support for palette/interlaced PNGs.

Usage: python3 check_headers.py
Exit 0 when every asset matches EXPECTED, else 1 with a diff table.
SPDX-License-Identifier: Apache-2.0
"""
import struct
import sys
import zlib
from pathlib import Path

HERE = Path(__file__).resolve().parent

MAX_DIM = 4096
MAX_PIXELS = 8 * 1024 * 1024

# Fixtures whose PNG chunk CRCs are intentionally corrupt, with the chunk that
# was damaged. Everything else must have valid CRCs on every chunk. This is the
# fixture-side half of the decoder's integrity gate: if one of these is ever
# regenerated as a valid PNG, the board harness silently stops testing
# corruption rejection, so the mistake is caught here instead.
CORRUPT_CRC = {
    "bad_crc_rgb.png": "IDAT",
    "xcsn0g01.png": "IDAT",
    "xhdn0g08.png": "IHDR",
}

# name -> (kind, w, h, lv_cf or None when rejected at info stage)
EXPECTED = {
    # JPEG baseline 4:2:0, 3 components -> RGB888
    "testorig.jpg": ("jpg", 227, 149, "RGB888"),
    "testimg.jpg": ("jpg", 227, 149, "RGB888"),
    "testimgint.jpg": ("jpg", 227, 149, "RGB888"),
    "aic_800x480.jpg": ("jpg", 800, 480, "RGB888"),
    "aic_801x479.jpg": ("jpg", 801, 479, "RGB888"),
    "aic_160x120.jpg": ("jpg", 160, 120, "RGB888"),
    # JPEG negatives: arithmetic (SOF9), progressive (SOF2), empty
    "testimgari.jpg": ("jpg", None, None, None),
    "testimgp.jpg": ("jpg", None, None, None),
    "empty.jpg": ("jpg", None, None, None),
    # PNG RGB / RGBA / 8-bit palette -> accept
    "basn2c08.png": ("png", 32, 32, "RGB888"),
    "basn6a08.png": ("png", 32, 32, "ARGB8888"),
    "basn3p08.png": ("png", 32, 32, "ARGB8888"),
    # Sub-8-bit palette parses at info, but the SDK MPP rejects every
    # bit_depth != 8 at IHDR, so open is INVALID (see README limitations).
    "s33n3p04.png": ("png", 33, 33, "ARGB8888*"),
    "s37n3p04.png": ("png", 37, 37, "ARGB8888*"),
    "z09n2c08.png": ("png", 32, 32, "RGB888"),
    "testorig.png": ("png", None, None, None),  # gray
    "basn0g08.png": ("png", None, None, None),  # gray
    # Interlaced RGB parses at info stage; MPP decode rejects it (open INVALID)
    "basi2c08.png": ("png", 32, 32, "RGB888*"),
    # Corrupt: bad IDAT CRC parses at info, fails at MPP decode
    "bad_crc_rgb.png": ("png", 32, 32, "RGB888*"),
    "xcsn0g01.png": ("png", None, None, None),  # gray 1-bit
    "xhdn0g08.png": ("png", None, None, None),  # bad IHDR CRC + gray
    "empty.png": ("png", None, None, None),
}


def jpeg_info(data):
    if len(data) < 2 or data[0] != 0xFF or data[1] != 0xD8:
        return None
    i = 2
    while i + 3 < len(data):
        if data[i] != 0xFF:
            i += 1
            continue
        marker = (0xFF << 8) | data[i + 1]
        size = (data[i + 2] << 8) | data[i + 3]
        if size < 2:
            return None
        if marker in (0xFFC0, 0xFFC1):
            if i + 2 + 15 > len(data):
                return None
            prec = data[i + 4]
            h = (data[i + 5] << 8) | data[i + 6]
            w = (data[i + 7] << 8) | data[i + 8]
            n = data[i + 9]
            if prec != 8 or w <= 0 or h <= 0 or n not in (1, 3):
                return None
            if w > MAX_DIM or h > MAX_DIM or w * h > MAX_PIXELS:
                return None
            return (w, h, "RGB888")
        i += 2 + size
        if i > 8 * 1024 * 1024:
            return None
    return None


def png_info(data):
    sig = bytes([137, 80, 78, 71, 13, 10, 26, 10])
    if len(data) < 33 or data[:8] != sig:
        return None
    if struct.unpack(">I", data[8:12])[0] != 13 or data[12:16] != b"IHDR":
        return None
    w = struct.unpack(">I", data[16:20])[0]
    h = struct.unpack(">I", data[20:24])[0]
    depth, ctype = data[24], data[25]
    if w <= 0 or h <= 0 or w > MAX_DIM or h > MAX_DIM or w * h > MAX_PIXELS:
        return None
    if ctype == 2 and depth == 8:
        return (w, h, "RGB888")
    if ctype == 6 and depth == 8:
        return (w, h, "ARGB8888")
    if ctype == 3 and depth in (1, 2, 4, 8):
        return (w, h, "ARGB8888")
    return None


def png_bad_chunks(data):
    """Chunk types whose CRC32 does not match. None when the buffer is not a
    PNG, is truncated, or never reaches IEND."""
    sig = bytes([137, 80, 78, 71, 13, 10, 26, 10])
    if len(data) < 8 or data[:8] != sig:
        return None
    off, bad = 8, []
    while off + 12 <= len(data):
        clen = struct.unpack(">I", data[off:off + 4])[0]
        if clen > len(data) - off - 12:
            return None
        ctype = data[off + 4:off + 8]
        want = struct.unpack(">I", data[off + 8 + clen:off + 12 + clen])[0]
        if zlib.crc32(data[off + 4:off + 8 + clen]) & 0xFFFFFFFF != want:
            bad.append(ctype.decode("latin1"))
        off += 12 + clen
        if ctype == b"IEND":
            return bad if clen == 0 else None
    return None


def main():
    failures = []
    for name, exp in sorted(EXPECTED.items()):
        data = (HERE / name).read_bytes() if (HERE / name).exists() else b""
        got = jpeg_info(data) if name.endswith(".jpg") else png_info(data)
        exp_key = None if exp[1] is None else exp[1:]
        got_key = None if got is None else got
        # '*' marks info-accept/decode-reject assets; header check expects accept
        if exp[3] is not None and exp[3].endswith("*"):
            exp_key = (exp[1], exp[2], exp[3].rstrip("*"))
        status = "OK " if got_key == exp_key else "FAIL"
        if got_key != exp_key:
            failures.append(name)
        print(f"{status} {name:18s} expected={exp_key} got={got_key}")

    crc_failures = []
    for name in sorted(EXPECTED):
        if not name.endswith(".png"):
            continue
        data = (HERE / name).read_bytes() if (HERE / name).exists() else b""
        got_bad = png_bad_chunks(data)
        if name in CORRUPT_CRC:
            want_bad = [CORRUPT_CRC[name]]
        elif name == "empty.png":
            want_bad = None  # not a PNG at all
        else:
            want_bad = []
        ok = got_bad == want_bad
        if not ok:
            crc_failures.append(name)
        print(f"{'OK ' if ok else 'FAIL'} {name:18s} crc_bad={got_bad} expected={want_bad}")
    failures += crc_failures

    if failures:
        print(f"\n{len(failures)} mismatch(es): {failures}")
        return 1
    print(f"\nAll {len(EXPECTED)} header decisions match lv_aic_mpp_decoder.c rules.")
    print(f"All {len(CORRUPT_CRC)} corrupt-CRC fixtures still corrupt; "
          "every other PNG has valid chunk CRCs.")
    return 0


if __name__ == "__main__":
    sys.exit(main())
