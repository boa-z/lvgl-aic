# Native GIF stage

The SDK LVGL 9.1 configuration enables LV_USE_GIF. The application component
now exposes AIC_LVGL_USE_GIF (default off), mapped to the pinned, unmodified
LVGL 9.6 widget. GIF decoding uses the upstream software decoder; it does not
use the MPP JPEG/PNG decoder or import the legacy SDK widget/cache hooks.

## Ownership and limits

Enable CONFIG_AIC_LVGL_USE_GIF=y in the application's own configuration.
Use lv_gif_create and lv_gif_set_src with an LVGL filesystem path such as
L:/data/mpp_test/bulb.gif, or a borrowed lv_image_dsc_t with RAW GIF bytes.
A file remains open during playback. Keep the source path string, descriptor
and memory bytes alive until the widget is deleted or replaced successfully.
Delete the widget before tearing down its display, filesystem or LVGL.
The application must serialize all these operations with its LVGL loop.

The default decoded canvas is ARGB8888; set the color format before the source
when RGB565 is sufficient. Budget decoder state plus the decoded canvas and
source storage; this stage does not impose an application-wide memory budget.
Do not equate successful repeated lifecycle tests with heap leak profiling.
The board probe uses a trusted fixture, not arbitrary untrusted GIF uploads.

Check lv_gif_is_loaded immediately after setting the source. On failure, delete
the widget before the next timer-handler call. For closing or recovery use
delete/recreate. Null-source shutdown and failed replacement of an already
playing source are not accepted paths in this stage. No upstream patch is made.
Avoid lv_gif_get_size on a small embedded stack: its upstream implementation
allocates the decoder state locally. The board probe opens the heap-owned widget
instead and retains the smoke application's 32 KiB thread stack.

## Host gate

Configure tests/host with AIC_BUILD_GIF_TESTS=ON. The test receives a native host
path; its G: LVGL filesystem driver maps only /bulb.gif to that fixture. It
checks FILE and RAW input, RGB565/ARGB8888 canvas formats, frame progression,
changing rendered pixels, frozen frame/pixels while paused, resume/restart,
20 create/replace/delete cycles, invalid inputs and balanced file handles.
Another 20 panel cycles cover duplicate-open rejection and top-layer cleanup.
The bulb fixture stays in the pinned LVGL checkout; CMake copies it into the
build directory. AIC_SDK_ROOT and AIC_BUILD_FREETYPE_TESTS=ON enable the combined
10-test suite with the existing GE2D/MPP/FreeType contracts.

The navigation test also now updates the top-layer layout before coordinate
injection, has no direct-event fallback for failed clicks, checks that the Fonts
overlay blocks background navigation, and verifies top-layer teardown without
FreeType. Hardware appearance and touch acceptance remain separate evidence.

## Isolated board candidate

From the component directory inside the dedicated SDK worktree:

    powershell -NoProfile -ExecutionPolicy Bypass -File tools/sdk/build.ps1 -Phase ge2d -WithFonts -WithGif -AllowComponentDirty -Jobs 8

-WithGif supports mpp/ge2d resource profiles, with or without -WithFonts.
The script restores the exact smoke defconfig bytes after building. It stages
bulb.gif and the upstream LICENCE.txt with a SHA256 inventory; image verification
checks the actual packaged FAT files. No SDK core or LVGL source modification
is needed. The resulting evidence directory is output/lvgl-evidence/ge2d-fonts-gif
for this combined profile. Do not build another SDK profile in this worktree
concurrently. The script builds and checks images; it does not flash the board.

After flashing the candidate and waiting for the smoke UI:

    lv_aic_gif show
    lv_aic_gif status
    lv_aic_gif pause
    lv_aic_gif status
    lv_aic_gif resume
    lv_aic_gif restart
    lv_aic_gif close

Wait for each command's result before issuing another. The shell publishes one
bounded request; the UI timer performs all widget operations. Show returns
result=0 and opens an opaque modal panel. A bulb should animate without residue;
its Close button returns to the unchanged page. Two status samples while paused
should show the same frame; after resuming, sample over multiple frame periods
to observe advancement. Repeat show/close at least 20 times, verify navigation
and Fonts after closing, and leave playback running for five minutes. Record
full logs, visible corruption, crashes and latency. Busy/not-ready requests must
be retried; queued means accepted for processing, not completed.

## Candidate evidence (2026-10-01)

Combined GIF + FreeType + SDK ABI host suite: 10/10 PASS. Features-off baseline:
8/8 PASS. Both exercise actual navigation coordinates without event bypasses.
GE2D + FreeType + GIF firmware build, live-symbol gate and image checks PASS;
the image contains 32 fixture/provenance files with verified hashes and nine
verified payload CRCs. Image SHA256:

    c816ada135d18699a14454691be63e845d1d695222e8d05ebf1ce14aba0279b7

The existing three-page interface was accepted by the operator before this
stage. This new GIF image still requires board acceptance; no board is flashed
by these checks and no GIF performance/cache-coherency result is claimed.
