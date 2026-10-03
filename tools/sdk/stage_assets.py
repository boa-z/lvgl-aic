#!/usr/bin/env python3
"""Stage only non-product MPP fixtures; retain their licenses and SHA256 inventory."""
import argparse
import hashlib
import json
import shutil
from pathlib import Path

from sdk_paths import sdk_root, component_root, lvgl_root
parser = argparse.ArgumentParser()
parser.add_argument("--fonts", action="store_true")
parser.add_argument("--gif", action="store_true")
parser.add_argument("--aicp", action="store_true")
args = parser.parse_args()
root = sdk_root()
source = component_root() / "tests/data/mpp"
stage = root / "build/lvgl-mpp-data/mpp_test"
stage.mkdir(parents=True, exist_ok=True)
files = {p.name: p for p in source.iterdir() if p.is_file() and p.suffix.lower() in (".jpg", ".png", ".license")}
files.update({"a.jpg": source / "aic_160x120.jpg", "b.png": source / "basn2c08.png", "c.png": source / "basn6a08.png"})
for p in source.iterdir():
    if p.is_file() and ("LICENSE" in p.name or p.name == "README.md"):
        files[p.name] = p
if args.fonts:
    upstream = lvgl_root()
    files.update({
        "Lato-Regular.ttf": upstream / "examples/libs/freetype/Lato-Regular.ttf",
        "NotoSansSC-Regular.ttf": upstream / "tests/src/test_files/fonts/noto/NotoSansSC-Regular.ttf",
        "Lato-OFL.txt": upstream / "scripts/generators/built_in_font/font_license/Lato/OFL.txt",
        "NotoSansSC-OFL.txt": upstream / "scripts/generators/built_in_font/font_license/NotoSansSC/OFL.txt",
    })
if args.gif:
    upstream = lvgl_root()
    files.update({
        "bulb.gif": upstream / "examples/libs/gif/bulb.gif",
        "LVGL-LICENCE.txt": upstream / "LICENCE.txt",
    })
if args.aicp:
    vendor = root / "packages/artinchip/lvgl-ui/aic_demo/aic_widget_demo/img_usage_demo/assets/image"
    files.update({name: vendor / name for name in ("bird.aicp", "flower.aicp")})
# Remove only previously inventoried generated assets absent from this profile.
previous = stage / "SHA256.json"
if previous.exists():
    for name in json.loads(previous.read_text(encoding="utf-8")):
        if name not in files:
            if Path(name).name != name:
                raise ValueError("Invalid staged asset name: " + name)
            path = stage / name
            if path.is_file():
                path.unlink()
inventory = {}
for name, path in sorted(files.items()):
    shutil.copyfile(path, stage / name)
    inventory[name] = hashlib.sha256(path.read_bytes()).hexdigest()
(stage / "SHA256.json").write_text(json.dumps(inventory, indent=2) + "\n", encoding="utf-8")
print("Staged %d fixtures/provenance files in %s" % (len(inventory), stage))
