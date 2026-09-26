# Validation record

## Status

This file is intentionally explicit about unverified work.

| Date | Commit | SDK commit | LVGL commit | Board | Result | Notes |
|---|---|---|---|---|---|---|
| 2026-09-24 | `531cb8138b0ae60445814ba671a26c2607a7f3fb` | `c5807f9e7d18292f920dafaa018b8174635085c4` | `80ca777e37a2b176770726a02e07a6fb79ef0b39` | none | partial | External LVGL 9.6 host configure/build and CTest smoke test PASS; real D13x target build and board validation pending |
| 2026-09-24 | `e59ca1f1f750e445741acdeb5297cd70052b8616` | `c5807f9e7d18292f920dafaa018b8174635085c4` | `80ca777e37a2b176770726a02e07a6fb79ef0b39` | none | partial | Optional `lvgl-aic-sdl-smoke` SDL2 host target and mouse self-test PASS; this is not hardware evidence |
| 2026-09-24 | `cb1691519ccb7aa377a2f43938b1a423dc5ab837` | `c5807f9e7d18292f920dafaa018b8174635085c4` | `80ca777e37a2b176770726a02e07a6fb79ef0b39` | none | partial | Official LVGL 9.6 widgets/benchmark/stress/music/keypad demos added and passed in the 800x480 SDL host; no hardware claim |
| 2026-09-24 | `d1492bf7377b056c66656e166847f4d81b2ec7b4` | `c5807f9e7d18292f920dafaa018b8174635085c4` | `80ca777e37a2b176770726a02e07a6fb79ef0b39` | D133ECS / D50T-2-Lite | baseline PASS (provisional) | User-flashed image `05DDBA327C6026E50C23445B48EDE29EBAE3BD0EF55D4DCDB29F8670A3690EE1`; semaphore/lifecycle/first-frame logs observed; 800x480 software-rendered page and GT911 manual touch confirmed. RGB mirror was explicitly disabled. Raw-coordinate, VSync/PAN counter, cache-stress, rotation-variant, and long-run evidence is deferred to the next phase. |

## Required Phase 1 evidence

- [x] LVGL 9.6 compile
- [x] LVGL 9.6 link
- [x] framebuffer display — user-confirmed 800x480 baseline
- [x] 800x480 software-rendered surface — user-confirmed baseline
- [x] touch read callback — user-confirmed GT911 manual interaction
- [x] continuous refresh — moving marker remained live
- [x] VSync/PAN behavior — port path exercised; formal counters deferred
- [x] rotation behavior or documented limitation — 0° baseline; angle variants deferred
- [x] cache coherency check — moving-marker baseline; stress measurement deferred
- [x] no legacy `lvgl-ui` dependency
- [x] no LVGL upstream modification

The closeout treats the user-confirmed board smoke as the Phase 1 baseline.
Items marked as deferred are not claimed as independent measurements; they are
intentionally carried into the next phase.

## Host build evidence

The following checks were run against an external temporary checkout of the
pinned LVGL source; no LVGL source is stored in this repository. The
reusable command is documented in [`tests/host/README.md`](../tests/host/README.md):

- LVGL 9.6.0 CMake configure/build: PASS (external checkout, 2026-09-24);
- `lvgl_aic` public API and Phase 1 port sources: PASS;
- platform-only manual smoke page: PASS;
- CTest `lvgl_aic_platform_smoke`: PASS (1/1);
- optional `lvgl-aic-sdl-smoke` SDL2 build: PASS (MSYS2 UCRT64, SDL2 2.32.10);
- SDL2 CTest `lvgl_aic_sdl_smoke_self`: PASS (2/2 total with the headless smoke);
- SDL2 screenshot and injected mouse click verified the shared 800x480 manual page;
- official LVGL 9.6 demo modes `widgets`, `benchmark`, `stress`, `music`, and
  `keypad_encoder`: PASS; bounded CTest coverage is 7/7 with
  `AIC_BUILD_SDL_DEMOS=ON`;
- 800x480 screenshots captured the upstream widgets, benchmark, stress, music,
  and keypad/encoder layouts; vector/GLTF and legacy ArtInChip demos remain
  intentionally excluded;
- host component targets use `-Wall -Wextra -Werror` on GCC/Clang;
- `.github/workflows/host.yml` checks out the pinned LVGL commit and runs the
  same host smoke test;
- compile-time rejection of the repository's LVGL 9.1 header: PASS;
- AIC BSP target compile: PASS with the real D13x toolchain and board smoke
  described below; host stubs are not counted as target evidence.

The full LVGL upstream build emits existing MSVC code-page and enum warnings;
those warnings are not attributed to `lvgl-aic`. The component sources were
additionally checked with `-Wall -Wextra -Werror` using the target-facing
stubs.

## Phase 1.5 target build evidence

The real target build was run in the isolated Luban-Lite integration checkout,
not in the parent working tree with its unrelated uncommitted changes:

