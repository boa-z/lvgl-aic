# Native dynamic widget verification stage

The SDK-era native widget surface also includes animated images, image buttons,
spinners, scales, spans and windows. These already exist in the pinned LVGL 9.6;
this stage adds executable integration coverage and a unified board test page,
not replacement widget implementations or a claim that they were absent.

## Manual page

`-WithWidgets` appends **Native dynamic widgets** after the existing pages.
The existing header and page order remain stable. The page adapts to 800x480
and 480x800 displays and consumes immutable RGB565 arrays, without files or
peripheral configuration. Disabled widget builds omit the page.

- Animated blue/amber/green frames and a spinner exercise native timers.
- Pause / resume freezes both animations. Leaving the page stops animations;
  returning preserves the explicit pause state and selected value.
- An image button changes color on press/check and steps the value through
  0/25/50/75/100. The round scale needle and styled span update together.
- Open window creates a scrollable native window. Its content intercepts input
  over underlying controls. Closing or leaving the page removes it; the value
  persists. The window is page-local, so the shared navigation remains usable.
- Full teardown deletes native objects, animations and page state. `lv_win` is
  deliberately exercised for compatibility; new application UI should prefer
  the upstream flex-header/content replacement for this deprecated widget.

All helper state belongs to this optional test page. Production applications
keep native widget ownership. No SDK, upstream LVGL or product sources change.

## Host and target gates

The dedicated contract uses real LVGL timers, rendering and pointer hit testing
in both orientations across six init/deinit cycles. It checks every animation
frame, spinner pixels, a stable paused render, needle and image-button pixels,
span values, 120 window open/scroll/close interactions, input interception,
page hide/re-entry, child counts and animation teardown. The existing manual
page contract traverses the additional page using its normal Next/Prev buttons.
Screenshots are software layout evidence, not GE or panel acceptance.

The firmware static gate requires native creation APIs and both page helpers in
the final live link map. Physical acceptance remains **NOT_RUN**: visit the page,
check the sequence and spinner, pause/resume, step through values, scroll/close
the window at least 20 times, and navigate away/back in both running and paused
states. Record corruption, stale animations, touch blocking or crashes together
with the image identity and full serial log.

## Completed development gates

Combined host **86/86 PASS** (22.03 s), FreeType/vector/SVG/Lottie-disabled
baseline **79/79 PASS** (31.96 s). A subsequent optional-feature guard/header
cleanup passed the focused platform/manual/native tests (3/3); a strict
`-Wall -Wextra -Werror -fsyntax-only` compile of both page sources and the manual
page harness also passed with widget features absent. Both software previews
were rendered and inspected after increasing scale tick contrast.

Full D13x boot/app, final-link/static, image and manifest gates **PASS**.
All 29 file hashes and three clean source pins were independently verified.
Exact source/image identities are in [validation](validation.md). There was no
SDK/core source change, board flashing or physical execution in this stage.
