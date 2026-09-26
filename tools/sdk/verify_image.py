#!/usr/bin/env python3
"""Verify ELF ABI and SDK AIC.FW image metadata/payload CRCs, without hardware."""
from pathlib import Path
import struct
import hashlib
import json
import os
import subprocess
import tempfile
import sys
import zlib


def require(condition, message):
    if not condition:
        raise SystemExit(message)


def cstring(data):
    return data.split(b'\0', 1)[0].decode('ascii')


app, boot = map(Path, sys.argv[1:3])
for label, folder in (('application', app), ('bootloader', boot)):
    data = (folder / 'd13x.elf').read_bytes()
    require(data[:7] == b'\x7fELF\x01\x01\x01', f'{folder}: expected little-endian ELF32')
    require(struct.unpack_from('<H', data, 18)[0] == 243, f'{folder}: expected RISC-V')
    flags = struct.unpack_from('<I', data, 36)[0]
    require(flags & 6 == 4, f'{folder}: expected double-float ABI')
    require((folder / 'd13x.bin').stat().st_size > 0, f'{folder}: empty binary')
    print(f'PASS: {label} ELF32 RISC-V double-float ABI')

images = list(app.glob('*.img'))
require(len(images) == 1, f'Expected one image, found {len(images)}')
data = images[0].read_bytes()
require(len(data) >= 512, 'Truncated firmware header')
require(cstring(data[:8]) == 'AIC.FW', 'Invalid firmware magic')
require(cstring(data[8:72]) == 'd13x', 'Wrong image platform')
require(cstring(data[72:136]) == 'D50T-2-Lite_page_2k_block_128k', 'Wrong image product')
meta_offset, meta_size, file_offset, file_size = struct.unpack_from('<4I', data, 332)
require(file_offset + file_size == len(data), 'Truncated or oversized firmware image')
require(meta_offset >= 512 and meta_size > 0 and
        meta_offset + meta_size == file_offset and meta_size % 512 == 0,
        'Invalid metadata bounds')
components = set()
data_payload = None
for offset in range(meta_offset, meta_offset + meta_size, 512):
    meta = data[offset:offset + 512]
    require(cstring(meta[:8]) == 'META', f'Invalid metadata at {offset}')
    name = cstring(meta[8:72])
    start, size, crc = struct.unpack_from('<3I', meta, 136)
    require(size > 0 and start + size <= len(data), f'{name}: invalid payload bounds')
    if name == 'image.info':
        require(start == 0 and size == meta_offset, 'Invalid image.info header reference')
    else:
        require(start >= file_offset, f'{name}: payload overlaps image metadata')
    require(zlib.crc32(data[start:start + size]) & 0xffffffff == crc,
            f'{name}: CRC mismatch')
    require(name not in components, f'Duplicate component: {name}')
    components.add(name)
    if name == 'image.target.data':
        data_payload = data[start:start + size]
    print(f'  {name}: {size} bytes')
required = {'image.updater.psram', 'image.updater.spl', 'image.info',
            'image.target.spl', 'image.target.env', 'image.target.env_r', 'image.target.os', 'image.target.rodata', 'image.target.data'}
require(required <= components, f'Missing image components: {required - components}')
print(f'PASS: {images[0].name}: {len(components)} payload CRCs, {len(data)} bytes')


from sdk_paths import sdk_root
root = sdk_root()
if len(sys.argv) > 3 and sys.argv[3] in ("mpp", "ge2d"):
    inventory = json.loads((root / "build/lvgl-mpp-data/mpp_test/SHA256.json").read_text())
    tool = root / "tools/scripts" / ("mcopy.exe" if os.name == "nt" else "mcopy")
    with tempfile.TemporaryDirectory(prefix="lvgl-mpp-image-") as directory:
        fat = Path(directory) / "data.fatfs"
        fat.write_bytes(data_payload)
        for name, expected in inventory.items():
            extracted = Path(directory) / name
            result = subprocess.run([str(tool), "-i", str(fat), "::/mpp_test/" + name, name], cwd=directory,
                                    env={**os.environ, "MTOOLS_SKIP_CHECK": "1"}, capture_output=True)
            require(result.returncode == 0, "Packaged asset unreadable: " + name + " " + result.stderr.decode(errors="replace"))
            require(hashlib.sha256(extracted.read_bytes()).hexdigest() == expected,
                    "Packaged asset hash mismatch: " + name)
    print("PASS: %d packaged MPP fixture/provenance hashes" % len(inventory))
print("SHA256: " + hashlib.sha256(data).hexdigest())
