#!/usr/bin/env python3
# SPDX-License-Identifier: Apache-2.0
"""Static Gate 1 checks for the Luban-Lite LVGL 9.6 integration."""

import argparse
import re
import subprocess
from pathlib import Path


REQUIRED_CONFIG_SYMBOLS = (
    "LPKG_USING_LVGL",
    "LVGL_V_9",
    "LPKG_LVGL_IMPL_AIC",
    "LPKG_MPP",
    "KERNEL_RTTHREAD",
    "RT_TOUCH_PIN_IRQ",
    "AIC_PAN_DISPLAY",
    "AIC_TOUCH_PANEL_GT911",
    "AIC_LVGL_PORT",
    "AIC_LVGL_USE_DISPLAY",
    "AIC_LVGL_USE_TOUCH",
)

DISABLED_CONFIG_SYMBOLS = (
    "LPKG_LVGL_IMPL_LEGACY",
    "AIC_LVGL_USE_GE2D",
    "AIC_LVGL_USE_MPP_DEC",
    "AIC_LVGL_USE_FT_CACHE",
)

REQUIRED_MAP_SYMBOLS = {
    "lv_init": r"third-party[\\/]lvgl[\\/]src",
    "lv_display_create": r"third-party[\\/]lvgl[\\/]src",
    "lv_obj_create": r"third-party[\\/]lvgl[\\/]src",
    "lv_thread_init": r"third-party[\\/]lvgl[\\/]src[\\/]osal[\\/]lv_rtthread\.o",
    "lv_thread_sync_init": r"third-party[\\/]lvgl[\\/]src[\\/]osal[\\/]lv_rtthread\.o",
    "lv_malloc_core": r"third-party[\\/]lvgl[\\/]src[\\/]stdlib[\\/]rtthread[\\/]lv_mem_core_rtthread\.o",
    "lv_draw_sw_init": r"third-party[\\/]lvgl[\\/]src[\\/]draw[\\/]sw[\\/]lv_draw_sw\.o",
    "lv_aic_init": r"custom[\\/]lvgl-aic[\\/]port[\\/]lv_aic\.o",
    "lv_aic_display_init": r"custom[\\/]lvgl-aic[\\/]port[\\/]lv_aic_display\.o",
    "lv_aic_indev_init": r"custom[\\/]lvgl-aic[\\/]port[\\/]lv_aic_indev\.o",
}


def run_git(root, *args):
    return subprocess.check_output(
        ["git", "-C", str(root), *args], text=True, stderr=subprocess.STDOUT
    ).strip()


def fail(message):
    raise SystemExit("Gate 1 check FAILED: " + message)


def check_submodule(root, path, expected):
    submodule = root / path
    if not submodule.is_dir():
        fail("missing submodule: %s" % path)
    actual = run_git(submodule, "rev-parse", "HEAD")
    if actual != expected:
        fail("%s is %s, expected %s" % (path, actual, expected))


def check_config(root, phase="gate1"):
    config = (root / ".config").read_text(encoding="utf-8")
    header = (root / "rtconfig.h").read_text(encoding="utf-8")
    for symbol in REQUIRED_CONFIG_SYMBOLS:
        if not re.search(r"^CONFIG_%s=y$" % re.escape(symbol), config, re.MULTILINE):
            fail(".config does not enable %s" % symbol)
        if not re.search(r"^#define %s(?:\s|$)" % re.escape(symbol), header, re.MULTILINE):
            fail("rtconfig.h does not define %s" % symbol)
    if '#define AIC_LVGL_TOUCH_DEVICE "gt911"' not in header:
        fail("rtconfig.h does not select AIC_LVGL_TOUCH_DEVICE=gt911")
    disabled = list(DISABLED_CONFIG_SYMBOLS)
    if phase in ("mpp", "ge2d"):
        disabled.remove("AIC_LVGL_USE_MPP_DEC")
        if not re.search(r"^CONFIG_AIC_LVGL_USE_MPP_DEC=y$", config, re.MULTILINE):
            fail("MPP test profile must enable AIC_LVGL_USE_MPP_DEC")
        if not re.search(r"^#define AIC_LVGL_USE_MPP_DEC(?:\s|$)", header, re.MULTILINE):
            fail("MPP test header must enable AIC_LVGL_USE_MPP_DEC")
    if phase == "ge2d":
        # The GE2D profile is the MPP profile plus the draw unit, so the MPP
        # regression is observable on the same image.
        disabled.remove("AIC_LVGL_USE_GE2D")
        if not re.search(r"^CONFIG_AIC_LVGL_USE_GE2D=y$", config, re.MULTILINE):
            fail("GE2D test profile must enable AIC_LVGL_USE_GE2D")
        if not re.search(r"^#define AIC_LVGL_USE_GE2D(?:\s|$)", header, re.MULTILINE):
            fail("GE2D test header must enable AIC_LVGL_USE_GE2D")
    for symbol in disabled:
        if re.search(r"^CONFIG_%s=y$" % re.escape(symbol), config, re.MULTILINE):
            fail("forbidden Phase 1 symbol enabled: %s" % symbol)
        if re.search(r"^#define %s(?:\s|$)" % re.escape(symbol), header, re.MULTILINE):
            fail("forbidden Phase 1 symbol present in rtconfig.h: %s" % symbol)


