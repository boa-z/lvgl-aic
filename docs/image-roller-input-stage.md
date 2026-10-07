# Image roller pointer ownership and continuous snap

This stage repairs actual input behavior in the existing application-owned SDK
image roller. It preserves stable IDs and the public API. SDK and upstream LVGL
source trees are unchanged; uniform-size carousel geometry remains required.

## Corrected behavior

1. A click on the already-centered item, or another no-op selection request,
   may not emit SCROLL_END. Its old target previously survived until the next
   pointer drag ended and pulled the carousel back. A real pointer SCROLL_BEGIN
   now supersedes that pending selection. Animated selection begins retain their
   target. The original pointer regression ends on ID 2 instead of advancing.
2. Loop reordering previously used a public bounded scroll-to call, canceling
   native snap/selection animation during its execution. One reproduced drag
   settled with the selected item's center at x=122 instead of x=150. The loop
   now shifts raw scroll coordinates and the matching native animation's
   start/end/current values by the same origin change. Timing and the final
   target are preserved; unrelated animations on the owner are left intact.
3. Synchronous selection can emit VALUE_CHANGED whose callback deletes the
   roller. Selection now returns immediately after invoking native scrolling,
   without traversing the freed owner's next child. A wrapped child lookup
   detects that forbidden post-delete access even if freed memory remains readable.

The native animation representation and raw scrolling are private LVGL
interfaces, imported only through the existing compatibility header and recorded
in [compatibility](compatibility.md). This is not a claim of portable behavior
across unverified upstream versions or arbitrary mutations from layout callbacks.

## Meaningful manual page

The previous four tiny symbol images all fit inside the test viewport. The page
therefore could not demonstrate the advertised loop drag on the board. It now
uses four equal-width colored hit areas with a 12-pixel gap, exceeding the
viewport width, and initially centers the second item. The symbols remain native
LVGL content. The swipe controls and page navigation remain available.

The manual-page host contract clicks the centered item and then drags across the
carousel in each of three create/delete cycles. It asserts overflow and a changed
stable active ID. The 800x480 host preview was rendered and visually checked; its local artifact is
`output/roller-input-preview.png`. The standalone input contract uses actual pointer dispatch,
LVGL timers and exact rendered RGB565 interior samples.

## Verification

- 48 drags after no-op pointer / synchronous API / animated API selection, across
  horizontal/vertical, loop on/off and both drag directions.
- Four drags taking over an in-flight animated selection, across directions and
  loop modes.
- 40 consecutive loop drags, checking centered active IDs and an unrelated owner
  animation that must still finish at its original value.
- 80 forward/backward explicit selections over all IDs with animation on/off,
  both directions and loop modes. All 172 input/selection scenes sample exact
  resource colors at the centered selected image.
- Eight VALUE_CHANGED callback deletions across directions, loop and animation
  modes; the post-delete child traversal oracle remains silent after correction.
- Pre-correction logs: `output/roller-input-before.log` (stale target),
  `output/roller-snap-before.log` (off-center snap), and
  `output/roller-delete-before.log` (post-delete traversal).
- Earlier geometry/lifetime contracts still cover gap compensation, child
  removal, stable IDs and deletion during animation.

Final combined FreeType/vector/SVG/Lottie host configuration with manual previews
passes **89/89** (45.35 s); the disabled baseline passes **81/81** (26.66 s).
Logs are `output/roller-input-{build,tests,baseline-build,baseline-tests}.log`.
The complete GE/fonts/widgets/media/SPI/SVG/Lottie D13x firmware profile at
rotation 90 passes boot/app build, final-link/static, packaged-image and manifest
checks. Thirty artifact hashes and three clean source pins were independently
verified. New manual-page instructions are present in allocated ELF `.rodata`;
nine image payload CRCs and 41 packaged fixture/provenance hashes pass. Exact
identities are in [validation](validation.md); build log:
`output/roller-input-firmware.log`. Physical validation is **NOT_RUN**. For consolidated
board validation, open the SDK widgets page, tap the centered carousel icon,
then drag repeatedly in both directions. Check that selection follows the drag,
settles centered and continues smoothly across wrap boundaries. Repeat while
navigating away and back; retain the full serial log. Host pixels and firmware
linkage do not establish board touch feel, GE/cache behavior or rendering speed.
