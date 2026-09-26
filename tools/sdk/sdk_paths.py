"""Resolve the integration SDK; never depend on the caller working directory."""
import os
from pathlib import Path

def sdk_root():
    root = Path(os.environ.get("LVGL_AIC_SDK_ROOT", Path(__file__).resolve().parents[5])).resolve()
    if not (root / "SConstruct").is_file():
        raise SystemExit("Set LVGL_AIC_SDK_ROOT to a Luban-Lite SDK checkout")
    return root
