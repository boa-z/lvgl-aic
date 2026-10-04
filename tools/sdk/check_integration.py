#!/usr/bin/env python3
# SPDX-License-Identifier: Apache-2.0
"""Static Gate 1 checks for the Luban-Lite LVGL 9.6 integration."""

import argparse
import hashlib
import json
import struct
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


def check_ge_draw_buffer(root, map_path):
    objdump = root / "toolchain/bin/riscv64-unknown-elf-objdump.exe"
    if not objdump.is_file():
        objdump = objdump.with_suffix("")
    assembly = subprocess.check_output([str(objdump), "-d",
        "--disassemble=lv_draw_layer_alloc_buf", str(map_path.with_suffix(".elf"))], text=True)
    if "<__wrap_lv_draw_buf_create>" not in assembly:
        fail("LVGL layer allocator bypasses application CMA draw-buffer policy")
    print("GE CMA layer allocation call site: PASS")


def check_ge_cmdq(root, map_path):
    """Check generated provenance and the actual linked operations pointer."""
    from stage_ge_cmdq import corrected, SOURCE
    expected = corrected((root / SOURCE).read_text(encoding="utf-8"))
    generated = root / "build/lvgl-ge-cmdq.c"
    evidence = json.loads(generated.with_suffix(".json").read_text(encoding="utf-8"))
    if generated.read_bytes() != expected.encode("utf-8") or evidence.get(
            "normalized_generated_sha256") != hashlib.sha256(generated.read_bytes()).hexdigest():
        fail("GE CMDQ generated source/provenance mismatch")
    nm = root / "toolchain/bin/riscv64-unknown-elf-nm.exe"
    if not nm.is_file():
        nm = nm.with_suffix("")
    elf = map_path.with_suffix(".elf")
    symbols = {}
    for line in subprocess.check_output([str(nm), str(elf)], text=True).splitlines():
        fields = line.split()
        if len(fields) == 3:
            symbols[fields[2]] = int(fields[0], 16)
    required = ("ge_ops_lists", "ge_normal_ops", "__wrap_ge_cmdq_ops")
    if any(name not in symbols for name in required):
        fail("GE CMDQ operations symbols missing")
    data = elf.read_bytes()
    if data[:6] != b"\x7fELF\x01\x01":
        fail("GE CMDQ routing check requires ELF32 little endian")
    phoff = struct.unpack_from("<I", data, 28)[0]
    entsize, count = struct.unpack_from("<HH", data, 42)
    address = symbols["ge_ops_lists"]
    for i in range(count):
        kind, offset, vaddr, _, filesz = struct.unpack_from("<IIIII", data, phoff+i*entsize)
        if kind == 1 and vaddr <= address and address+12 <= vaddr+filesz:
            actual = struct.unpack_from("<III", data, offset+address-vaddr)
            if actual != (symbols["ge_normal_ops"], symbols["__wrap_ge_cmdq_ops"], 0):
                fail("GE CMDQ operations table bypasses corrected backend")
            print("GE CMDQ generated source and linked operations routing: PASS")
            return
    fail("GE CMDQ operations table not found in ELF load segment")