def check_map(map_path):
    text = map_path.read_text(encoding="utf-8", errors="replace")
    if re.search(r"packages[\\/]artinchip[\\/]lvgl-ui|lvgl_v9[\\/]lvgl", text, re.IGNORECASE):
        fail("legacy ArtInChip LVGL object/path appears in link map")
    if re.search(r"env_support[\\/]rt-thread", text, re.IGNORECASE):
        fail("upstream RT-Thread entry-point object appears in link map")
    if not re.search(r"packages[\\/]third-party[\\/]lvgl[\\/]src", text, re.IGNORECASE):
        fail("LVGL 9.6 upstream src objects are absent from link map")
    if not re.search(r"packages[\\/]custom[\\/]lvgl-aic[\\/]port", text, re.IGNORECASE):
        fail("lvgl-aic port objects are absent from link map")

    lines = text.splitlines()
    for symbol, object_pattern in REQUIRED_MAP_SYMBOLS.items():
        indexes = [index for index, line in enumerate(lines)
                   if re.search(r"\b%s\b" % re.escape(symbol), line)]
        if not indexes:
            fail("required symbol is absent from link map: %s" % symbol)
        contexts = ["\n".join(lines[max(0, index - 4):index + 5])
                    for index in indexes]
        if any(re.search(r"lvgl-ui|lvgl_v9", context, re.IGNORECASE)
               for context in contexts):
            fail("symbol %s resolves to legacy LVGL" % symbol)
        if not any(re.search(object_pattern, context, re.IGNORECASE)
                   for context in contexts):
            fail("symbol %s does not resolve to its required object" % symbol)
        print("symbol %s: required object verified" % symbol)


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--root", type=Path, required=True)
    parser.add_argument("--map", dest="map_path", type=Path, required=True)
    parser.add_argument("--phase", choices=("gate1", "mpp", "ge2d"), default="gate1")
    parser.add_argument("--allow-component-dirty", action="store_true",
                        help="Development builds only; preserve the component diff with evidence")
    args = parser.parse_args()

    root = args.root.resolve()
    map_path = args.map_path.resolve()

    check_submodule(root, "packages/third-party/lvgl", "80ca777e37a2b176770726a02e07a6fb79ef0b39")
    check_submodule(root, "packages/custom/lvgl-aic", run_git(root, "rev-parse", "HEAD:packages/custom/lvgl-aic"))
    if run_git(root / "packages/third-party/lvgl", "status", "--porcelain"):
        fail("LVGL submodule has local modifications")
    if not args.allow_component_dirty and run_git(root / "packages/custom/lvgl-aic", "status", "--porcelain"):
        fail("lvgl-aic submodule has local modifications")
    for submodule in (root / "packages/third-party/lvgl", root / "packages/custom/lvgl-aic"):
        if any(submodule.rglob("*.o")):
            fail("object files leaked into submodule source: %s" % submodule)
    if not map_path.is_file():
        fail("link map does not exist: %s" % map_path)
    try:
        map_path.relative_to(root)
    except ValueError:
        fail("link map is outside the integration checkout: %s" % map_path)
    check_config(root, args.phase)
    check_map(map_path)
    if args.phase in ("mpp", "ge2d"):
        text = map_path.read_text(encoding="utf-8", errors="replace")
        for symbol in ("lv_aic_mpp_decoder_init", "mpp_decoder_decode", "lv_aic_mpp_test_run"):
            if not re.search(r"^\s+0x[0-9a-f]+\s+" + symbol + r"\s*$", text, re.MULTILINE):
                fail("MPP live symbol absent: " + symbol)
    if args.phase == "ge2d":
        # The draw unit must be linked from our own sources, not from the
        # legacy ArtInChip lvgl-ui tree (already rejected in check_map).
        text = map_path.read_text(encoding="utf-8", errors="replace")
        required_ge2d = (
            ("lv_draw_aic_ge2d_init",
             r"custom[\\/]lvgl-aic[\\/]draw[\\/]ge2d[\\/]lv_draw_aic_ge2d\.o"),
            ("lv_draw_aic_ge2d_fill",
             r"custom[\\/]lvgl-aic[\\/]draw[\\/]ge2d[\\/]lv_draw_aic_ge2d_fill\.o"),
            ("lv_aic_ge2d_test_run",
             r"custom[\\/]lvgl-aic[\\/]tests[\\/]manual[\\/]lv_aic_ge2d_test\.o"),
        )
        for symbol, object_pattern in required_ge2d:
            indexes = [index for index, line in enumerate(text.splitlines())
                       if re.search(r"\b%s\b" % re.escape(symbol), line)]
            if not indexes:
                fail("GE2D live symbol absent: " + symbol)
            lines = text.splitlines()
            contexts = ["\n".join(lines[max(0, index - 4):index + 5])
                        for index in indexes]
            if not any(re.search(object_pattern, context, re.IGNORECASE)
                       for context in contexts):
                fail("GE2D symbol %s does not resolve to its required object" % symbol)
            print("symbol %s: required object verified" % symbol)
        # The engine entry points prove the backend actually talks to the GE
        # driver instead of quietly falling back to software.
        for symbol in ("mpp_ge_open", "mpp_ge_fillrect", "mpp_ge_emit", "mpp_ge_sync"):
            if not re.search(r"^\s+0x[0-9a-f]+\s+" + symbol + r"\s*$", text, re.MULTILINE):
                fail("GE2D engine symbol absent: " + symbol)
    print(args.phase + " static checks: PASS (not board validation)")


if __name__ == "__main__":
    main()
