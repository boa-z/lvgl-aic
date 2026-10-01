# Application-owned Luban-Lite integration

Updated 2026-09-30. Earlier packages/custom layouts and kernel semaphore
backports in historical records are superseded.

## Layout and pins

The selected application owns Kconfig, SConscript, its entry point, and sibling
third_party/lvgl and third_party/lvgl-aic submodules. The smoke consumer is
application/rt-thread/lvgl-aic-smoke. Pin LVGL to
80ca777e37a2b176770726a02e07a6fb79ef0b39 and this component to a reviewed gitlink.
Do not copy upstream sources or use symlinks. Dependency upgrades are explicit.

## Kconfig

Source the component Kconfig from the selected application's Kconfig using its
SDK-relative path. Disable LPKG_USING_LVGL. Enable KERNEL_RTTHREAD, LPKG_MPP,
RT_USING_EVENT, AIC_LVGL_PORT and the required display/touch options. Set
AIC_LVGL_TOUCH_DEVICE to the board device (gt911 for the smoke profiles).
Board profiles must also select the actual framebuffer/touch/GE/decoder devices.
MPP decoding and GE2D are independent opt-ins, off by default. Smoke pages
require AIC_LVGL_MANUAL_TEST. Encoder and mouse are optional application-owned
providers: register lv_aic_input_provider_t callbacks before lv_aic_init() and
use the returned native indevs. AIC FreeType cache symbols remain a separate
gap. Native FreeType is separate: AIC_LVGL_USE_FREETYPE selects
the independent LPKG_USING_FREETYPE library; AIC_LVGL_FREETYPE_GLYPHS defaults to
64. No vendor LVGL/font adapter is linked. The SDK emits symbols without CONFIG_
in rtconfig.h. See [font integration](font-stage.md).

## SCons and configuration ownership

The application invokes SConscript('third_party/lvgl-aic/SConscript'). This
component collects its sibling LVGL src C files and its own port sources;
do not collect the core a second time. Upstream RT-Thread entry points,
examples/demos and the unrelated VG Lite driver are excluded.

compat/lvgl_aic_build_config.h selects the reviewed lv_conf.h through explicit
wrappers. LV_KCONFIG_IGNORE and the external bridge prevent legacy configuration
from becoming a second source of truth. Current SDK SCons flags are global
within the selected image: application directory ownership does not mean
compiler-flag isolation. The image must not include the legacy LVGL package.

Do not modify SDK packages/, kernel/, global Kconfig or upstream LVGL.
Board profiles remain under target/configs because the SDK loader requires them.
The consuming application owns thread/loop parameters in its own Kconfig.

## RT-Thread adaptation

Target builds use LV_OS_CUSTOM and compat/lv_aic_rtthread_os.{c,h}. An RT event
bit implements binary notifications: repeated signals coalesce and receive
clears the bit atomically. RT_USING_EVENT is mandatory. Do not backport
RT_IPC_CMD_SET_VLIMIT or change SDK semaphore layout. The upstream RT-Thread
backend is not the active adapter.

Host OS contracts use a fake RT API. Actual scheduling and interrupt wakeups
require board tests after integration changes.

## Build and evidence

Use a dedicated SDK worktree. Configuration, generated headers, bootloader
staging and output change in that checkout; never run concurrent SDK profile
builds there or build in the active product checkout.

From the SDK root, invoke
application/rt-thread/lvgl-aic-smoke/third_party/lvgl-aic/tools/sdk/build.ps1
with -Phase gate1, mpp or ge2d and -Jobs 8. Development builds use
-AllowComponentDirty to permit a working tree or component HEAD differing from
the parent index pin; source state is archived. Do not promote unpublished
component commits to the SDK release pin. See [tools](../tools/sdk/README.md)
for exact Windows and shell commands.

The link-map gate requires app third-party symbols and rejects legacy lvgl-ui.
output/lvgl-evidence/<phase> contains images, ELF/map, config, logs, source
patches/untracked archives and hashes. No build tool flashes hardware.
