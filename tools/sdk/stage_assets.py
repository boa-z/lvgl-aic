#!/usr/bin/env python3
"""Stage only non-product MPP fixtures; retain their licenses and SHA256 inventory."""
import hashlib
import json
import shutil
from pathlib import Path

from sdk_paths import sdk_root
root = sdk_root()
source = root / "packages/custom/lvgl-aic/tests/data/mpp"
stage = root / "build/lvgl-mpp-data/mpp_test"
stage.mkdir(parents=True, exist_ok=True)
files = {p.name: p for p in source.iterdir() if p.is_file() and p.suffix.lower() in (".jpg", ".png", ".license")}
files.update({"a.jpg": source / "aic_160x120.jpg", "b.png": source / "basn2c08.png", "c.png": source / "basn6a08.png"})
for p in source.iterdir():
    if p.is_file() and ("LICENSE" in p.name or p.name == "README.md"):
        files[p.name] = p
inventory = {}
for name, path in sorted(files.items()):
    shutil.copyfile(path, stage / name)
    inventory[name] = hashlib.sha256(path.read_bytes()).hexdigest()
(stage / "SHA256.json").write_text(json.dumps(inventory, indent=2) + "\n", encoding="utf-8")
print("Staged %d fixtures/provenance files in %s" % (len(inventory), stage))
