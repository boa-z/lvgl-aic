#!/usr/bin/env bash
# SPDX-License-Identifier: Apache-2.0
# Build a D50T-2-Lite platform-only Gate 1, MPP or GE2D test image.
set -euo pipefail

ROOT="${LVGL_AIC_SDK_ROOT:-$(cd "$(dirname "${BASH_SOURCE[0]}")/../../../../.." && pwd)}"
export LVGL_AIC_SDK_ROOT="$ROOT"
[[ -f "$ROOT/SConstruct" ]] || { echo "Invalid SDK root: $ROOT" >&2; exit 2; }
cd "$ROOT"

JOBS="${JOBS:-$(getconf _NPROCESSORS_ONLN 2>/dev/null || echo 4)}"
BOOTLOADER_DEFCONFIG="d13x_d50t-2-lite_baremetal_bootloader_defconfig"
APP_DEFCONFIG="d13x_d50t-2-lite_rt-thread_lvgl-aic-smoke_defconfig"
PHASE="${PHASE:-gate1}"
case "$PHASE" in
    gate1) ;;
    mpp)
        APP_DEFCONFIG="d13x_d50t-2-lite_rt-thread_lvgl-aic-mpp_defconfig"
        python3 packages/custom/lvgl-aic/tools/sdk/stage_assets.py
        ;;
    ge2d)
        # The GE2D profile is the MPP profile plus the draw unit, so the MPP
        # regression is exercised on the same image.
        APP_DEFCONFIG="d13x_d50t-2-lite_rt-thread_lvgl-aic-ge2d_defconfig"
        python3 packages/custom/lvgl-aic/tools/sdk/stage_assets.py
        ;;
    *) echo "PHASE must be gate1, mpp or ge2d" >&2; exit 2 ;;
esac
APP_OUTPUT="output/${APP_DEFCONFIG%_defconfig}"

scons "--apply-def=$BOOTLOADER_DEFCONFIG"
scons -j"$JOBS"
cp output/d13x_d50t-2-lite_baremetal_bootloader/images/d13x.bin \
   target/d13x/d50t-2-lite/pack/bootloader.bin

scons "--apply-def=$APP_DEFCONFIG"
scons -n -j"$JOBS"
scons -j"$JOBS"
CHECK_ARGS=()
[[ "${ALLOW_COMPONENT_DIRTY:-0}" == 1 ]] && CHECK_ARGS+=(--allow-component-dirty)
python3 packages/custom/lvgl-aic/tools/sdk/check_integration.py "${CHECK_ARGS[@]}" \
    --root . \
    --map "$APP_OUTPUT/images/d13x.map" --phase "$PHASE"
python3 packages/custom/lvgl-aic/tools/sdk/verify_image.py "$APP_OUTPUT/images" \
    output/d13x_d50t-2-lite_baremetal_bootloader/images "$PHASE"