- SDK baseline: `c5807f9e7d18292f920dafaa018b8174635085c4`;
- LVGL: `80ca777e37a2b176770726a02e07a6fb79ef0b39` (`v9.6.0`);
- component: `d1492bf7377b056c66656e166847f4d81b2ec7b4`;
- application defconfig:
  `d13x_d50t-2-lite_rt-thread_lvgl-aic-smoke_defconfig`;
- bootloader prerequisite defconfig:
  `d13x_d50t-2-lite_baremetal_bootloader_defconfig`;
- toolchain: Xuantie-900 GCC V2.6.1 B-20220906, GCC 10.2.0;
- bootloader `d13x.elf`: PASS;
- application `d13x.elf`: PASS, 9,030,912 bytes;
- application image generation: PASS;
- image: `output/d13x_d50t-2-lite_rt-thread_lvgl-aic-smoke/images/d13x_D50T-2-Lite_page_2k_block_128k_v1.0.0.img`, 1,505,792 bytes;
- image SHA256: `05DDBA327C6026E50C23445B48EDE29EBAE3BD0EF55D4DCDB29F8670A3690EE1`;
- RGB panel alignment: both D50T smoke and bootloader defconfigs explicitly set
  `# CONFIG_RGB_DATA_MIRROT is not set` and `CONFIG_AIC_RGB_DATA_MIRROR=0`;
- static link-map check: PASS (`lv_init`, `lv_display_create`, and
  `lv_obj_create` resolve to `packages/third-party/lvgl/src`; no
  `packages/artinchip/lvgl-ui` or `lvgl_v9/lvgl` object appears);
- compile-time `lv_conf.h` marker probe: PASS;
- Kconfig touch symbol: generated as
  `#define AIC_LVGL_TOUCH_DEVICE "gt911"`;
- forbidden Phase 1 symbols: `AIC_LVGL_USE_GE2D`,
  `AIC_LVGL_USE_MPP_DEC`, and `AIC_LVGL_USE_FT_CACHE` remain disabled.

The target build enables `LPKG_MPP` only for framebuffer/BSP infrastructure;
it does not add an LVGL MPP image decoder or GE2D implementation. The build
uses the platform-only smoke application, not the D50T product UI.

The only compiler diagnostics observed in the LVGL OSAL are pre-existing
`%d`/`rt_err_t` format warnings from upstream `lv_rtthread.c`; the build is
not warning-clean because the pinned upstream source is intentionally not
modified.

## Hardware closeout (2026-09-24)

A D133ECS / D50T-2-Lite board was flashed with the mirror-disabled image:

```text
output/d13x_d50t-2-lite_rt-thread_lvgl-aic-smoke/images/d13x_D50T-2-Lite_page_2k_block_128k_v1.0.0.img
SHA256: 05DDBA327C6026E50C23445B48EDE29EBAE3BD0EF55D4DCDB29F8670A3690EE1
```

The board produced the semaphore vlimit self-test pass, three lifecycle passes,
and the first software-rendered frame log. The user additionally confirmed the
800x480 smoke page, corrected colors, live marker animation, and GT911 manual
button interaction. The `/sdcard` mount warning is unrelated to this display
and touch path.

This closes the current smoke baseline provisionally. The following independent
measurements were not performed in this closeout and are deferred to the next
phase rather than claimed as PASS:

- raw and scaled DOWN/MOVE/UP coordinate records;
- four-corner/direct-fill color-block evidence;
- PAN/VSync counters or logic-analyzer timing;
- cache-stress animation and framebuffer coherency evidence;
- 90/180/270-degree rotation variants;
- 30–60 minute long-run, heap/PSRAM/thread stability measurements.

## Hardware policy

For this closeout, report:

```text
Build validation: PASS
Hardware baseline: PASS (user-confirmed, provisional)
Extended diagnostics: DEFERRED
```

Do not convert the deferred items into independent hardware claims.

## Gate 1 closeout revisions (Phase 2 entry)

- `lvgl-aic` Gate 1 code: `d1492bf7377b056c66656e166847f4d81b2ec7b4`
- `lvgl-aic` Gate 1 closeout docs: `f90f5e067e8606ce82c7a542eb566827d510a8b9`
- `lvgl-aic` tag: `v0.1.0` on `f90f5e0` (pushed to `boa-z/lvgl-aic` on
  2026-09-24 with deploy token; `f90f5e0` == `origin/main` at tag time)
- parent/superproject: `f7572509111d1c70962e6347e5ad7bc87b77fbba`
  (`codex/d50t-meter-adaptation`, pushed to `boa-w/luban-lite-jc-d50t-rev`
  on 2026-09-24)
- LVGL: `80ca777e37a2b176770726a02e07a6fb79ef0b39` (`v9.6.0`)
- Luban-Lite SDK baseline: `c5807f9e7d18292f920dafaa018b8174635085c4`
- verified image SHA256:
  `05DDBA327C6026E50C23445B48EDE29EBAE3BD0EF55D4DCDB29F8670A3690EE1`

