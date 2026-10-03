#!/usr/bin/env python3
# SPDX-License-Identifier: Apache-2.0
"""Check original acceptance assets through C extraction/composition and Pillow rectangles."""
import argparse
import json
import subprocess
import sys
from pathlib import Path
from PIL import Image, ImageChops
from apng_sdk_probe import reference_frames
sys.path.insert(0, str(Path(__file__).resolve().parents[2] / 'tools/sdk'))
from apng_fixtures import fixtures

parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('--exe', type=Path, required=True)
parser.add_argument('--output', type=Path, required=True)
args = parser.parse_args()
results = []
for name, data in fixtures().items():
    out = args.output / name.replace('.png', '')
    out.mkdir(parents=True, exist_ok=True)
    source = out / name
    source.write_bytes(data)
    subprocess.run([str(args.exe.resolve()), str(source.resolve()), str(out.resolve())], check=True)
    frames = reference_frames(data) or [dict(width=128, height=96, x=0, y=0, dispose=0, blend=0)]
    for i, frame in enumerate(frames):
        with Image.open(out / ('%04d.png' % i)) as image:
            assert image.size == (frame['width'], frame['height'])
            (out / ('%04d.rgba' % i)).write_bytes(image.convert('RGBA').tobytes())
    subprocess.run([str(args.exe.resolve()), str(source.resolve()), str(out.resolve()), str(out.resolve())], check=True)
    canvas = Image.new('RGBA', (128, 96))
    maximum = 0
    for i, frame in enumerate(frames):
        before = canvas.copy()
        box = (frame['x'], frame['y'], frame['x'] + frame['width'], frame['y'] + frame['height'])
        with Image.open(out / ('%04d.png' % i)) as image:
            patch = image.convert('RGBA')
        if frame['blend']:
            patch = Image.alpha_composite(canvas.crop(box), patch)
        canvas.paste(patch, box[:2])
        actual = Image.frombytes('RGBA', canvas.size, (out / ('%04d.canvas.rgba' % i)).read_bytes())
        error = max(high for _, high in ImageChops.difference(canvas, actual).getextrema())
        maximum = max(maximum, error)
        assert error <= 1, (name, i, error)
        if len(frames) > 1 and i == 1:
            assert actual.getpixel((20, 20)) == (128, 144, 72, 255)
        if frame['dispose'] == 1:
            canvas.paste((0, 0, 0, 0), box)
        elif frame['dispose'] == 2:
            canvas = before
    results.append(dict(asset=name, frames=len(frames), maximum_error=maximum, status='PASS'))
(args.output / 'result.json').write_text(json.dumps(results, indent=2) + '\n', encoding='utf-8')
print('PASS original APNG fixtures:', sum(r['frames'] for r in results), 'frames')
