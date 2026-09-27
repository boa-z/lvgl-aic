#!/usr/bin/env python3
"""Decode one lv_aic_capture dump from a serial log; Python stdlib only."""
import argparse
from pathlib import Path
import re
import struct
import zlib

FORMATS = {"RGB565": 2, "RGB888": 3, "ARGB8888": 4, "XRGB8888": 4}


def decode(text):
    header = None
    raw = bytearray()
    sequence = 0
    ended = False
    for line in text.splitlines():
        if "AICCAP " not in line:
            continue
        line = line[line.index("AICCAP "):].strip()
        if ended:
            raise ValueError("More than one capture: save a single BEGIN/END block")
        m = re.fullmatch(r"AICCAP BEGIN 1 (\d+) (\d+) (\w+) frame=(\d+)", line)
        if m:
            if header is not None:
                raise ValueError("Repeated BEGIN before END")
            width, height, fmt, frame = m.groups()
            width, height, frame = int(width), int(height), int(frame)
            if fmt not in FORMATS or not 0 < width <= 65535 or not 0 < height <= 65535:
                raise ValueError("Unsupported dimensions/format")
            bpp = FORMATS[fmt]
            limit = width * height * bpp
            if limit > 64 * 1024 * 1024:
                raise ValueError("Capture exceeds 64 MiB limit")
            header = width, height, fmt, frame
            continue
        if header is None:
            raise ValueError("Missing valid BEGIN")
        m = re.fullmatch(r"AICCAP DATA (\d+) (.+)", line)
        if m:
            if int(m[1]) != sequence:
                raise ValueError("Lost, repeated or reordered DATA line")
            tokens = m[2].split()
            if not 1 <= len(tokens) <= 6:
                raise ValueError("Invalid RLE line")
            for token in tokens:
                if not re.fullmatch(r"[0-9a-fA-F]{%d}" % (4 + 2 * bpp), token):
                    raise ValueError("Invalid RLE token")
                count = int(token[:4], 16)
                if count == 0 or len(raw) + count * bpp > limit:
                    raise ValueError("Invalid RLE length")
                raw.extend(bytes.fromhex(token[4:]) * count)
            sequence += 1
            continue
        m = re.fullmatch(r"AICCAP END lines=(\d+) crc32=([0-9a-fA-F]{8})", line)
        if m:
            if int(m[1]) != sequence or len(raw) != limit:
                raise ValueError("Incomplete capture")
            if zlib.crc32(raw) & 0xffffffff != int(m[2], 16):
                raise ValueError("CRC32 mismatch: corrupted serial capture")
            ended = True
            continue
        raise ValueError("Malformed capture line")
    if not ended:
        raise ValueError("Missing END: wait for UART dump to finish")
    return header, bytes(raw)


def png_bytes(header, raw):
    width, height, fmt, _ = header
    bpp = FORMATS[fmt]
    scanlines = bytearray()
    for y in range(height):
        scanlines.append(0)
        for x in range(width):
            offset = (y * width + x) * bpp
            if fmt == "RGB565":
                value = raw[offset] | raw[offset + 1] << 8
                r, g, b = (value >> 11) & 31, (value >> 5) & 63, value & 31
                scanlines.extend(((r << 3) | (r >> 2), (g << 2) | (g >> 4), (b << 3) | (b >> 2)))
            else:
                # D13x packed RGB/ARGB memory is B,G,R[,A]. Display RGB only.
                scanlines.extend((raw[offset + 2], raw[offset + 1], raw[offset]))
    def chunk(tag, data):
        return struct.pack(">I", len(data)) + tag + data + struct.pack(">I", zlib.crc32(tag + data) & 0xffffffff)
    ihdr = struct.pack(">2I5B", width, height, 8, 2, 0, 0, 0)
    return b"\x89PNG\r\n\x1a\n" + chunk(b"IHDR", ihdr) + chunk(b"IDAT", zlib.compress(scanlines)) + chunk(b"IEND", b"")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("log", type=Path)
    parser.add_argument("png", type=Path)
    args = parser.parse_args()
    try:
        data = args.log.read_bytes()
        encoding = "utf-16" if data[:2] in (b"\xff\xfe", b"\xfe\xff") else "utf-8-sig"
        header, raw = decode(data.decode(encoding, errors="replace"))
        args.png.write_bytes(png_bytes(header, raw))
    except (OSError, ValueError) as exc:
        parser.exit(1, "capture error: %s\n" % exc)
    print("PASS %dx%d %s frame=%d CRC32=%08x -> %s" % (*header, zlib.crc32(raw) & 0xffffffff, args.png))


if __name__ == "__main__":
    main()