def check_ve_arbitration(root, map_path):
    """Prove final codec call sites use the wrapper, not just a live root."""
    objdump = root / "toolchain/bin/riscv64-unknown-elf-objdump.exe"
    if not objdump.is_file():
        objdump = objdump.with_suffix("")
    elf = map_path.with_suffix(".elf")
    if not objdump.is_file() or not elf.is_file():
        fail("VE arbitration check requires target objdump and linked ELF")
    assembly = subprocess.check_output(
        [str(objdump), "-d", str(elf)], text=True, encoding="utf-8", errors="replace"
    )
    functions = {}
    current = None
    for line in assembly.splitlines():
        header = re.match(r"^[0-9a-f]+ <([^>]+)>:", line)
        if header:
            current = header.group(1)
            functions[current] = []
        elif current:
            functions[current].append(line)
            if "<ve_get_client>" in line and current != "__wrap_ve_get_client":
                fail("VE arbitration bypass in " + current)
    wrapper = "\n".join(functions.get("__wrap_ve_get_client", []))
    if "<ve_get_client>" not in wrapper:
        fail("VE wrapper does not call the real SDK arbitration entry")
    callers = ("png_hardware_decode", "ve_decode_jpeg", "decode_slice")
    present = [name for name in callers if name in functions]
    if not present:
        fail("no linked codec caller for VE arbitration verification")
    for name in present:
        if "<__wrap_ve_get_client>" not in "\n".join(functions[name]):
            fail("codec VE call is not wrapped: " + name)
    print("VE arbitration call sites: PASS (" + ", ".join(present) + ")")


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
    for symbol in ("AIC_LVGL_USE_VIDEO_PLANE", "AIC_LVGL_USE_PLAYER", "AIC_LVGL_USE_PLAYER_SESSION",
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
    parser.add_argument("--with-barcode", action="store_true")
    parser.add_argument("--with-spi", action="store_true")
    parser.add_argument("--with-camera", action="store_true")
    parser.add_argument("--with-demos", action="store_true")
    parser.add_argument("--with-music", action="store_true")
    parser.add_argument("--with-vector", action="store_true")
    parser.add_argument("--with-svg", action="store_true")
    parser.add_argument("--with-lottie", action="store_true")
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
                       "lv_aic_apng_compose", "lv_aic_apng_timeline_commit",
                       "lv_aic_apng_test_show", "lv_aic_apng_test_poll",
                       "lv_aic_apng_slave_create", "lv_aic_apng_slave_set_master",
                       "lv_aic_player_control", "lv_aic_player_get_media_info"):
            if not re.search(r"^\s+0x[0-9a-f]+\s+" + symbol + r"\s*$", text, re.MULTILINE):
                fail("APNG live symbol absent: " + symbol)
        print("APNG widget/worker/codec/composition/timing live symbols: PASS")
    enabled = re.search(r"^CONFIG_AIC_LVGL_USE_BARCODE=y$", config, re.MULTILINE) is not None
    if enabled != args.with_barcode:
        fail("barcode profile mismatch")
    if args.with_barcode:
        if "CONFIG_AIC_USING_BARCODE_DEMO=y" in config:
            fail("SDK barcode demo conflicts with decoder ownership")
        text = map_path.read_text(encoding="utf-8", errors="replace")
        for symbol in ("lv_aic_barcode_decode", "Initial_Decoder", "Decoding_Image",
                       "GetResultLength", "GetDecoderResult", "Set_Donfig_Decoder"):
            if not re.search(r"^\s+0x[0-9a-f]+\s+" + symbol + r"\s*$", text, re.MULTILINE):
                fail("barcode decoder live symbol absent: " + symbol)
        print("SDK barcode archive and worker adapter live symbols: PASS (not decoder execution)")
    for feature in ("AIC_LVGL_USE_CAMERA", "AIC_LVGL_USE_VIN"):
        enabled = re.search(r"^CONFIG_" + feature + r"=y$", config, re.MULTILINE) is not None
        defined = re.search(r"^#define " + feature + r"(?:\s|$)", header, re.MULTILINE) is not None
        if enabled != args.with_camera or defined != args.with_camera:
            fail("Camera profile mismatch: " + feature)
    if args.with_camera:
        for feature in ("AIC_MPP_VIN", "AIC_DVP_DRV", "AIC_USING_CAMERA", "AIC_I2C_DRV"):
            if not re.search(r"^CONFIG_" + feature + r"=y$", config, re.MULTILINE):
                fail("Camera dependency missing: " + feature)
        text = map_path.read_text(encoding="utf-8", errors="replace")
        apis = ("create", "configure", "set_format", "set_channel", "get_channel",
                "get_channel_status", "set_video_plane", "open", "start", "stop",
                "pause", "resume", "close", "get_state", "pending_cleanup",
                "barcode_enable", "barcode_disable", "barcode_only", "barcode_callback",
                "capture_prepare", "capture_start", "capture_poll", "capture_destroy")
        for symbol in tuple("lv_aic_camera_" + api for api in apis) + ("mpp_vin2_init", "mpp_vin2_deinit", "mpp_vin2_vb_init", "mpp_vin2_vb_deinit"):
            if not re.search(r"^\s+0x[0-9a-f]+\s+" + symbol + r"\s*$", text, re.MULTILINE):
                fail("Camera live symbol absent: " + symbol)
        print("Camera widget/worker/VIN final link: PASS (no device execution)")
    for feature in ("AIC_LVGL_BUILD_DEMO_WIDGETS", "AIC_LVGL_BUILD_DEMO_BENCHMARK"):
        enabled = re.search(r"^CONFIG_" + feature + r"=y$", config, re.MULTILINE) is not None
        defined = re.search(r"^#define " + feature + r"(?:\s|$)", header, re.MULTILINE) is not None
        if enabled != args.with_demos or defined != args.with_demos:
            fail("Demo profile mismatch: " + feature)
    if args.with_demos:
        text = map_path.read_text(encoding="utf-8", errors="replace")
        for symbol in ("lv_demo_widgets", "lv_demo_widgets_with_args", "lv_demo_benchmark",
                       "lv_demo_benchmark_set_end_cb", "lv_demo_benchmark_summary_display"):
            if not re.search(r"^\s+0x[0-9a-f]+\s+" + symbol + r"\s*$", text, re.MULTILINE):
                fail("Demo live symbol absent: " + symbol)
        print("Upstream widgets/benchmark final link: PASS (not board execution)")
    feature = "AIC_LVGL_BUILD_DEMO_MUSIC"
    enabled = re.search(r"^CONFIG_" + feature + r"=y$", config, re.MULTILINE) is not None
    defined = re.search(r"^#define " + feature + r"(?:\s|$)", header, re.MULTILINE) is not None
    if enabled != args.with_music or defined != args.with_music:
        fail("Music demo profile mismatch")
    if args.with_music:
        text = map_path.read_text(encoding="utf-8", errors="replace")
        for symbol in ("lv_demo_music", "lv_demo_music_with_args", "lv_demo_music_play",
                       "lv_demo_music_pause", "lv_demo_music_resume", "lv_demo_music_album_next"):
            if not re.search(r"^\s+0x[0-9a-f]+\s+" + symbol + r"\s*$", text, re.MULTILINE):
                fail("Music demo live symbol absent: " + symbol)
        print("Upstream music UI final link: PASS (not audio playback)")
    feature = "AIC_LVGL_USE_VECTOR"
    enabled = re.search(r"^CONFIG_" + feature + r"=y$", config, re.MULTILINE) is not None
    defined = re.search(r"^#define " + feature + r"(?:\s|$)", header, re.MULTILINE) is not None
    if enabled != args.with_vector or defined != args.with_vector:
        fail("Vector profile mismatch")
    if args.with_vector:
        if not re.search(r"^CONFIG_RT_USING_CPLUSPLUS=y$", config, re.MULTILINE):
            fail("Vector target requires the SDK C++ runtime")
        text = map_path.read_text(encoding="utf-8", errors="replace")
        for symbol in ("lvgl_aic_thorvg_config_probe", "cplusplus_system_init", "lv_draw_vector", "lv_draw_sw_vector", "lv_vector_path_create",
                       "lv_draw_vector_dsc_create", "tvg_swcanvas_create", "tvg_canvas_draw"):
            if not re.search(r"^\s+0x[0-9a-f]+\s+" + symbol + r"\s*$", text, re.MULTILINE):
                fail("Vector live symbol absent: " + symbol)
        print("Native vector/ThorVG software final link: PASS (not board execution)")
    enabled = re.search(r"^CONFIG_AIC_LVGL_USE_SVG=y$", config, re.MULTILINE) is not None
    if enabled != args.with_svg:
        fail("SVG profile mismatch")
    if args.with_svg:
        if not args.with_vector:
            fail("SVG requires vector rendering")
        text = map_path.read_text(encoding="utf-8", errors="replace")
        for symbol in ("lv_svg_decoder_init", "lv_svg_load_data", "lv_svg_render_create", "lv_draw_svg_render"):
            if not re.search(r"^\s+0x[0-9a-f]+\s+" + symbol + r"\s*$", text, re.MULTILINE):
                fail("SVG live symbol absent: " + symbol)
        print("Native SVG decoder/parser/render final link: PASS (not board execution)")
    enabled = re.search(r"^CONFIG_AIC_LVGL_USE_LOTTIE=y$", config, re.MULTILINE) is not None
    defined = re.search(r"^#define AIC_LVGL_USE_LOTTIE(?:\s|$)", header, re.MULTILINE) is not None
    if enabled != args.with_lottie or defined != args.with_lottie:
        fail("Lottie profile mismatch")
    if args.with_lottie:
        if not args.with_vector:
            fail("Lottie requires vector rendering")
        text = map_path.read_text(encoding="utf-8", errors="replace")
        for symbol in ("lv_lottie_create", "lv_lottie_set_buffer", "lv_lottie_set_draw_buf",
                       "lv_lottie_set_src_data", "lv_lottie_set_src_file", "lv_lottie_get_anim",
                       "tvg_animation_new", "tvg_animation_set_frame"):
            if not re.search(r"^\s+0x[0-9a-f]+\s+" + symbol + r"\s*$", text, re.MULTILINE):
                fail("Lottie live symbol absent: " + symbol)
        from stage_lottie import corrected
        expected = corrected((lvgl_root() / "src/libs/thorvg/tvgLottieBuilder.cpp").read_text(encoding="utf-8"))
        if (root / "build/lvgl-lottie-builder.cpp").read_bytes() != expected.encode("utf-8"):
            fail("Lottie generated opacity correction mismatch")
        # GCC can inline updateSolid into an internal updateLayer.part clone;
        # inspect a nonzero live section, never the discarded zero-address copy.
        sections = re.findall(r"^ \.text\._ZN13LottieBuilder11updateLayer[^\n]*\n"
                              r"\s+0x([0-9a-f]+)\s+0x[0-9a-f]+\s+[^\n]*lvgl-lottie-builder\.o\s*$",
                              text, re.MULTILINE)
        if not any(int(address, 16) for address in sections):
            fail("Lottie layer builder does not resolve to corrected object")
        lines = text.splitlines()
        live = [i for i, line in enumerate(lines) if re.search(r"^\s+0x[0-9a-f]+\s+lv_lottie_set_draw_buf\s*$", line)]
        if not live or not any("lv_aic_lottie.o" in "\n".join(lines[max(0, i-4):i+1]) for i in live):
            fail("Lottie canvas format wrapper is not linked")
        print("Native Lottie widget/loader and corrected solid opacity final link: PASS (not board execution)")
    spi_enabled = re.search(r"^CONFIG_AIC_LVGL_USE_SPI_SDK=y$", config, re.MULTILINE) is not None
    if spi_enabled != args.with_spi:
        fail("SPI profile mismatch")
    if args.with_spi:
        if not re.search(r"^#define AIC_LVGL_USE_SPI_SDK(?:\s|$)", header, re.MULTILINE):
            fail("SPI target header mismatch")
        text = map_path.read_text(encoding="utf-8", errors="replace")
        for symbol in ('lv_aic_spi_display_create_pipelined', 'lv_aic_spi_display_poll', 'lv_aic_spi_pipeline_create', 'lv_aic_spi_pipeline_submit', 'lv_aic_spi_pipeline_take', 'lv_aic_spi_pipeline_stop', 'lv_aic_spi_pipeline_close', 'lv_aic_spi_session_submit_ex', 'lv_aic_spi_display_stats', 'lv_aic_spi_display_claim_blit', 'lv_aic_spi_display_blit', 'lv_aic_spi_display_blit_take', 'lv_aic_spi_display_create_buffered', 'lv_aic_spi_display_create', 'lv_aic_spi_display_get', 'lv_aic_spi_display_result', 'lv_aic_spi_display_close', 'lv_aic_spi_panel_set_lifecycle', 'lv_aic_spi_panel_create', 'lv_aic_spi_panel_prepare', 'lv_aic_spi_panel_close', 'lv_aic_spi_session_enable_ge2d', 'lv_aic_spi_session_enable_overlap', 'lv_aic_spi_session_open_owned', 'lv_aic_spi_session_close', 'lv_aic_spi_worker_create', 'lv_aic_spi_handoff_run', 'lv_aic_spi_sdk_write_qspi', 'lv_aic_spi_sdk_submit_qspi', 'lv_aic_spi_sdk_wait_complete', 'rt_qspi_transfer_message', 'rt_spi_wait_completion', 'rt_spi_nonblock_set', 'rt_spi_get_transfer_status'):
            if not re.search(r"^\s+0x[0-9a-f]+\s+" + symbol + r"\s*$", text, re.MULTILINE):
                fail("SPI live symbol absent: " + symbol)
        if args.phase == "ge2d":
            for symbol in ("lv_aic_spi_ge2d_create", "lv_aic_spi_ge2d_convert", "lv_aic_spi_ge2d_close"):
                if not re.search(r"^\s+0x[0-9a-f]+\s+" + symbol + r"\s*$", text, re.MULTILINE):
                    fail("SPI GE conversion live symbol absent: " + symbol)
        print("SPI display/worker/panel/session/SDK final link: PASS (no device execution)")
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
    if args.phase == "ge2d":
        check_ge_draw_buffer(root, map_path)
    if args.phase == "ge2d" and "CONFIG_AIC_GE_CMDQ=y" in config:
        check_ge_cmdq(root, map_path)
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
        for symbol in ("lv_ge_fill", "lv_mpp_image_alloc", "lv_mpp_image_flush_cache", "lv_mpp_image_free",
                       "lv_aic_mpp_image_alloc_bounded", "lv_aic_canvas_create", "lv_aic_canvas_alloc_buffer",
                       "lv_aic_canvas_draw_text", "lv_aic_canvas_draw_text_to_center",
                       "lv_img_roller_create", "lv_img_roller_ready",
                       "lv_swipe_v1_create", "lv_swipe_v1_set_next",
                       "lv_list_create", "lv_list_add_button", "lv_list_get_button_text",
                       "lv_menu_create", "lv_menu_page_create", "lv_menu_cont_create",
                       "lv_menu_set_load_page_event", "lv_menu_set_page", "lv_menu_get_cur_main_page"):
            if not re.search(r"^\s+0x[0-9a-f]+\s+" + symbol + r"\s*$", text, re.MULTILINE):
                fail("SDK widget live symbol absent: " + symbol)
        print("SDK widget live symbols: PASS")
    if args.with_player:
        text = map_path.read_text(encoding="utf-8", errors="replace")
        for symbol in ("lv_aic_video_plane_present_rotated", "lv_aic_video_plane_enable_ui_alpha", "lv_aic_video_plane_open", "lv_aic_video_plane_present",
                       "lv_aic_video_plane_hide", "lv_aic_video_plane_close", "lv_aic_video_plane_faulted",
                       "lv_aic_player_set_video_plane", "lv_aic_player_set_video_plane_rotation_budget", "lv_aic_player_create", "lv_aic_player_set_src", "lv_aic_player_start",
                       "lv_aic_player_seek", "lv_aic_player_playback_seek",
                       "lv_aic_player_set_rate", "lv_aic_player_get_rate",
                       "lv_aic_player_group_create", "lv_aic_player_group_add",
                       "lv_aic_player_group_remove", "lv_aic_player_group_get_count",
                       "lv_aic_player_group_control", "lv_aic_player_set_group", "lv_aic_player_get_group",
                       "lv_aic_player_playback_preserve", "lv_aic_player_frames_submit_checked",
                       "lv_aic_media_runtime_acquire", "lv_aic_media_runtime_release",
                       "lv_aic_player_set_auto_restart", "lv_aic_player_get_auto_restart_count",
                       "lv_aic_slave_player_create", "lv_aic_slave_player_set_master",
                       "lv_aic_player_playback_prepare", "lv_aic_player_frames_poll_image",
                       "lv_aic_player_allocator_create", "aic_player_create", "aic_player_get_frame"):
            if not re.search(r"^\s+0x[0-9a-f]+\s+" + symbol + r"\s*$", text, re.MULTILINE):
                fail("player live symbol absent: " + symbol)
        for api in ('set_width', 'set_height', 'set_pivot', 'get_pivot', 'set_rotation', 'get_rotation', 'set_scale', 'get_scale', 'set_scale_x', 'get_scale_x', 'set_scale_y', 'get_scale_y', 'set_offset_x', 'get_offset_x', 'set_offset_y', 'get_offset_y', 'set_inner_align', 'get_inner_align'):
            symbol = "lv_aic_player_" + api
            if not re.search(r"^\s+0x[0-9a-f]+\s+" + symbol + r"\s*$", text, re.MULTILINE):
                fail("player transform/size live symbol absent: " + symbol)
        print("Player transform and deferred-size live symbols: PASS")
        if args.with_apng:
            for symbol in ("lv_aic_player_configure_apng", "lv_aic_apng_playback_preserve",
                           "lv_aic_plane_test_poll", "lv_aic_plane_test_deinit",
                           "__fsym_lv_aic_plane_test"):
                if not re.search(r"^\s+0x[0-9a-f]+\s+" + symbol + r"\s*$", text, re.MULTILINE):
                    fail("unified player APNG symbol absent: " + symbol)
        print("player link closure: PASS (no media playback execution)")
    if args.with_player or args.with_apng:
        check_ve_arbitration(root, map_path)
    print(args.phase + " static checks: PASS (not board validation)")


if __name__ == "__main__":
    main()
