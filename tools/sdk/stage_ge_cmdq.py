#!/usr/bin/env python3
# SPDX-License-Identifier: Apache-2.0
"""Generate an app-owned CMDQ backend; never edit the SDK source."""
import hashlib
import json
from pathlib import Path

SOURCE = "packages/artinchip/mpp/ge/cmdq_ops.c"
REVIEWED_SHA256 = "7c9b73f113354b5403cc929fe8b9c93f9276d7f2daa93dd2c96ff8295c80b8c0"

def digest(text):
    return hashlib.sha256(text.encode("utf-8")).hexdigest()

def corrected(text):
    text = text.replace("\r\n", "\n")
    if digest(text) != REVIEWED_SHA256:
        raise ValueError("SDK CMDQ source changed; review gradient adapter before building")
    edits = [("void update_gradient_cmd(", "static void update_gradient_cmd("), ("cmd[3] = SRC_GRADIENT_STEP_SET(g_step);",
              "cmd[2] = SRC_GRADIENT_STEP_SET(g_step);"),
             ("struct ge_ops ge_cmdq_ops =", "struct ge_ops __wrap_ge_cmdq_ops =")]
    for channel in "argb":
        edits.append(("((end_%s - start_%s) << 16)" % (channel, channel),
                      "((end_%s - start_%s) * 65536)" % (channel, channel)))
    for old, new in edits:
        if text.count(old) != 1:
            raise ValueError("Unexpected SDK CMDQ patch site: " + old)
        text = text.replace(old, new)
    return text

def generate(sdk_root, destination):
    source = Path(sdk_root) / SOURCE
    original = source.read_text(encoding="utf-8")
    result = corrected(original)
    destination = Path(destination)
    destination.parent.mkdir(parents=True, exist_ok=True)
    if not destination.exists() or destination.read_text(encoding="utf-8") != result:
        destination.write_bytes(result.encode("utf-8"))
    evidence = {"sdk_source": SOURCE, "normalized_source_sha256": digest(original),
                "normalized_generated_sha256": digest(result),
                "operations_symbol": "__wrap_ge_cmdq_ops"}
    destination.with_suffix(".json").write_text(json.dumps(evidence, indent=2)+"\n", encoding="utf-8")
    return str(destination)