Phase 2 work starts from `phase2-mpp` branched at `v0.1.0`. Gate 1 SW
baseline (`GE2D=OFF`, `MPP_DEC=OFF`, `FT_CACHE=OFF`) must remain intact
until Phase 2 gates replace it.

## Phase 2A status (code complete, board validation pending)

Implemented on `phase2-mpp` after `v0.1.0`:

- `feat(image): establish AIC MPP decoder boundary`
- `feat(image): add MPP JPEG decoding for LVGL 9.6`
- `feat(image): add MPP PNG and alpha decoding`
- `fix(image): harden MPP buffer ownership and failure cleanup`
- `test(image): add MPP decoder lifecycle and board tests`

Design (enforced by construction, not just review):

- `GE2D` remains `OFF`; `image/mpp` includes no GE2D header and the MPP
  link map contains no `ge2d` object.
- Format mapping lives in `lv_aic_mpp_format.*` (`RGB565/RGB888/ARGB8888`
  only; YUV/BGR variants rejected instead of mislabeled).
- Decoder owns one `lv_image_decoder_t` via `lv_image_decoder_create`;
  `lv_aic_init` order is display -> input -> decoder with reverse teardown.
- `FILE` (`.jpg/.jpeg/.png`) only; `VARIABLE/AICP/BMP/fake` return
  `LV_RESULT_INVALID`.
- Buffers use `lv_draw_buf_init()` (valid `data/unaligned_data/handlers/
  stride/data_size`); CMA `allocation_base` is freed from the base pointer,
  and PNG post-process heap replacements are tracked as `heap_buf`.
- No custom image cache (`lv_drop_one_cached_image()` returns false).
- Decoder private structs come only via `compat/lvgl_aic_private.h`.

Verification so far:

- Host `lvgl_aic_platform_smoke`: PASS (MPP stubs, SW baseline unregressed).
- Target `d13x_d50t-2-lite_rt-thread_lvgl-aic-smoke` with `MPP_DEC=0`: PASS.
- Target same config with temporary `MPP_DEC=1`: PASS (link map shows
  `lv_aic_mpp_{decoder,format,stream}` objects, zero `ge2d` hits).
- Phase 2 validation config keeps `AIC_LVGL_USE_GE2D=n`.

Still required on hardware before any Gate 2 claim:

- Flash an `MPP_DEC=1` image and load `/data/mpp_test/{a.jpg,b.png,c.png}`.
- Confirm JPEG RGB, PNG RGB, and PNG RGBA render correctly under the SW
  renderer (color, alpha, stride, repeated refresh).
- Run corrupt/missing/zero-byte/unsupported-format cases (safe `INVALID`).
- Run A/B/C multi-image plus >= 1000 decode/release cycles and
  `lv_aic_init/deinit` repeats with heap/CMA sampled.
- Record `decode_time_ms`/buffer sizes from `lv_aic_mpp_decoder_last_stats()`.
- Re-confirm the Gate 1 display/touch baseline did not regress.

## Phase 2A review and user board log (2026-09-26)

The user supplied a D50T-2-Lite log reporting semaphore self-test, three
lifecycle cycles, all listed acceptance cases, 1000 decode/close cycles and
first software frame presented. No image SHA256 was supplied; this evidence
cannot be bound to the newly reviewed build. RT heap before=119708,
after=115396, peak=144900 is not proof of no CMA leak. The odd-width JPEG
reports 801x479, stride 2448, CMA 1175040 bytes. Visual color/alpha correctness
and touch regression still need confirmation. The /sdcard mount failure is
separate from the /data fixture filesystem.

Review retained PNG chunk CRC checks and expected rejection of 4-bit palette
files: the SDK accepts only 8-bit PNG input. No vendor decoder changes were
made. Fixed a CRC walker bounds error (chunk framing needs 12 bytes, not 8),
validated packet signature and empty IEND, and added every truncated prefix
of a valid PNG to the production-code host regression test. CRC validity is
separate from decoder format support. Adler-32 is not checked; the SDK still
swallows some PNG hardware errors, so arbitrary corrupt-stream rejection is
not established by these fixtures.

The external allocator uses SDK-provided byte stride and padded height,
preserves frame metadata, and places its allocator interface first. Disabled
feature guards use numeric values; RT-Thread empty Kconfig defines are
normalized. Removed unused session fields, consolidated fixture preconditions
and shortened stress logs to avoid ULOG truncation.

Build/evidence tools live in this repository's `tools/sdk/`. The component
SConscript tracks staged fixtures so asset changes trigger link/packing.
See `tools/sdk/README.md`. Fresh build manifests mark board validation
NOT_RUN. Gate 2 remains open pending image-bound visual, touch and CMA evidence.

Review verification: 9/9 host CTests and 22 fixture header decisions plus
PNG CRC checks passed. The Windows MPP build passed static integration,
ELF ABI, 9 firmware payload CRCs and 26 packaged fixture/provenance hashes.
An asset-only README change triggered relink/repack without recompiling C.
Detailed logs and source snapshots are in SDK build/lvgl-evidence/.
