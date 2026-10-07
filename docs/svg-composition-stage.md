# SVG image composition and effects

SVG now uses an isolated vector-only ARGB layer whose buffer is explicitly
marked premultiplied. The ordinary LVGL image/layer composition path applies
image opacity, recolor, rounded clipping, bitmap masks, color keys, blend mode
and tiling after the SVG's overlapping shapes have composed. Other custom
decoders retain their own callbacks. This also preserves the straight-alpha
representation of destination layers containing earlier non-vector drawing.

For non-tiled images, vector rasterization keeps the original transform and
destination resolution. Allocation is clipped to the visible destination area.
The final layer blend has identity geometry, so neither pivot/scale/rotation nor
image effects are applied twice. Parent opacity and existing image-style opacity
continue through LVGL's normal descriptor initialization and layer composition.

A tiled SVG is rasterized once at its intrinsic dimensions, then follows the
native bitmap tiling path (including its sampling and transform semantics).
Color keys compare rasterized colors, not SVG source color strings; vector
rounding/overlap can leave one-level residual channels, so an exact source
literal is not necessarily an exact pixel key.

This adds a temporary 32-bit image buffer for each live SVG composition, even
without explicit image effects, because ThorVG output is premultiplied while
LVGL's ordinary ARGB destinations use straight alpha. Visible-area clipping,
checked size arithmetic, the existing draw-buffer allocator and configured
LVGL layer memory limit apply. On checked buffer-allocation failure the new
layer is deleted and that image is skipped with a warning, leaving prior
destination pixels intact. This is not a complete parser/renderer heap quota
or recovery from all native internal allocation assertions.

The shared software image correction also fixes premultiplied layer masking:
A8/L8 coverage scales RGB and alpha together. Straight-alpha layers still mask
alpha alone. Both explicit premultiplied format and ordinary ARGB plus the flag
are covered. Corrections remain build-local and source-fingerprinted.

## Evidence

The old path ignored image opacity: an independent sample expected RGB
136/16/24 and obtained 254/0/0. The corrected native-renderer contract runs nine
scenes through 20 composition lifetimes, checks image opacity, overlapping
shapes, partially transparent content, recolor, radius, zero/partial masks,
color-key ranges, repeated tiles and transformed composition. It verifies
layer-list and allocation-accounting cleanup after every draw, a forced buffer
allocation failure, straight-alpha destination pixels and actual parent/image
opacity layers. Analytic sample tolerance is four channel levels.

A separate native mask regression compares straight, explicit-premultiplied and
flagged-premultiplied layers, with centered masks at 0/64/128/255 coverage and
independent blend arithmetic (tolerance two). The existing 48-case transform
oracle and FILE/VARIABLE regressions remain green.

Full combined host **79/79 PASS**; disabled baseline **74/74 PASS**.
Combined D13x firmware passes boot/app compilation, final-link/live-symbol,
image and clean-source manifest checks; [validation](validation.md) records
exact pins and the independently checked image hash. Blend modes
and transformed tile/mask combinations delegate to native composition but do
not yet have exhaustive pixel coverage. The later [document stage](svg-documents-stage.md)
corrects tested group/use transforms, explicit solid paint precedence, missing
references and stroke-dash lifetime. Broader SVG document semantics, external
assets/fonts, parser/renderer memory limits and physical performance remain
open. Hardware **NOT_RUN**; this does not add GE vector rasterization.
