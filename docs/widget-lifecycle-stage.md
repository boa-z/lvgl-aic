# SDK widget child ownership and loop continuity

This increment corrects two reproduced failures in the application-owned SDK
widget adaptations. It does not add GE operations or claim hardware acceptance.
SDK and upstream LVGL source trees remain unchanged.

## Swipe child ownership

Previously, moving a swipe image to another parent left the old widget reporting
four registered children. Its transition could still reposition that image, and
its event callback retained the old owner pointer after that owner was deleted.
The original contract fails at the expected three-child count after reparenting
(`output/swipe-ownership-before.log`).

The widget now checks its actual child list on CHILD_CHANGED. This detail matters:
LVGL 9.6 notifies the original parent before updating the moved image's parent
pointer. Removed children share the same cleanup as deleted children: cancel the
owner's animation, invalidate its transition generation, remove only the owner's
callback, release its source-pair array and unregister its ID. Application event
callbacks and the image's current borrowed source remain intact. An available ID
may be reused for a new registered child. Moving an image back or into a second
swipe does not automatically register it; source content remains application-owned.

Reparenting in SCROLL_BEGIN or SCROLL_END can replace the removed child and start
a new transition. Only that replacement emits completion; the canceled original
cannot finish it. Ordinary four-slot next/previous and source cycling retain the
SDK API and semantics. This relaxes the port's previous no-reparent restriction;
it is a lifecycle correction, not a claim that the SDK promised automatic adoption.

## Image roller loop geometry

With a seven-pixel flex gap, wrapping the last image to the front shifted every
retained image by seven pixels. The equivalent opposite-end and vertical paths
also omitted the gap from scroll compensation. The original implementation fails
the screen-coordinate oracle (`output/roller-gap-before.log`).

The loop step now includes the column gap for horizontal flow and row gap for
vertical flow. The documented uniform-image-size layout remains required. No
claim is made for arbitrary mixed sizes, reverse flow or floating layout items.
Tests explicitly enter overscroll with the public unbounded scroll API and assert
that edge reordering occurred; a bounded scroll request would not exercise wrap.

## Host validation

- Combined FreeType/SVG/vector/Lottie profile: **88/88 PASS**, 22.45 seconds.
- Disabled baseline profile: **80/80 PASS**, 20.88 seconds.
- New ownership contract: five complete LVGL lifetimes, 80 detach cases across all
  slots and animation/deletion order, 20 event-boundary replacements, transfer to
  another swipe and return without adoption, and 160 real pointer clicks.
- Clicks cover both navigation slots and both resource-cycling slots through LVGL
  input dispatch and timers. There are 240 exact RGB565 interior pixel checks
  against the selected resource colors. This is host software-rendering evidence.
- All tracked LVGL heap bytes return to zero after each complete teardown, and no
  animations remain. Tracking uses malloc/realloc/free core wrappers; this does
  not assert behavior for arbitrary invalid user callbacks or allocator failures.
- Loop tests cover both directions, both ends and gaps 0/7/19: 12 wraps and 48
  retained-child screen positions, with stable legacy selection/lifetime tests.

Logs: component `output/widget-lifecycle-{build,tests,baseline-build,baseline-tests}.log`.
Full combined D13x boot/app build, final-link/static checks, packaged image checks
and manifest **PASS**. Thirty file hashes and all three clean source pins were
independently verified. The final allocated map includes the new swipe class
callback from the component object. The image contains nine verified payload
CRCs and 41 packaged MPP fixture/provenance hashes. Exact build identities and
hashes are in [validation](validation.md). Physical validation is **NOT_RUN**. At the consolidated board session, exercise the SDK
widgets page in both directions, tap both navigation and resource-cycle images,
and check loop continuity with a nonzero image gap. Board memory/timing and touch
feel require separate observations; host PASS does not establish them.
