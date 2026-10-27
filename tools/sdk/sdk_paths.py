"""Resolve an application-owned component and its containing Luban-Lite SDK."""
import os
import re
from pathlib import Path

def component_root():
    return Path(__file__).resolve().parents[2]


def lvgl_root():
    return component_root().parent / "lvgl"


def application_root():
    return component_root().parents[1]


def generated_root(root):
    """与 SCons 的 PRJ_NAME 使用相同的 defconfig 身份，不跨目标复用生成对象。"""
    config = (Path(root) / ".config").read_text(encoding="utf-8")
    match = re.search(r'^CONFIG_PRJ_DEFCONFIG_FILENAME="([^"/\\]+)_defconfig"$', config, re.MULTILINE)
    if not match or match[1] in (".", ".."):
        raise ValueError("Missing or invalid CONFIG_PRJ_DEFCONFIG_FILENAME")
    return Path(root) / "build" / match[1] / "lvgl"


def sdk_root():
    explicit = os.environ.get("LVGL_AIC_SDK_ROOT")
    candidates = [Path(explicit).resolve()] if explicit else component_root().parents
    for root in candidates:
        if (root / "SConstruct").is_file():
            return root
    raise SystemExit("Set LVGL_AIC_SDK_ROOT to the containing Luban-Lite SDK checkout")


if __name__ == "__main__":
    print(sdk_root())
