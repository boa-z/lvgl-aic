"""Resolve an application-owned component and its containing Luban-Lite SDK."""
import os
from pathlib import Path

def component_root():
    return Path(__file__).resolve().parents[2]


def lvgl_root():
    return component_root().parent / "lvgl"


def application_root():
    return component_root().parents[1]


def sdk_root():
    explicit = os.environ.get("LVGL_AIC_SDK_ROOT")
    candidates = [Path(explicit).resolve()] if explicit else component_root().parents
    for root in candidates:
        if (root / "SConstruct").is_file():
            return root
    raise SystemExit("Set LVGL_AIC_SDK_ROOT to the containing Luban-Lite SDK checkout")


if __name__ == "__main__":
    print(sdk_root())
