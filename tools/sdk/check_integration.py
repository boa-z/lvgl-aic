#!/usr/bin/env python3
# SPDX-License-Identifier: Apache-2.0
"""Static Gate 1 checks for the Luban-Lite LVGL 9.6 integration."""

import argparse
import re
import subprocess
from pathlib import Path
from sdk_paths import component_root, lvgl_root


REQUIRED_CONFIG_SYMBOLS = (
    "RT_USING_EVENT",
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
    "LPKG_USING_LVGL",
    "AIC_LVGL_USE_GE2D",
    "AIC_LVGL_USE_MPP_DEC",
    "AIC_LVGL_USE_FT_CACHE",
)

REQUIRED_MAP_SYMBOLS = {
    "lv_init": r"third_party[\\/]lvgl[\\/]src",
    "lv_display_create": r"third_party[\\/]lvgl[\\/]src",
    "lv_obj_create": r"third_party[\\/]lvgl[\\/]src",
    "lv_thread_init": r"third_party[\\/]lvgl-aic[\\/]compat[\\/]lv_aic_rtthread_os\.o",
    "lv_thread_sync_init": r"third_party[\\/]lvgl-aic[\\/]compat[\\/]lv_aic_rtthread_os\.o",
    "lv_malloc_core": r"third_party[\\/]lvgl[\\/]src[\\/]stdlib[\\/]rtthread[\\/]lv_mem_core_rtthread\.o",
    "lv_draw_sw_init": r"third_party[\\/]lvgl[\\/]src[\\/]draw[\\/]sw[\\/]lv_draw_sw\.o",
    "lv_aic_init": r"third_party[\\/]lvgl-aic[\\/]port[\\/]lv_aic\.o",
    "lv_aic_display_init": r"third_party[\\/]lvgl-aic[\\/]port[\\/]lv_aic_display\.o",
    "lv_aic_indev_init": r"third_party[\\/]lvgl-aic[\\/]port[\\/]lv_aic_indev\.o",
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


def check_config(root, phase="gate1", with_fonts=False, with_gif=False, with_widgets=False, with_player=False):
    config = (root / ".config").read_text(encoding="utf-8")
    header = (root / "rtconfig.h").read_text(encoding="utf-8")
    for symbol in REQUIRED_CONFIG_SYMBOLS:
        if not re.search(r"^CONFIG_%s=y$" % re.escape(symbol), config, re.MULTILINE):
            fail(".config does not enable %s" % symbol)
        if not re.search(r"^#define %s(?:\s|$)" % re.escape(symbol), header, re.MULTILINE):
            fail("rtconfig.h does not define %s" % symbol)
    if '#define AIC_LVGL_TOUCH_DEVICE "gt911"' not in header:
        fail("rtconfig.h does not select AIC_LVGL_TOUCH_DEVICE=gt911")
    for symbol in ("AIC_LVGL_USE_FREETYPE", "LPKG_USING_FREETYPE"):
        enabled = bool(re.search(r"^CONFIG_%s=y$" % symbol, config, re.MULTILINE))
        defined = bool(re.search(r"^#define %s(?:\s|$)" % symbol, header, re.MULTILINE))
        if enabled != with_fonts or defined != with_fonts:
            fail("font profile mismatch: " + symbol)
    if with_fonts and "CONFIG_AIC_LVGL_FREETYPE_GLYPHS=64" not in config:
        fail("font probe profile requires the 64-glyph cache")
    enabled = bool(re.search(r"^CONFIG_AIC_LVGL_USE_GIF=y$", config, re.MULTILINE))
    defined = bool(re.search(r"^#define AIC_LVGL_USE_GIF(?:\s|$)", header, re.MULTILINE))
    if enabled != with_gif or defined != with_gif:
        fail("GIF profile mismatch")
    for symbol in ("AIC_LVGL_USE_IMG_ROLLER", "AIC_LVGL_USE_SWIPE_V1"):
        enabled = bool(re.search(r"^CONFIG_%s=y$" % symbol, config, re.MULTILINE))
        defined = bool(re.search(r"^#define %s(?:\s|$)" % symbol, header, re.MULTILINE))
        if enabled != with_widgets or defined != with_widgets:
            fail("widget profile mismatch: " + symbol)
    for symbol in ("AIC_LVGL_USE_PLAYER", "AIC_LVGL_USE_PLAYER_SESSION",
                   "AIC_MPP_PLAYER_INTERFACE", "AIC_MPP_PLAYER_VIDEO_EXT_RENDER"):
        enabled = bool(re.search(r"^CONFIG_%s=y$" % symbol, config, re.MULTILINE))
        defined = bool(re.search(r"^#define %s(?:\s|$)" % symbol, header, re.MULTILINE))
        if enabled != with_player or defined != with_player:
            fail("player profile mismatch: " + symbol)
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
    if not re.search(r"application[\\/].*third_party[\\/]lvgl[\\/]src", text, re.IGNORECASE):
        fail("LVGL 9.6 upstream src objects are absent from link map")
    if not re.search(r"application[\\/].*third_party[\\/]lvgl-aic[\\/]port", text, re.IGNORECASE):
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
    parser.add_argument("--with-fonts", action="store_true")
    parser.add_argument("--with-gif", action="store_true")
    parser.add_argument("--with-widgets", action="store_true")
    parser.add_argument("--with-player", action="store_true")
    parser.add_argument("--with-apng", action="store_true")
    parser.add_argument("--with-aicp", action="store_true")
    parser.add_argument("--rotation", type=int, choices=(0, 90, 180, 270), default=0)
    parser.add_argument("--allow-component-dirty", action="store_true",
                        help="Development builds only; preserve the component diff with evidence")
    args = parser.parse_args()

    root = args.root.resolve()
    map_path = args.map_path.resolve()

    component = component_root()
    upstream = lvgl_root()
    component_path = component.relative_to(root).as_posix()
    check_submodule(root, upstream.relative_to(root).as_posix(), "80ca777e37a2b176770726a02e07a6fb79ef0b39")
    if not args.allow_component_dirty:
        check_submodule(root, component_path, run_git(root, "rev-parse", ":" + component_path))
    if run_git(upstream, "status", "--porcelain"):
        fail("LVGL submodule has local modifications")
    if not args.allow_component_dirty and run_git(component, "status", "--porcelain"):
        fail("lvgl-aic submodule has local modifications")
    for submodule in (upstream, component):
        if any(submodule.rglob("*.o")):
            fail("object files leaked into submodule source: %s" % submodule)
    if not map_path.is_file():
        fail("link map does not exist: %s" % map_path)
    try:
        map_path.relative_to(root)
    except ValueError:
        fail("link map is outside the integration checkout: %s" % map_path)
    check_config(root, args.phase, args.with_fonts, args.with_gif, args.with_widgets, args.with_player)
    config = (root / ".config").read_text(encoding="utf-8")
    header = (root / "rtconfig.h").read_text(encoding="utf-8")
    for symbol in ("AIC_LVGL_USE_APNG", "AIC_LVGL_USE_APNG_WIDGET"):
        enabled = re.search(r"^CONFIG_" + symbol + r"=y$", config, re.MULTILINE) is not None
        defined = re.search(r"^#define " + symbol + r"(?:\s|$)", header, re.MULTILINE) is not None
        if enabled != args.with_apng or defined != args.with_apng:
            fail("APNG profile mismatch: " + symbol)
    if args.with_apng:
        text = map_path.read_text(encoding="utf-8", errors="replace")
        for symbol in ("lv_aic_apng_create", "lv_aic_apng_set_src", "lv_aic_apng_start",
                       "lv_aic_apng_restart", "lv_aic_apng_set_rate", "lv_aic_apng_close",
                       "lv_aic_apng_playback_prepare", "lv_aic_apng_stream_tick",
                       "lv_aic_apng_decoder_decode", "lv_aic_apng_frames_publish",
                       "lv_aic_apng_compose", "lv_aic_apng_timeline_commit"):
            if not re.search(r"^\s+0x[0-9a-f]+\s+" + symbol + r"\s*$", text, re.MULTILINE):
                fail("APNG live symbol absent: " + symbol)
        print("APNG widget/worker/codec/composition/timing live symbols: PASS")
    turns = args.rotation // 90
    if args.with_aicp:
        if "CONFIG_AIC_MPP_AICP_DEC_ENABLE=y" not in config or not re.search(
                r"^#define AIC_MPP_AICP_DEC_ENABLE(?:\s|$)", header, re.MULTILINE):
            fail("AICP codec profile mismatch")
        text = map_path.read_text(encoding="utf-8", errors="replace")
        if not re.search(r"^\s+0x[0-9a-f]+\s+create_aicp_decoder\s*$", text, re.MULTILINE):
            fail("SDK AICP codec live symbol absent")
        print("SDK AICP codec live symbol: PASS")
    for text, pattern in (
        (config, r"^CONFIG_AIC_LVGL_DISPLAY_ROTATION=%d$" % turns),
        (header, r"^#define AIC_LVGL_DISPLAY_ROTATION\s+%d\s*$" % turns),
    ):
        if not re.search(pattern, text, re.MULTILINE):
            fail("application display rotation profile mismatch")
    check_map(map_path)
    if args.rotation and args.phase == "ge2d":
        text = map_path.read_text(encoding="utf-8", errors="replace")
        if not re.search(r"^\s+0x[0-9a-f]+\s+lv_draw_aic_ge2d_display_rotate\s*$",
                         text, re.MULTILINE):
            fail("GE display rotation live symbol absent")
        print("GE display rotation live symbol: PASS")
    if args.phase in ("mpp", "ge2d"):
        text = map_path.read_text(encoding="utf-8", errors="replace")
        for symbol in ("lv_aic_mpp_decoder_init", "mpp_decoder_decode", "lv_aic_mpp_test_run",
                       "lv_aic_mpp_resource_test_run", "lv_aic_mpp_cache_drop",
                       "lv_aic_mpp_cache_set_limit"):
            if not re.search(r"^\s+0x[0-9a-f]+\s+" + symbol + r"\s*$", text, re.MULTILINE):
                fail("MPP live symbol absent: " + symbol)
    if args.phase == "ge2d":
        # The draw unit must be linked from our own sources, not from the
        # legacy ArtInChip lvgl-ui tree (already rejected in check_map).
        text = map_path.read_text(encoding="utf-8", errors="replace")
        required_ge2d = (
            ("lv_draw_aic_ge2d_init",
             r"third_party[\\/]lvgl-aic[\\/]draw[\\/]ge2d[\\/]lv_draw_aic_ge2d\.o"),
            ("lv_draw_aic_ge2d_fill",
             r"third_party[\\/]lvgl-aic[\\/]draw[\\/]ge2d[\\/]lv_draw_aic_ge2d_fill\.o"),
            # Phase 3B. A separate object on purpose: the IMAGE/LAYER blit must
            # not be folded back into the FILL unit, and this proves the linker
            # took it from the new file rather than from the legacy tree.
            ("lv_draw_aic_ge2d_image",
             r"third_party[\\/]lvgl-aic[\\/]draw[\\/]ge2d[\\/]lv_draw_aic_ge2d_image\.o"),
            ("lv_aic_ge2d_test_run",
             r"third_party[\\/]lvgl-aic[\\/]tests[\\/]manual[\\/]lv_aic_ge2d_test\.o"),
            ("lv_aic_ge2d_fill_test_run",
             r"third_party[\\/]lvgl-aic[\\/]tests[\\/]manual[\\/]lv_aic_ge2d_fill_test\.o"),
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
        for symbol in ("lv_aic_ge2d_scale_axis", "lv_aic_ge2d_scale_test_run"):
            if not re.search(r"^\s+0x[0-9a-f]+\s+" + symbol + r"\s*$", text, re.MULTILINE):
                fail("GE2D scale symbol absent: " + symbol)
        # The engine entry points prove the backend actually talks to the GE
        # driver instead of quietly falling back to software.
        for symbol in ("mpp_ge_open", "mpp_ge_fillrect", "mpp_ge_bitblt",
                       "mpp_ge_emit", "mpp_ge_sync"):
            if not re.search(r"^\s+0x[0-9a-f]+\s+" + symbol + r"\s*$", text, re.MULTILINE):
                fail("GE2D engine symbol absent: " + symbol)
    if args.with_fonts:
        text = map_path.read_text(encoding="utf-8", errors="replace")
        for symbol in ("lv_freetype_font_create", "lv_freetype_font_delete",
                       "lv_aic_font_probe", "lv_aic_font_test_create", "FT_Init_FreeType"):
            if not re.search(r"^\s+0x[0-9a-f]+\s+" + symbol + r"\s*$", text, re.MULTILINE):
                fail("native font live symbol absent: " + symbol)
        print("native font live symbols: PASS")
    if args.with_gif:
        text = map_path.read_text(encoding="utf-8", errors="replace")
        for symbol in ("lv_gif_create", "lv_gif_set_src", "lv_gif_pause",
                       "lv_gif_resume", "lv_aic_gif_test_show", "lv_aic_gif_test_poll",
                       "__fsym_lv_aic_gif"):
            if not re.search(r"^\s+0x[0-9a-f]+\s+" + symbol + r"\s*$", text, re.MULTILINE):
                fail("native GIF live symbol absent: " + symbol)
        print("native GIF live symbols: PASS")
    if args.with_widgets:
        text = map_path.read_text(encoding="utf-8", errors="replace")
        for symbol in ("lv_img_roller_create", "lv_img_roller_ready",
                       "lv_swipe_v1_create", "lv_swipe_v1_set_next"):
            if not re.search(r"^\s+0x[0-9a-f]+\s+" + symbol + r"\s*$", text, re.MULTILINE):
                fail("SDK widget live symbol absent: " + symbol)
        print("SDK widget live symbols: PASS")
    if args.with_player:
        text = map_path.read_text(encoding="utf-8", errors="replace")
        for symbol in ("lv_aic_player_create", "lv_aic_player_set_src", "lv_aic_player_start",
                       "lv_aic_player_seek", "lv_aic_player_playback_seek",
                       "lv_aic_player_set_auto_restart", "lv_aic_player_get_auto_restart_count",
                       "lv_aic_slave_player_create", "lv_aic_slave_player_set_master",
                       "lv_aic_player_playback_prepare", "lv_aic_player_frames_poll_image",
                       "lv_aic_player_allocator_create", "aic_player_create", "aic_player_get_frame"):
            if not re.search(r"^\s+0x[0-9a-f]+\s+" + symbol + r"\s*$", text, re.MULTILINE):
                fail("player live symbol absent: " + symbol)
        print("player link closure: PASS (no media playback execution)")
    print(args.phase + " static checks: PASS (not board validation)")


if __name__ == "__main__":
    main()
