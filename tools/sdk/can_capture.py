#!/usr/bin/env python3
"""Receive an lv_aic_can_capture stream and write a PNG; stdlib + python-can.

Device framing (CAN ID 0x1CA, 500 kbit/s, 8 bytes, little-endian):
  BEGIN flags=0x01: seq=0 | w | h | fmt
  DATA  flags=0x00: seq | count | p0 p1 p2   (seq 1,2,... modulo 65536)
  END   flags=0x02: seq=data frame count mod 65536 | crc32 (zlib)
fmt: 0=RGB565 1=RGB888 2=ARGB8888 3=XRGB8888. RLE runs and the CRC32 match
the UART capture, so PNG encoding is shared with capture_to_png.py.

--trigger sends a 0x1CB "CAP" frame after the bus is open, so no serial
console is needed; the board's CAN OTA endpoint (autostarted) receives it.
Without --trigger, start the board with `lv_aic_can_capture start` after
this script is listening.

Wiring: board CANH/CANL to the adapter, 120 ohm termination on the bus,
adapter + board at the same bitrate. The adapter must stay on the bus:
classic CAN needs a second node for ACK.

Examples:
  pip install python-can
  python can_capture.py --interface pcan --channel PCAN_USBBUS1 --trigger out.png
  python can_capture.py --interface slcan --channel COM9 out.png
  python can_capture.py --interface socketcan --channel can0 --trigger out.png
  python can_capture.py --selftest  # no hardware: protocol round-trip
"""
import argparse
import sys
import time
import zlib
from pathlib import Path

CAP_ID = 0x1CA
TRIGGER_ID = 0x1CB
BEGIN, DATA, END = 0x01, 0x00, 0x02
FORMATS = {0: ("RGB565", 2), 1: ("RGB888", 3), 2: ("ARGB8888", 4), 3: ("XRGB8888", 4)}

sys.path.insert(0, str(Path(__file__).resolve().parent))
from capture_to_png import png_bytes


def decode(frames):
    header = None
    raw = bytearray()
    want = 1
    count_data = 0
    limit = 0
    bpp = 0
    for arb_id, payload in frames:
        if arb_id != CAP_ID or len(payload) != 8:
            continue
        seq = payload[0] | payload[1] << 8
        flags = payload[7]
        if flags == BEGIN:
            if header is not None:
                raise ValueError("Repeated BEGIN")
            w = payload[2] | payload[3] << 8
            h = payload[4] | payload[5] << 8
            fmt = payload[6]
            if fmt not in FORMATS or not 0 < w <= 65535 or not 0 < h <= 65535:
                raise ValueError("Bad BEGIN dimensions/format")
            name, bpp = FORMATS[fmt]
            limit = w * h * bpp
            if limit > 64 * 1024 * 1024:
                raise ValueError("Capture exceeds 64 MiB limit")
            header = (w, h, name, 0)
        elif flags == DATA:
            if header is None:
                raise ValueError("DATA before BEGIN")
            if seq != want:
                raise ValueError("Lost/reordered frame: got %d want %d" % (seq, want))
            count = payload[2] | payload[3] << 8
            if count == 0 or len(raw) + count * bpp > limit:
                raise ValueError("Invalid RLE length")
            raw.extend(bytes(payload[4:4 + bpp]) * count)
            want = (want + 1) & 0xFFFF
            count_data += 1
        elif flags == END:
            if header is None:
                raise ValueError("END before BEGIN")
            if seq != count_data & 0xFFFF:
                raise ValueError("END count mismatch")
            crc = payload[2] | payload[3] << 8 | payload[4] << 16 | payload[5] << 24
            if len(raw) != limit:
                raise ValueError("Incomplete capture")
            if zlib.crc32(raw) & 0xFFFFFFFF != crc:
                raise ValueError("CRC32 mismatch")
            return header, bytes(raw)
        else:
            raise ValueError("Unknown flags %02x" % flags)
    raise ValueError("Missing END: stream interrupted")


