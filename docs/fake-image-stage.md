# SDK .fake pseudo-images

The SDK path L:/<width>x<height>_<blend>_<AARRGGBB>.fake describes a
solid fill, not a media frame. This port supports the canonical syntax with
positive dimensions up to 4096 and at most 8M pixels, blend 0 or 1, exactly
eight hexadecimal color digits and an exact lowercase suffix.

Requires the component MPP decoder and GE draw unit configuration. The
application's existing L filesystem drive is adapted during decoder init.
LVGL 9.6 opens FILE sources before querying decoder info; synthetic readonly
zero-length handles allow this query without creating files. Normal handles
delegate to the original callbacks. Decoder deinit restores those callbacks
and refuses to run while a synthetic handle remains open. Other drive letters
are not adapted. If L is absent, ordinary resource decoding still works.

The draw unit follows the SDK transformed bounding-rectangle fill semantics.
blend=0 replaces the encoded ARGB value, including zero alpha; blend=1 uses
the encoded alpha for source-over. The executor does not apply image recolor,
tiling or global opacity to this SDK pseudo-fill. Bounds are clipped to the
task clip and layer buffer. Scale is bounded to 1/16..16 and pivot coordinates
to -4096..4096; skew and invalid geometry fail explicitly.

GE fill is used when the destination is accessible and the operation is
supported. Otherwise the CPU performs the same replacement or LVGL solid
blend on RGB565/RGB888/XRGB8888/ARGB8888. This handler remains selected if GE
open fails. Partial blends on ARGB8888 stay on CPU. A GE submission error
fails the task and never replays pixels through software.

Host validation: 17/17 PASS. Coverage includes malformed/truncated paths,
all 256 replacement alpha values on four GE formats, real LVGL metadata
lookup, synthetic handle/deinit ownership, real-file forwarding and callback
restoration, clipped CPU pixels/row guards, zero-alpha replacement, partial
ARGB blending, orthogonal bounds and each GE submission failure stage.
GE commands are mocked; CPU pixels and LVGL decoder/filesystem code are real.

Board probes are integrated into the fill suite: encoded alpha 0/128/255,
90 replacement pixels each, all outside pixels and row padding checked.
Target build and physical board execution: pending for this stage.
This feature does not implement YUV/media-frame ownership or media widgets.
