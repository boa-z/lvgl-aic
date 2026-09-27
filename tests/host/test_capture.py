"""Serial capture integrity and channel-order regression (no hardware claim)."""
import importlib.util
from pathlib import Path
import struct
import unittest
import zlib

SOURCE = Path(__file__).resolve().parents[2] / "tools/sdk/capture_to_png.py"
SPEC = importlib.util.spec_from_file_location("capture_to_png", SOURCE)
capture = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(capture)


def fixture(fmt, pixel):
    raw = pixel * 2
    lines = ["boot log", "aic /> AICCAP BEGIN 1 2 1 %s frame=7" % fmt,
             "AICCAP DATA 0 0002%s" % pixel.hex(),
             "AICCAP END lines=1 crc32=%08x" % (zlib.crc32(raw) & 0xffffffff)]
    return chr(10).join(lines)


class CaptureTest(unittest.TestCase):
    def test_formats(self):
        for fmt, pixel in [("RGB565", bytes([0, 248])),
                           ("RGB888", bytes([0, 0, 255])),
                           ("ARGB8888", bytes([0, 0, 255, 255])),
                           ("XRGB8888", bytes([0, 0, 255, 0]))]:
            with self.subTest(fmt=fmt):
                header, raw = capture.decode(fixture(fmt, pixel))
                self.assertEqual(header, (2, 1, fmt, 7))
                self.assertEqual(raw, pixel * 2)
                png = capture.png_bytes(header, raw)
                self.assertEqual(png[:8], bytes([137, 80, 78, 71, 13, 10, 26, 10]))
                offset, idat = 8, b""
                while offset < len(png):
                    size = struct.unpack_from(">I", png, offset)[0]
                    tag = png[offset + 4:offset + 8]
                    data = png[offset + 8:offset + 8 + size]
                    crc = struct.unpack_from(">I", png, offset + 8 + size)[0]
                    self.assertEqual(crc, zlib.crc32(tag + data) & 0xffffffff)
                    if tag == b"IDAT":
                        idat += data
                    offset += size + 12
                self.assertEqual(zlib.decompress(idat), bytes([0, 255, 0, 0, 255, 0, 0]))

    def test_transport_errors(self):
        valid = fixture("RGB888", bytes([4, 8, 16]))
        broken = [valid.replace("DATA 0", "DATA 1"),
                  valid.replace("0002040810", "0002040811"),
                  valid.replace("0002040810", "0000040810"),
                  valid.replace("0002040810", "0003040810"),
                  valid.replace("0002040810", "zzzz040810"),
                  valid[:valid.index("AICCAP END")],
                  valid.replace("lines=1", "lines=2"),
                  valid.replace("2 1 RGB888", "65535 65535 RGB888"),
                  valid + chr(10) + valid]
        for text in broken:
            with self.subTest(text=text), self.assertRaises(ValueError):
                capture.decode(text)


if __name__ == "__main__":
    unittest.main()