def encode_frames(width, height, fmt, pixels):
    """Test helper: build a frame list the way the firmware does."""
    name, bpp = FORMATS[fmt]
    yield CAP_ID, bytes((0, 0, width & 0xFF, width >> 8, height & 0xFF,
                         height >> 8, fmt, BEGIN))
    seq = 1
    i = 0
    n = width * height
    while i < n:
        run = 1
        while i + run < n and run < 65535 and \
                pixels[(i + run) * bpp:(i + run + 1) * bpp] == pixels[i * bpp:(i + 1) * bpp]:
            run += 1
        wire = seq & 0xFFFF
        yield CAP_ID, bytes((wire & 0xFF, wire >> 8, run & 0xFF, run >> 8,
                             *pixels[i * bpp:(i + 1) * bpp],
                             *bytes(3 - bpp), DATA))
        seq += 1
        i += run
    crc = zlib.crc32(pixels) & 0xFFFFFFFF
    count = (seq - 1) & 0xFFFF
    yield CAP_ID, bytes((count & 0xFF, count >> 8, crc & 0xFF,
                         crc >> 8 & 0xFF, crc >> 16 & 0xFF, crc >> 24 & 0xFF,
                         0, END))


def selftest():
    import random
    random.seed(0xA1C)
    for fmt in (0, 1):
        name, bpp = FORMATS[fmt]
        pixels = bytearray()
        while len(pixels) < 96 * 64 * bpp:
            pixels.extend(bytes(random.randrange(256) for _ in range(bpp)) * random.randrange(1, 40))
        pixels = bytes(pixels[:96 * 64 * bpp])
        header, raw = decode(list(encode_frames(96, 64, fmt, pixels)))
        assert header[:3] == (96, 64, name) and raw == pixels, fmt
    # Alternating pixels: 800x100 = 80000 runs, crossing the 16-bit wrap.
    pixels = (b"\x00\xf8\x1f\x00" * 40000)
    frames = list(encode_frames(800, 100, 0, pixels))
    assert len(frames) - 2 == 80000
    header, raw = decode(frames)
    assert raw == pixels
    for bad in (frames[:70000] + frames[70001:], frames[:-1]):
        try:
            decode(bad)
        except ValueError:
            continue
        raise AssertionError("corrupt stream accepted")
    print("PASS can_capture protocol round-trip RGB565/RGB888, 80000-run seq wrap, loss/END checks")


def main():
    parser = argparse.ArgumentParser(description=__doc__,
                                     formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--selftest", action="store_true")
    parser.add_argument("--interface", default="slcan")
    parser.add_argument("--channel", default="")
    parser.add_argument("--bitrate", type=int, default=500000)
    parser.add_argument("--timeout", type=float, default=120.0)
    parser.add_argument("--trigger", action="store_true",
                        help="send the 0x1CB CAP frame to start the board capture")
    parser.add_argument("png", type=Path, nargs="?")
    args = parser.parse_args()
    if args.selftest:
        selftest()
        return
    if not args.channel or args.png is None:
        parser.exit(2, "capture needs --channel and a PNG path\n")
    try:
        import can
    except ImportError:
        parser.exit(1, "pip install python-can\n")
    bus = can.interface.Bus(channel=args.channel, interface=args.interface,
                            bitrate=args.bitrate)
    frames = []
    started = None
    try:
        if args.trigger:
            bus.send(can.Message(arbitration_id=TRIGGER_ID, data=b"CAP",
                                 is_extended_id=False), timeout=1)
        while True:
            msg = bus.recv(args.timeout)
            if msg is None:
                parser.exit(1, "timeout waiting for CAN frames\n")
            if msg.arbitration_id != CAP_ID or msg.is_extended_id or msg.is_remote_frame:
                continue
            data = bytes(msg.data)
            # Ignore the tail of an earlier stream until a BEGIN arrives.
            if started is None:
                if len(data) != 8 or data[7] != BEGIN:
                    continue
                started = time.monotonic()
            frames.append((msg.arbitration_id, data))
            if len(data) == 8 and data[7] == END:
                break
    finally:
        bus.shutdown()
    elapsed = time.monotonic() - started
    header, raw = decode(frames)
    args.png.write_bytes(png_bytes(header, raw))
    print("PASS %dx%d %s CRC32=%08x frames=%d %.1fs (%.0f frames/s) -> %s"
          % (*header[:3], zlib.crc32(raw) & 0xFFFFFFFF, len(frames), elapsed,
             len(frames) / elapsed if elapsed else 0, args.png))


if __name__ == "__main__":
    main()
