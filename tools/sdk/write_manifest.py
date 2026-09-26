#!/usr/bin/env python3
"""Record source state and artifacts for a local development image."""
import hashlib, json, subprocess, sys, zipfile
from pathlib import Path
from sdk_paths import sdk_root
root=sdk_root()
out=Path(sys.argv[1])
def git(path,*args):
    return subprocess.check_output(["git","-C",str(path),*args])
manifest={"phase":sys.argv[2],"hardware":"D50T-2-Lite","board_validation":"NOT_RUN","sources":{},"files":{}}
for name,path in (("sdk",root),("lvgl-aic",root/"packages/custom/lvgl-aic"),("lvgl",root/"packages/third-party/lvgl")):
    manifest["sources"][name]={"commit":git(path,"rev-parse","HEAD").decode().strip(),"status":git(path,"status","--porcelain").decode()}
    (out/(name+".patch")).write_bytes(git(path,"diff","HEAD","--binary"))
    # git diff does not contain untracked source files. Preserve them separately.
    with zipfile.ZipFile(out/(name+"-untracked.zip"), "w", zipfile.ZIP_DEFLATED) as archive:
        for relative in git(path,"ls-files","--others","--exclude-standard","-z").decode().split("\0"):
            source = path / relative
            if relative and source.is_file():
                source.resolve().relative_to(path.resolve())
                archive.write(source, relative)
manifest["compiler"] = subprocess.check_output(
    [str(root/"toolchain/bin/riscv64-unknown-elf-gcc"), "--version"]).decode().splitlines()[0]

for path in sorted(out.rglob("*")):
    if path.is_file() and path.name not in ("manifest.json","manifest.log"):
        manifest["files"][path.relative_to(out).as_posix()]=hashlib.sha256(path.read_bytes()).hexdigest()
(out/"manifest.json").write_text(json.dumps(manifest,indent=2)+"\n",encoding="utf-8")
