#!/usr/bin/env python3
# SPDX-License-Identifier: Apache-2.0
"""Validate C frame extraction against Pillow using local SDK demo assets.

SDK assets stay outside this repository. This checks decoded frame rectangles,
not APNG composition, playback timing, GE, or hardware MPP decode.
"""
import argparse
import json
import struct
import subprocess
import zlib
from pathlib import Path
from PIL import Image


def chunks(data):
    pos = 8
    while pos < len(data):
        size = struct.unpack_from('>I', data, pos)[0]
        kind = data[pos + 4:pos + 8]
        raw = data[pos + 8:pos + 8 + size]
        assert zlib.crc32(kind + raw) & 0xffffffff == struct.unpack_from('>I', data, pos + 8 + size)[0]
        yield kind, raw
        pos += size + 12
    assert pos == len(data)


def reference_frames(data):
    frames = []
    for kind, raw in chunks(data):
        if kind == b'fcTL':
            _, width, height, x, y, num, den, dispose, blend = struct.unpack('>IIIIIHHBB', raw)
            frames.append(dict(width=width, height=height, x=x, y=y, delay_num=num,
                               delay_den=den or 100, dispose=dispose, blend=blend, data=bytearray()))
        elif kind == b'IDAT' and frames:
            frames[-1]['data'].extend(raw)
        elif kind == b'fdAT':
            frames[-1]['data'].extend(raw[4:])
    return frames


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--sdk', type=Path, required=True)
    parser.add_argument('--exe', type=Path, required=True)
    parser.add_argument('--output', type=Path, required=True)
    args = parser.parse_args()
    assets = args.sdk / 'packages/artinchip/lvgl-ui/aic_demo/aic_widget_demo/aic_apng_demo/assets'
    results = []
    for name in ('clock', 'world-cup', 'ayanami_rei'):
        source = assets / (name + '.png')
        output = args.output / name
        output.mkdir(parents=True, exist_ok=True)
        subprocess.run([str(args.exe.resolve()), str(source.resolve()), str(output.resolve())], check=True)
        frames = reference_frames(source.read_bytes())
        for i, expected in enumerate(frames):
            png = output / ('%04d.png' % i)
            actual = list(chunks(png.read_bytes()))
            assert b''.join(raw for kind, raw in actual if kind == b'IDAT') == expected['data']
            assert all(kind not in (b'acTL', b'fcTL', b'fdAT', b'dcTL') for kind, _ in actual)
            with Image.open(png) as image:
                image.load()
                assert image.size == (expected['width'], expected['height'])
                image.convert('RGBA').tobytes()  # Force palette/tRNS/PNG inflate path.
        results.append(dict(asset=name, extracted_frames=len(frames), decoded='PASS'))
    (args.output / 'result.json').write_text(json.dumps(results, indent=2) + '\n', encoding='utf-8')
    print('PASS SDK APNG extraction and host Pillow rectangle decode:', sum(r['extracted_frames'] for r in results), 'frames')


if __name__ == '__main__':
    main()
