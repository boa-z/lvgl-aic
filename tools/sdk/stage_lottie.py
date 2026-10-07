#!/usr/bin/env python3
# SPDX-License-Identifier: Apache-2.0
"""Build-only correction for the pinned ThorVG solid-layer opacity."""
import argparse
import hashlib
from pathlib import Path

REVIEWED_SHA256 = "720b66834db2889c29b3dd956cdda586b926818b2401375bfdc6126c7be0a617"

def corrected(text):
    text = text.replace("\r\n", "\n")
    if hashlib.sha256(text.encode("utf-8")).hexdigest() != REVIEWED_SHA256:
        raise ValueError("ThorVG Lottie builder changed; review opacity correction before building")
    old = "    solidFill->opacity(layer->cache.opacity);"
    if text.count(old) != 1:
        raise ValueError("Unexpected Lottie solid-layer patch site")
    # updateLayer already assigns this opacity to the containing scene.
    return text.replace(old, "    solidFill->opacity(255);  // Layer scene applies opacity exactly once.")

def generate(source, destination):
    result = corrected(Path(source).read_text(encoding="utf-8"))
    destination = Path(destination)
    destination.parent.mkdir(parents=True, exist_ok=True)
    if not destination.exists() or destination.read_text(encoding="utf-8") != result:
        destination.write_bytes(result.encode("utf-8"))
    return str(destination)

if __name__ == "__main__":
    parser = argparse.ArgumentParser()
    parser.add_argument("--source", required=True)
    parser.add_argument("--output", required=True)
    args = parser.parse_args()
    generate(args.source, args.output)
